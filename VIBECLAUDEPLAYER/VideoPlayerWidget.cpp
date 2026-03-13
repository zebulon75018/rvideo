#include "VideoPlayerWidget.h"
#include "VideoDecoder.h"

#include <QPainter>
#include <QLinearGradient>
#include <QResizeEvent>

#include <cmath>
#include <algorithm>

// ════════════════════════════════════════════════════════════════════════════

VideoPlayerWidget::VideoPlayerWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setStyleSheet("background: #000;");
}

// ════════════════════════════════════════════════════════════════════════════
//  Public API
// ════════════════════════════════════════════════════════════════════════════

void VideoPlayerWidget::setVideoDecoder(VideoDecoder *dec)
{
    m_decoder      = dec;
    m_lastPos      = -999.0;
    m_currentFrame = QImage();
    computeFrameRect();
    update();
}

void VideoPlayerWidget::clear()
{
    m_decoder      = nullptr;
    m_currentFrame = QImage();
    m_lastPos      = -999.0;
    m_infoText.clear();
    m_frameRect    = QRect();
    update();
}

void VideoPlayerWidget::setInfoText(const QString &text)
{
    m_infoText = text;
    update();
}

void VideoPlayerWidget::setPosition(double seconds)
{
    if (!m_decoder || !m_decoder->hasVideo()) return;
    if (std::abs(seconds - m_lastPos) < MIN_SEEK_DELTA) return;
    fetchFrame(seconds);
}

// ════════════════════════════════════════════════════════════════════════════
//  Private
// ════════════════════════════════════════════════════════════════════════════

void VideoPlayerWidget::fetchFrame(double seconds)
{
    QImage img = m_decoder->frameAt(seconds);
    if (img.isNull()) return;

    m_currentFrame = std::move(img);
    m_lastPos      = seconds;
    computeFrameRect();
    update();
}

void VideoPlayerWidget::computeFrameRect()
{
    if (m_currentFrame.isNull() || width() == 0 || height() == 0)
    {
        m_frameRect = rect();
        return;
    }

    double imgAR = static_cast<double>(m_currentFrame.width())
                 / static_cast<double>(m_currentFrame.height());
    double wgtAR = static_cast<double>(width())
                 / static_cast<double>(height());

    int fw, fh;
    if (imgAR >= wgtAR)
    {
        fw = width();
        fh = static_cast<int>(std::round(width() / imgAR));
    }
    else
    {
        fh = height();
        fw = static_cast<int>(std::round(height() * imgAR));
    }

    m_frameRect = QRect((width()  - fw) / 2,
                        (height() - fh) / 2,
                        fw, fh);
}

// ════════════════════════════════════════════════════════════════════════════
//  Qt overrides
// ════════════════════════════════════════════════════════════════════════════

QSize VideoPlayerWidget::sizeHint() const
{
    if (m_decoder && m_decoder->hasVideo() && m_decoder->frameWidth() > 0)
    {
        constexpr int PREFERRED_H = 240;
        int w = m_decoder->frameWidth() * PREFERRED_H
                / std::max(1, m_decoder->frameHeight());
        return { w, PREFERRED_H };
    }
    return { 640, 240 };
}

void VideoPlayerWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    computeFrameRect();
}

void VideoPlayerWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);

    // ── Background ────────────────────────────────────────────────────────────
    p.fillRect(rect(), QColor(8, 8, 8));

    if (m_currentFrame.isNull())
    {
        // ── Placeholder ───────────────────────────────────────────────────────
        p.setPen(QColor(60, 60, 70));
        p.setFont(QFont("Sans", 12));
        p.drawText(rect(), Qt::AlignCenter,
                   m_decoder ? "Chargement de la vidéo…" : "Aucune vidéo");

        p.setPen(QPen(QColor(30, 30, 40), 1, Qt::DashLine));
        p.drawRect(rect().adjusted(4, 4, -4, -4));
        return;
    }

    // ── Video frame (letterboxed) ─────────────────────────────────────────────
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(m_frameRect, m_currentFrame, m_currentFrame.rect());

    // ── Top vignette gradient ─────────────────────────────────────────────────
    {
        int vigH = std::min(40, m_frameRect.height() / 4);
        if (vigH > 0)
        {
            QLinearGradient vign(0, m_frameRect.top(),
                                 0, m_frameRect.top() + vigH);
            vign.setColorAt(0.0, QColor(0, 0, 0, 130));
            vign.setColorAt(1.0, QColor(0, 0, 0, 0));
            p.fillRect(m_frameRect.left(), m_frameRect.top(),
                       m_frameRect.width(), vigH, vign);
        }
    }

    // ── Info text (top-left) ──────────────────────────────────────────────────
    if (!m_infoText.isEmpty())
    {
        p.setPen(QColor(220, 220, 220, 200));
        p.setFont(QFont("Monospace", 8, QFont::Bold));
        p.drawText(m_frameRect.left() + 8,
                   m_frameRect.top()  + 16,
                   m_infoText);
    }

    // ── Timecode (bottom-right) ───────────────────────────────────────────────
    if (m_lastPos >= 0.0)
    {
        int mm  = static_cast<int>(m_lastPos) / 60;
        int ss  = static_cast<int>(m_lastPos) % 60;
        int cs  = static_cast<int>((m_lastPos - std::floor(m_lastPos)) * 100);

        QString ts = (mm > 0)
            ? QString("%1:%2.%3")
                .arg(mm)
                .arg(ss, 2, 10, QChar('0'))
                .arg(cs, 2, 10, QChar('0'))
            : QString("%1.%2s")
                .arg(ss)
                .arg(cs, 2, 10, QChar('0'));

        p.setFont(QFont("Monospace", 9, QFont::Bold));

        // Shadow
        p.setPen(QColor(0, 0, 0, 200));
        p.drawText(m_frameRect.right() - 86,
                   m_frameRect.bottom() - 11, ts);
        // Main text
        p.setPen(QColor(255, 255, 200, 230));
        p.drawText(m_frameRect.right() - 87,
                   m_frameRect.bottom() - 12, ts);
    }
}
