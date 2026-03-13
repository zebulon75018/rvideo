#pragma once

#include <QWidget>
#include <QImage>
#include <QString>

class VideoDecoder;

/**
 * VideoPlayerWidget
 * -----------------
 * Displays a single video frame fetched from a VideoDecoder.
 *
 *  • setPosition(seconds) seeks and repaints the frame
 *  • Aspect-ratio preserving letterbox / pillarbox layout
 *  • "No video" placeholder when empty
 *  • Optional filename overlay (top-left) + timecode (bottom-right)
 */
class VideoPlayerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPlayerWidget(QWidget *parent = nullptr);

    /** Attach a VideoDecoder. Call once after a video file is loaded. */
    void setVideoDecoder(VideoDecoder *dec);

    /** Detach decoder and clear the displayed frame. */
    void clear();

    /** Seek to position (seconds) and refresh the displayed frame. */
    void setPosition(double seconds);

    /** Text shown in the top-left corner of the video frame. */
    void setInfoText(const QString &text);

    QSize sizeHint() const override;

protected:
    void paintEvent  (QPaintEvent  *event) override;
    void resizeEvent (QResizeEvent *event) override;

private:
    /** Fetch the frame at `seconds` from the decoder and store it. */
    void fetchFrame(double seconds);

    /** Recompute the letterbox QRect that fits the current frame in the widget. */
    void computeFrameRect();

    VideoDecoder *m_decoder       = nullptr;
    QImage        m_currentFrame;
    double        m_lastPos       = -999.0;
    QString       m_infoText;
    QRect         m_frameRect;

    // Skip seeks that are closer together than one video frame (~1/30 s)
    static constexpr double MIN_SEEK_DELTA = 1.0 / 30.0;
};
