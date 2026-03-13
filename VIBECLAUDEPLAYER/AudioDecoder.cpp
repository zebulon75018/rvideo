#include "AudioDecoder.h"

#include <QDebug>
#include <QFileInfo>
#include <cstring>
#include <algorithm>

// ── libsndfile ──────────────────────────────────────────────────────────────
#include <sndfile.h>

// ── FFmpeg ──────────────────────────────────────────────────────────────────
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

// ─────────────────────────────────────────────────────────────────────────────

AudioDecoder::AudioDecoder(QObject *parent)
    : QObject(parent)
{}

void AudioDecoder::clear()
{
    m_samples.clear();
    m_sampleRate = 0;
    m_channels   = 0;
    m_numFrames  = 0;
    m_filePath.clear();
}

double AudioDecoder::duration() const
{
    if (m_sampleRate == 0 || m_channels == 0)
        return 0.0;
    return static_cast<double>(m_numFrames) / m_sampleRate;
}

// ── Public entry point ───────────────────────────────────────────────────────

bool AudioDecoder::loadFile(const QString &filePath)
{
    clear();
    m_filePath = filePath;

    const QString ext = QFileInfo(filePath).suffix().toLower();

    bool ok = false;

    // Prefer libsndfile for uncompressed formats it handles natively
    if (ext == "wav" || ext == "aif" || ext == "aiff" ||
        ext == "flac" || ext == "ogg" || ext == "au")
    {
        ok = loadViaSndFile(filePath);
        if (!ok)
            ok = loadViaFFmpeg(filePath); // fallback
    }
    else
    {
        // MP3, MP4, M4A, AAC, MOV, MKV… → FFmpeg
        ok = loadViaFFmpeg(filePath);
    }

    if (!ok)
    {
        clear();
        emit loadFinished(false, QString("Failed to decode: %1").arg(filePath));
        return false;
    }

    emit loadFinished(true, {});
    return true;
}

// ── libsndfile back-end ──────────────────────────────────────────────────────

bool AudioDecoder::loadViaSndFile(const QString &path)
{
    SF_INFO info{};
    SNDFILE *sf = sf_open(path.toUtf8().constData(), SFM_READ, &info);
    if (!sf)
    {
        qWarning() << "libsndfile:" << sf_strerror(nullptr);
        return false;
    }

    m_sampleRate = info.samplerate;
    m_channels   = info.channels;
    m_numFrames  = info.frames;

    const int64_t totalSamples = m_numFrames * m_channels;
    m_samples.resize(static_cast<size_t>(totalSamples));

    constexpr int CHUNK = 65536;
    int64_t read = 0;
    while (read < totalSamples)
    {
        sf_count_t n = sf_read_float(sf, m_samples.data() + read,
                                     std::min<int64_t>(CHUNK, totalSamples - read));
        if (n <= 0) break;
        read += n;

        emit loadProgress(static_cast<int>(100 * read / totalSamples));
    }

    sf_close(sf);

    if (read < totalSamples)
        m_samples.resize(static_cast<size_t>(read));

    qDebug() << "libsndfile loaded:" << path
             << "| sr:" << m_sampleRate
             << "| ch:" << m_channels
             << "| frames:" << m_numFrames;
    return true;
}

// ── FFmpeg back-end ──────────────────────────────────────────────────────────

bool AudioDecoder::loadViaFFmpeg(const QString &path)
{
    AVFormatContext *fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, path.toUtf8().constData(), nullptr, nullptr) < 0)
    {
        qWarning() << "FFmpeg: cannot open" << path;
        return false;
    }

    if (avformat_find_stream_info(fmtCtx, nullptr) < 0)
    {
        avformat_close_input(&fmtCtx);
        qWarning() << "FFmpeg: stream info not found";
        return false;
    }

    // Find best audio stream
    const AVCodec *codec = nullptr;
    int streamIdx = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (streamIdx < 0)
    {
        avformat_close_input(&fmtCtx);
        qWarning() << "FFmpeg: no audio stream";
        return false;
    }

    AVStream *stream = fmtCtx->streams[streamIdx];

    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, stream->codecpar);
    if (avcodec_open2(codecCtx, codec, nullptr) < 0)
    {
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        qWarning() << "FFmpeg: cannot open codec";
        return false;
    }

    // ── Resample to interleaved float32 ─────────────────────────────────────
    SwrContext *swrCtx = nullptr;

#if LIBAVUTIL_VERSION_MAJOR >= 57
    AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
    int outChannels = 2;
    if (codecCtx->ch_layout.nb_channels == 1)
    {
        outLayout  = AV_CHANNEL_LAYOUT_MONO;
        outChannels = 1;
    }
    swr_alloc_set_opts2(&swrCtx,
                        &outLayout,     AV_SAMPLE_FMT_FLT, codecCtx->sample_rate,
                        &codecCtx->ch_layout, codecCtx->sample_fmt, codecCtx->sample_rate,
                        0, nullptr);
#else
    int64_t inLayout  = codecCtx->channel_layout
                        ? codecCtx->channel_layout
                        : av_get_default_channel_layout(codecCtx->channels);
    int outChannels = (codecCtx->channels == 1) ? 1 : 2;
    int64_t outLayout = (outChannels == 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
    swrCtx = swr_alloc_set_opts(nullptr,
                                outLayout,  AV_SAMPLE_FMT_FLT, codecCtx->sample_rate,
                                inLayout,   codecCtx->sample_fmt, codecCtx->sample_rate,
                                0, nullptr);
#endif

    swr_init(swrCtx);

    m_sampleRate = codecCtx->sample_rate;
    m_channels   = outChannels;

    // Estimate total frames from duration
    int64_t estFrames = 0;
    if (fmtCtx->duration != AV_NOPTS_VALUE)
        estFrames = static_cast<int64_t>(
            fmtCtx->duration * m_sampleRate / AV_TIME_BASE);
    if (estFrames > 0)
        m_samples.reserve(static_cast<size_t>(estFrames * outChannels + 4096));

    AVPacket *pkt  = av_packet_alloc();
    AVFrame  *frm  = av_frame_alloc();
    int64_t   totalSamplesSoFar = 0;

    while (av_read_frame(fmtCtx, pkt) >= 0)
    {
        if (pkt->stream_index != streamIdx)
        {
            av_packet_unref(pkt);
            continue;
        }

        if (avcodec_send_packet(codecCtx, pkt) < 0)
        {
            av_packet_unref(pkt);
            continue;
        }

        while (avcodec_receive_frame(codecCtx, frm) == 0)
        {
            int samples = frm->nb_samples;
            std::vector<float> buf(static_cast<size_t>(samples * outChannels));
            uint8_t *outPtr = reinterpret_cast<uint8_t *>(buf.data());

            int converted = swr_convert(swrCtx, &outPtr, samples,
                                        const_cast<const uint8_t **>(frm->extended_data),
                                        samples);
            if (converted > 0)
            {
                m_samples.insert(m_samples.end(),
                                 buf.begin(),
                                 buf.begin() + converted * outChannels);
                totalSamplesSoFar += converted;
            }
            av_frame_unref(frm);

            if (estFrames > 0)
                emit loadProgress(
                    static_cast<int>(100 * totalSamplesSoFar / estFrames));
        }
        av_packet_unref(pkt);
    }

    // Flush
    avcodec_send_packet(codecCtx, nullptr);
    while (avcodec_receive_frame(codecCtx, frm) == 0)
    {
        int samples = frm->nb_samples;
        std::vector<float> buf(static_cast<size_t>(samples * outChannels));
        uint8_t *outPtr = reinterpret_cast<uint8_t *>(buf.data());
        int converted = swr_convert(swrCtx, &outPtr, samples,
                                    const_cast<const uint8_t **>(frm->extended_data),
                                    samples);
        if (converted > 0)
            m_samples.insert(m_samples.end(),
                             buf.begin(),
                             buf.begin() + converted * outChannels);
        av_frame_unref(frm);
    }

    m_numFrames = static_cast<int64_t>(m_samples.size()) / m_channels;

    av_frame_free(&frm);
    av_packet_free(&pkt);
    swr_free(&swrCtx);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmtCtx);

    qDebug() << "FFmpeg loaded:" << path
             << "| sr:" << m_sampleRate
             << "| ch:" << m_channels
             << "| frames:" << m_numFrames;
    return !m_samples.empty();
}
