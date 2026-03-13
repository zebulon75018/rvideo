#include "AudioPlayer.h"

#include <QDebug>
#include <cstring>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent)
{
    PaError err = Pa_Initialize();
    if (err != paNoError)
        qWarning() << "PortAudio init error:" << Pa_GetErrorText(err);
    else
        m_paInited = true;

    // Poll position every 30 ms so the GUI stays smooth
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(30);
    connect(m_pollTimer, &QTimer::timeout, this, [this]()
    {
        const double pos = currentPositionSeconds();
        if (pos != m_lastEmittedPos)
        {
            m_lastEmittedPos = pos;
            emit positionChanged(pos);
        }
        // Detect natural end-of-stream
        if (m_state == State::Playing && m_samples)
        {
            int64_t totalFrames = static_cast<int64_t>(m_samples->size())
                                  / std::max(1, m_channels);
            if (m_currentFrame.load() >= totalFrames)
            {
                m_state.store(State::Stopped);
                m_currentFrame.store(0);
                emit playbackFinished();
                emit stateChanged(State::Stopped);
                m_pollTimer->stop();
            }
        }
    });
}

AudioPlayer::~AudioPlayer()
{
    closeStream();
    if (m_paInited)
        Pa_Terminate();
}

// ── Configuration ────────────────────────────────────────────────────────────

void AudioPlayer::setSamples(const std::vector<float> *samples,
                             int sampleRate,
                             int channels)
{
    stop();
    m_samples    = samples;
    m_sampleRate = sampleRate;
    m_channels   = channels;
    m_currentFrame.store(0);
}

// ── Transport ────────────────────────────────────────────────────────────────

void AudioPlayer::play()
{
    if (!m_samples || m_samples->empty()) return;

    if (m_state == State::Paused)
    {
        // Resume existing stream
        if (m_stream)
        {
            Pa_StartStream(m_stream);
            notifyState(State::Playing);
            m_pollTimer->start();
        }
        return;
    }

    if (m_state == State::Playing) return;

    // Fresh start
    closeStream();
    openStream();
    if (m_stream)
    {
        Pa_StartStream(m_stream);
        notifyState(State::Playing);
        m_pollTimer->start();
    }
}

void AudioPlayer::pause()
{
    if (m_state != State::Playing) return;
    if (m_stream)
        Pa_StopStream(m_stream);
    notifyState(State::Paused);
}

void AudioPlayer::stop()
{
    closeStream();
    m_currentFrame.store(0);
    m_lastEmittedPos = -1.0;
    notifyState(State::Stopped);
    m_pollTimer->stop();
}

void AudioPlayer::seek(double positionSeconds)
{
    if (!m_samples || m_sampleRate == 0) return;

    int64_t totalFrames = static_cast<int64_t>(m_samples->size())
                          / std::max(1, m_channels);
    int64_t frame = static_cast<int64_t>(positionSeconds * m_sampleRate);
    frame = std::clamp(frame, int64_t{0}, totalFrames);
    m_currentFrame.store(frame);
    emit positionChanged(currentPositionSeconds());
}

// ── Queries ──────────────────────────────────────────────────────────────────

double AudioPlayer::currentPositionSeconds() const
{
    if (m_sampleRate == 0) return 0.0;
    return static_cast<double>(m_currentFrame.load()) / m_sampleRate;
}

double AudioPlayer::durationSeconds() const
{
    if (!m_samples || m_sampleRate == 0 || m_channels == 0) return 0.0;
    return static_cast<double>(m_samples->size())
           / (m_sampleRate * m_channels);
}

// ── PortAudio helpers ─────────────────────────────────────────────────────────

void AudioPlayer::openStream()
{
    if (!m_paInited) return;

    PaStreamParameters out{};
    out.device           = Pa_GetDefaultOutputDevice();
    out.channelCount     = m_channels;
    out.sampleFormat     = paFloat32;
    out.suggestedLatency =
        Pa_GetDeviceInfo(out.device)->defaultLowOutputLatency;
    out.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(&m_stream,
                                nullptr, &out,
                                m_sampleRate,
                                paFramesPerBufferUnspecified,
                                paClipOff,
                                &AudioPlayer::paCallbackStatic,
                                this);
    if (err != paNoError)
    {
        qWarning() << "Pa_OpenStream error:" << Pa_GetErrorText(err);
        m_stream = nullptr;
    }
}

void AudioPlayer::closeStream()
{
    if (!m_stream) return;
    Pa_StopStream(m_stream);
    Pa_CloseStream(m_stream);
    m_stream = nullptr;
}

void AudioPlayer::notifyState(State s)
{
    if (m_state.load() != s)
    {
        m_state.store(s);
        emit stateChanged(s);
    }
}

// ── PortAudio callback (audio thread) ────────────────────────────────────────

int AudioPlayer::paCallbackStatic(const void * /*in*/, void *out,
                                  unsigned long frames,
                                  const PaStreamCallbackTimeInfo * /*ti*/,
                                  PaStreamCallbackFlags /*flags*/,
                                  void *userData)
{
    return static_cast<AudioPlayer *>(userData)->paCallback(out, frames);
}

int AudioPlayer::paCallback(void *out, unsigned long frames)
{
    float *dst = static_cast<float *>(out);

    if (!m_samples || m_state.load() != State::Playing)
    {
        std::memset(dst, 0, frames * m_channels * sizeof(float));
        return paContinue;
    }

    int64_t totalSamples = static_cast<int64_t>(m_samples->size());
    int64_t pos          = m_currentFrame.load() * m_channels;
    int64_t needed       = static_cast<int64_t>(frames) * m_channels;

    if (pos >= totalSamples)
    {
        std::memset(dst, 0, frames * m_channels * sizeof(float));
        return paComplete;
    }

    int64_t avail = totalSamples - pos;
    int64_t toCopy = std::min(needed, avail);

    std::memcpy(dst, m_samples->data() + pos, toCopy * sizeof(float));

    if (toCopy < needed)
        std::memset(dst + toCopy, 0, (needed - toCopy) * sizeof(float));

    m_currentFrame.fetch_add(static_cast<int64_t>(frames));
    return paContinue;
}
