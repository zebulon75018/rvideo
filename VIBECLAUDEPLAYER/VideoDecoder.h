#pragma once

#include <QObject>
#include <QImage>
#include <QMutex>
#include <QString>
#include <QList>
#include <QPair>

// OpenCV
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>

/**
 * VideoDecoder
 * ------------
 * OpenCV-based video reader providing:
 *   - on-demand frame seeking  (frameAt)
 *   - async thumbnail strip generation (thumbnailsReady signal)
 *
 * Thread safety:
 *   frameAt() is protected by a QMutex so it can be called from any thread,
 *   but in practice it should be called from the GUI thread only (30 Hz).
 *   generateThumbnailsAsync() runs in a QtConcurrent task.
 */
class VideoDecoder : public QObject
{
    Q_OBJECT

public:
    explicit VideoDecoder(QObject *parent = nullptr);
    ~VideoDecoder() override;

    /** Open a video file.  Returns true if the file has a video stream. */
    bool loadFile(const QString &path);

    /** Release all resources. */
    void clear();

    // ── Queries ───────────────────────────────────────────────────────────────
    bool    hasVideo()   const { return m_hasVideo; }
    double  duration()   const { return m_duration; }   ///< seconds
    double  fps()        const { return m_fps;      }
    int     frameWidth() const { return m_width;    }
    int     frameHeight()const { return m_height;   }
    QString filePath()   const { return m_filePath; }

    /**
     * Seek to `seconds` and return the corresponding frame as a QImage.
     * Returns a null QImage on failure.
     * Thread-safe (mutex-protected).
     */
    QImage frameAt(double seconds);

    /**
     * Start thumbnail generation in a background thread.
     * emits thumbnailsReady() when done.
     * @param count      maximum number of thumbnails
     * @param thumbHeight pixel height for each thumbnail
     */
    void generateThumbnailsAsync(int count = 80, int thumbHeight = 48);

signals:
    /** Emitted from a background thread – connect with Qt::QueuedConnection. */
    void thumbnailsReady(QList<QPair<double, QImage>> thumbnails);

private:
    QImage matToQImage(const cv::Mat &mat) const;

    cv::VideoCapture m_cap;
    mutable QMutex   m_mutex;

    bool    m_hasVideo = false;
    double  m_duration = 0.0;
    double  m_fps      = 0.0;
    int     m_width    = 0;
    int     m_height   = 0;
    QString m_filePath;
};
