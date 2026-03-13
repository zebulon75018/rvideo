#include "VideoDecoder.h"

#include <QMutexLocker>
#include <QDebug>
#include <QtConcurrent/QtConcurrent>

// ════════════════════════════════════════════════════════════════════════════

VideoDecoder::VideoDecoder(QObject *parent)
    : QObject(parent)
{}

VideoDecoder::~VideoDecoder()
{
    clear();
}

// ════════════════════════════════════════════════════════════════════════════
//  Load / clear
// ════════════════════════════════════════════════════════════════════════════

bool VideoDecoder::loadFile(const QString &path)
{
    clear();

    QMutexLocker lk(&m_mutex);

    m_cap.open(path.toStdString());
    if (!m_cap.isOpened())
    {
        qWarning() << "VideoDecoder: cannot open" << path;
        return false;
    }

    m_fps      = m_cap.get(cv::CAP_PROP_FPS);
    m_width    = static_cast<int>(m_cap.get(cv::CAP_PROP_FRAME_WIDTH));
    m_height   = static_cast<int>(m_cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    double fc  = m_cap.get(cv::CAP_PROP_FRAME_COUNT);

    if (m_fps <= 0 || m_width <= 0 || m_height <= 0 || fc <= 0)
    {
        qWarning() << "VideoDecoder: no valid video stream in" << path;
        m_cap.release();
        return false;
    }

    m_duration = fc / m_fps;
    m_hasVideo = true;
    m_filePath = path;

    qDebug() << "VideoDecoder loaded:" << path
             << "| fps:" << m_fps
             << "| size:" << m_width << "x" << m_height
             << "| duration:" << m_duration << "s";
    return true;
}

void VideoDecoder::clear()
{
    QMutexLocker lk(&m_mutex);
    m_cap.release();
    m_hasVideo = false;
    m_duration = m_fps = 0.0;
    m_width    = m_height = 0;
    m_filePath.clear();
}

// ════════════════════════════════════════════════════════════════════════════
//  Frame seeking
// ════════════════════════════════════════════════════════════════════════════

QImage VideoDecoder::frameAt(double seconds)
{
    QMutexLocker lk(&m_mutex);

    if (!m_hasVideo || !m_cap.isOpened())
        return {};

    seconds = std::clamp(seconds, 0.0, m_duration);

    // Seek by milliseconds – more reliable across codecs than frame index
    m_cap.set(cv::CAP_PROP_POS_MSEC, seconds * 1000.0);

    cv::Mat frame;
    if (!m_cap.read(frame) || frame.empty())
    {
        qWarning() << "VideoDecoder::frameAt: read failed at" << seconds << "s";
        return {};
    }

    return matToQImage(frame);
}

// ════════════════════════════════════════════════════════════════════════════
//  Thumbnail generation (background thread)
// ════════════════════════════════════════════════════════════════════════════

void VideoDecoder::generateThumbnailsAsync(int count, int thumbHeight)
{
    if (!m_hasVideo) return;

    // Capture state needed by the lambda (not the whole `this` for thread safety)
    QString path     = m_filePath;
    double  duration = m_duration;
    double  fps      = m_fps;

    // Run in a detached QtConcurrent task
    QFuture<void> f = QtConcurrent::run([this, path, duration, fps, count, thumbHeight]()
    {
        // Open a SEPARATE VideoCapture so we don't fight with the main one
        cv::VideoCapture thumbCap(path.toStdString());
        if (!thumbCap.isOpened()) return;

        int    srcW    = static_cast<int>(thumbCap.get(cv::CAP_PROP_FRAME_WIDTH));
        int    srcH    = static_cast<int>(thumbCap.get(cv::CAP_PROP_FRAME_HEIGHT));
        if (srcW <= 0 || srcH <= 0) return;

        int thumbW = (thumbHeight * srcW) / std::max(1, srcH);

        // Clamp count to a sensible maximum
        int actual = std::min(count, static_cast<int>(duration > 0 ? duration : 1) * 4);
        actual     = std::max(actual, 1);

        QList<QPair<double, QImage>> result;
        result.reserve(actual);

        for (int i = 0; i < actual; ++i)
        {
            double t = (actual == 1)
                       ? 0.0
                       : duration * i / (actual - 1);

            thumbCap.set(cv::CAP_PROP_POS_MSEC, t * 1000.0);

            cv::Mat frame;
            if (!thumbCap.read(frame) || frame.empty()) continue;

            // Resize to thumbnail dimensions
            cv::Mat thumb;
            cv::resize(frame, thumb, cv::Size(thumbW, thumbHeight),
                       0, 0, cv::INTER_LINEAR);

            result.append({ t, matToQImage(thumb) });
        }

        thumbCap.release();
        emit thumbnailsReady(result);
    });
    Q_UNUSED(f)
}

// ════════════════════════════════════════════════════════════════════════════
//  Helper: cv::Mat → QImage (BGR → RGB)
// ════════════════════════════════════════════════════════════════════════════

QImage VideoDecoder::matToQImage(const cv::Mat &mat) const
{
    cv::Mat rgb;
    if (mat.channels() == 3)
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    else if (mat.channels() == 4)
        cv::cvtColor(mat, rgb, cv::COLOR_BGRA2RGBA);
    else
        rgb = mat.clone();

    // Deep copy so the QImage owns its buffer (safe after mat goes out of scope)
    return QImage(rgb.data,
                  rgb.cols, rgb.rows,
                  static_cast<int>(rgb.step),
                  rgb.channels() == 4 ? QImage::Format_RGBA8888
                                      : QImage::Format_RGB888)
           .copy();
}
