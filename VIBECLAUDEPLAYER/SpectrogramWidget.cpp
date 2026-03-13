#include "SpectrogramWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QLinearGradient>
#include <QDebug>

#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ════════════════════════════════════════════════════════════════════════════
//  Constructor / Destructor
// ════════════════════════════════════════════════════════════════════════════

SpectrogramWidget::SpectrogramWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(400, 200);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_fftwIn   = fftwf_alloc_real(FFT_SIZE);
    m_fftwOut  = fftwf_alloc_complex(FREQ_BINS);
    m_fftwPlan = fftwf_plan_dft_r2c_1d(FFT_SIZE, m_fftwIn, m_fftwOut, FFTW_ESTIMATE);
}

SpectrogramWidget::~SpectrogramWidget()
{
    fftwf_destroy_plan(m_fftwPlan);
    fftwf_free(m_fftwIn);
    fftwf_free(m_fftwOut);
}

// ════════════════════════════════════════════════════════════════════════════
//  Public API
// ════════════════════════════════════════════════════════════════════════════

void SpectrogramWidget::clear()
{
    m_mono.clear();
    m_spec.clear();
    m_rmsEnv.clear();
    m_spectrogramImage = QImage();
    m_smoothSpec.clear();
    m_peakHold.clear();
    m_peakTimer.clear();
    m_playheadPos  = 0.0;
    m_sampleRate   = 0;
    m_duration     = 0.0;
    m_numCols = m_numRows = 0;
    clearThumbnails();
    update();
}

void SpectrogramWidget::clearThumbnails()
{
    m_thumbnails.clear();
    m_hasThumbs = false;
    update();
}

void SpectrogramWidget::setThumbnails(const QList<QPair<double, QImage>> &thumbs)
{
    m_thumbnails = thumbs;
    m_hasThumbs  = !thumbs.isEmpty();
    update();
}

void SpectrogramWidget::setDisplayMode(DisplayMode m)
{
    if (m_mode != m)
    {
        m_mode = m;
        if (m == DisplayMode::Frequency)
            refreshFreqBuffers(m_playheadPos);
        update();
    }
}

void SpectrogramWidget::setLogFrequency(bool on)
{
    if (m_logFreq != on) { m_logFreq = on; update(); }
}

void SpectrogramWidget::setAudioData(const std::vector<float> &samples,
                                     int sampleRate, int channels)
{
    m_sampleRate  = sampleRate;
    m_playheadPos = 0.0;

    // Mono mix
    int64_t numFrames = static_cast<int64_t>(samples.size()) / std::max(1, channels);
    m_mono.resize(static_cast<size_t>(numFrames));
    if (channels == 1)
    {
        m_mono.assign(samples.begin(), samples.end());
    }
    else
    {
        for (int64_t f = 0; f < numFrames; ++f)
        {
            float s = 0.f;
            for (int c = 0; c < channels; ++c)
                s += samples[static_cast<size_t>(f * channels + c)];
            m_mono[static_cast<size_t>(f)] = s / channels;
        }
    }
    m_duration = static_cast<double>(numFrames) / sampleRate;

    m_smoothSpec.assign(FREQ_BINS, -120.f);
    m_peakHold.assign  (FREQ_BINS, -120.f);
    m_peakTimer.assign (FREQ_BINS,  0);

    computeSpectrogram();
    buildSpectrogramImage();
    buildRmsEnvelope();
    refreshFreqBuffers(0.0);
    update();
}

void SpectrogramWidget::setPlaybackPosition(double seconds)
{
    m_playheadPos = std::clamp(seconds, 0.0, m_duration);
    if (!m_spec.empty())
        refreshFreqBuffers(m_playheadPos);
    update();
}

// ════════════════════════════════════════════════════════════════════════════
//  Layout
// ════════════════════════════════════════════════════════════════════════════

int SpectrogramWidget::timelineHeight() const
{
    return m_hasThumbs ? (THUMB_H + AXIS_H) : AXIS_H;
}

// ════════════════════════════════════════════════════════════════════════════
//  DSP
// ════════════════════════════════════════════════════════════════════════════

float SpectrogramWidget::hann(int n, int N)
{
    return 0.5f * (1.f - std::cos(2.f * float(M_PI) * n / (N - 1)));
}

void SpectrogramWidget::computeSpectrogram()
{
    m_spec.clear();
    if (m_mono.empty() || m_sampleRate == 0) return;

    const int64_t total = static_cast<int64_t>(m_mono.size());
    m_numRows = FREQ_BINS;

    std::vector<float> win(FFT_SIZE);
    for (int i = 0; i < FFT_SIZE; ++i)
        win[i] = hann(i, FFT_SIZE);

    m_specMinDb =  0.f;
    m_specMaxDb = -200.f;

    for (int64_t start = 0; start + FFT_SIZE <= total; start += HOP_SIZE)
    {
        for (int i = 0; i < FFT_SIZE; ++i)
            m_fftwIn[i] = m_mono[static_cast<size_t>(start + i)] * win[i];

        fftwf_execute(m_fftwPlan);

        std::vector<float> col(FREQ_BINS);
        for (int k = 0; k < FREQ_BINS; ++k)
        {
            float re  = m_fftwOut[k][0];
            float im  = m_fftwOut[k][1];
            float mag = std::sqrt(re*re + im*im) / FFT_SIZE;
            float db  = (mag > 1e-10f) ? 20.f * std::log10(mag) : -120.f;
            col[k] = db;
            m_specMinDb = std::min(m_specMinDb, db);
            m_specMaxDb = std::max(m_specMaxDb, db);
        }
        m_spec.push_back(std::move(col));
    }

    m_numCols = static_cast<int>(m_spec.size());
    if (m_specMaxDb <= m_specMinDb) m_specMaxDb = m_specMinDb + 1.f;
}

void SpectrogramWidget::buildRmsEnvelope()
{
    m_rmsEnv.clear();
    if (m_mono.empty()) return;
    constexpr int BLOCK = 512;
    int64_t total = static_cast<int64_t>(m_mono.size());
    int64_t nb    = total / BLOCK + 1;
    m_rmsEnv.resize(static_cast<size_t>(nb));
    for (int64_t b = 0; b < nb; ++b)
    {
        int64_t s0 = b * BLOCK, s1 = std::min(s0 + BLOCK, total);
        double sum = 0;
        for (int64_t s = s0; s < s1; ++s) { double v = m_mono[size_t(s)]; sum += v*v; }
        m_rmsEnv[size_t(b)] = float(std::sqrt(sum / (s1 - s0)));
    }
}

QRgb SpectrogramWidget::dbToColor(float db, float minDb, float maxDb)
{
    float t = std::clamp((db - minDb) / (maxDb - minDb), 0.f, 1.f);
    struct Stop { float pos; uint8_t r,g,b; };
    static constexpr Stop s[] = {
        {0.00f,0,0,4},{0.13f,27,12,65},{0.25f,71,15,100},{0.38f,122,23,84},
        {0.50f,175,54,34},{0.63f,218,112,31},{0.75f,252,180,25},
        {0.88f,252,230,100},{1.00f,252,252,164}
    };
    int i = 0;
    while (i < 7 && t > s[i+1].pos) ++i;
    float f = (t - s[i].pos) / (s[i+1].pos - s[i].pos);
    auto L = [&](uint8_t a, uint8_t b){ return int(a + f*(b-a)); };
    return qRgb(L(s[i].r,s[i+1].r), L(s[i].g,s[i+1].g), L(s[i].b,s[i+1].b));
}

void SpectrogramWidget::buildSpectrogramImage()
{
    if (m_spec.empty()) { m_spectrogramImage = QImage(); return; }
    m_spectrogramImage = QImage(m_numCols, m_numRows, QImage::Format_RGB32);
    for (int col = 0; col < m_numCols; ++col)
        for (int bin = 0; bin < m_numRows; ++bin)
            m_spectrogramImage.setPixel(col, m_numRows-1-bin,
                dbToColor(m_spec[col][bin], m_specMinDb, m_specMaxDb));
}

void SpectrogramWidget::refreshFreqBuffers(double seconds)
{
    if (m_spec.empty() || m_smoothSpec.empty()) return;
    int col = timeToColumn(seconds);
    if (col < 0 || col >= m_numCols) return;

    constexpr float ALPHA      = 0.45f;
    constexpr int   HOLD_FRAMES = 20;
    constexpr float DECAY_DB   = 1.2f;

    const auto &frame = m_spec[col];
    for (int b = 0; b < FREQ_BINS; ++b)
    {
        float v = frame[b];
        m_smoothSpec[b] = ALPHA * v + (1.f - ALPHA) * m_smoothSpec[b];
        if (v >= m_peakHold[b])
        { m_peakHold[b] = v; m_peakTimer[b] = HOLD_FRAMES; }
        else
        {
            if (m_peakTimer[b] > 0) --m_peakTimer[b];
            else m_peakHold[b] -= DECAY_DB;
        }
        m_peakHold[b] = std::max(m_peakHold[b], -120.f);
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  Coordinates
// ════════════════════════════════════════════════════════════════════════════

int SpectrogramWidget::timeToColumn(double seconds) const
{
    if (m_duration <= 0.0 || m_numCols == 0) return 0;
    return std::clamp(int(seconds / m_duration * (m_numCols-1)), 0, m_numCols-1);
}

int SpectrogramWidget::timeToPixelX(double t) const
{
    if (m_duration <= 0.0) return 0;
    return int(t / m_duration * width());
}

double SpectrogramWidget::pixelXToTime(int x) const
{
    if (width() == 0 || m_duration <= 0.0) return 0.0;
    return std::clamp(double(x) / width() * m_duration, 0.0, m_duration);
}

int SpectrogramWidget::freqToPixelX(double hz, const QRect &r) const
{
    if (m_sampleRate == 0 || r.width() <= 0) return r.left();
    double nyq = m_sampleRate / 2.0, norm;
    if (m_logFreq)
    {
        double lo = std::log10(20.0), hi = std::log10(nyq);
        norm = (std::log10(std::max(hz, 20.0)) - lo) / (hi - lo);
    }
    else norm = hz / nyq;
    return r.left() + int(std::clamp(norm,0.0,1.0) * r.width());
}

double SpectrogramWidget::pixelXToFreq(int x, const QRect &r) const
{
    if (r.width() == 0 || m_sampleRate == 0) return 0.0;
    double norm = double(x - r.left()) / r.width();
    double nyq  = m_sampleRate / 2.0;
    if (m_logFreq)
    {
        double lo = std::log10(20.0), hi = std::log10(nyq);
        return std::pow(10.0, lo + norm * (hi - lo));
    }
    return norm * nyq;
}

// ════════════════════════════════════════════════════════════════════════════
//  Mouse
// ════════════════════════════════════════════════════════════════════════════

void SpectrogramWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && m_duration > 0.0
        && m_mode != DisplayMode::Frequency)
    {
        m_dragging    = true;
        m_playheadPos = pixelXToTime(e->pos().x());
        update();
        emit seekRequested(m_playheadPos);
    }
}

void SpectrogramWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (m_mode != DisplayMode::Frequency)
    {
        if (m_dragging && (e->buttons() & Qt::LeftButton))
        {
            m_playheadPos = pixelXToTime(e->pos().x());
            update();
            emit seekRequested(m_playheadPos);
        }
        int phX = timeToPixelX(m_playheadPos);
        setCursor(std::abs(e->pos().x() - phX) <= PLAYHEAD_HIT
                  ? Qt::SizeHorCursor : Qt::ArrowCursor);
    }
    else setCursor(Qt::ArrowCursor);
}

void SpectrogramWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) m_dragging = false;
}

void SpectrogramWidget::resizeEvent(QResizeEvent *) { update(); }

// ════════════════════════════════════════════════════════════════════════════
//  Paint dispatch
// ════════════════════════════════════════════════════════════════════════════

void SpectrogramWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(12, 12, 18));

    switch (m_mode)
    {
    case DisplayMode::Spectrogram: paintSpectrogram(p); break;
    case DisplayMode::Frequency:   paintFrequency(p);   break;
    case DisplayMode::Amplitude:   paintAmplitude(p);   break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  Timeline strip (thumbnails + time ticks) — shared across modes
// ════════════════════════════════════════════════════════════════════════════

void SpectrogramWidget::drawTimelineStrip(QPainter &p, int y)
{
    const int tlH  = timelineHeight();
    const int tickY = y + (m_hasThumbs ? THUMB_H : 0);

    // ── Background ────────────────────────────────────────────────────────────
    p.fillRect(0, y, width(), tlH, QColor(10, 10, 16));

    // ── Thumbnail strip ───────────────────────────────────────────────────────
    if (m_hasThumbs && !m_thumbnails.isEmpty())
    {
        const QRect thumbStrip(0, y, width(), THUMB_H);

        // Draw each thumbnail at its proportional X position
        for (int i = 0; i < m_thumbnails.size(); ++i)
        {
            double t0 = m_thumbnails[i].first;
            double t1 = (i + 1 < m_thumbnails.size())
                        ? m_thumbnails[i + 1].first
                        : m_duration;

            int x0 = (m_duration > 0) ? int(t0 / m_duration * width()) : 0;
            int x1 = (m_duration > 0) ? int(t1 / m_duration * width()) : width();
            x1     = std::min(x1, width());

            if (x1 <= x0) continue;

            QRect destRect(x0, y, x1 - x0, THUMB_H);
            const QImage &img = m_thumbnails[i].second;
            if (!img.isNull())
                p.drawImage(destRect, img, img.rect());
        }

        // Subtle grid lines between thumbnails
        p.setPen(QPen(QColor(0,0,0,120), 1));
        for (auto &th : m_thumbnails)
        {
            int x = (m_duration > 0) ? int(th.first / m_duration * width()) : 0;
            p.drawLine(x, y, x, y + THUMB_H);
        }

        // Dark top / bottom border of thumbnail strip
        p.setPen(QPen(QColor(0,0,0,180), 1));
        p.drawLine(0, y,            width(), y);
        p.drawLine(0, y + THUMB_H,  width(), y + THUMB_H);
    }

    // ── Time ticks ────────────────────────────────────────────────────────────
    p.setFont(QFont("Monospace", 7));
    p.setPen(QColor(90, 90, 105));

    if (m_duration > 0.0)
    {
        double approx  = std::max(1.0, double(width()) / 80.0);
        double step    = std::max(1.0,
            std::pow(10.0, std::ceil(std::log10(m_duration / approx))));

        for (double t = 0.0; t <= m_duration + 1e-6; t += step)
        {
            int x = timeToPixelX(t);
            if (x < 0 || x >= width()) continue;
            p.setPen(QColor(80, 80, 95));
            p.drawLine(x, tickY, x, tickY + 4);

            int mm = int(t) / 60, ss = int(t) % 60;
            p.setPen(QColor(120, 120, 140));
            p.drawText(x + 2, tickY + 4, width(), AXIS_H,
                       Qt::AlignLeft | Qt::AlignVCenter,
                       mm > 0 ? QString("%1:%2").arg(mm).arg(ss,2,10,QChar('0'))
                               : QString("%1s").arg(ss));
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  Playhead
// ════════════════════════════════════════════════════════════════════════════

void SpectrogramWidget::drawPlayhead(QPainter &p, int x, int topY, int bottomY)
{
    // Shadow
    p.setPen(QPen(QColor(0,0,0,160), 3));
    p.drawLine(x+1, topY, x+1, bottomY);

    // Line
    p.setPen(QPen(QColor(255,255,255,220), 1.5f));
    p.drawLine(x, topY, x, bottomY);

    // Triangle handle
    QPolygonF tri;
    tri << QPointF(x-6, topY) << QPointF(x+6, topY) << QPointF(x, topY+10);
    p.setBrush(Qt::white);
    p.setPen(QPen(QColor(0,0,0,80), 1));
    p.drawPolygon(tri);

    // Time label next to handle
    int mm = int(m_playheadPos)/60, ss = int(m_playheadPos)%60;
    int cs = int((m_playheadPos - std::floor(m_playheadPos))*100);
    QString ts = mm>0
        ? QString("%1:%2.%3").arg(mm).arg(ss,2,10,QChar('0')).arg(cs,2,10,QChar('0'))
        : QString("%1.%2s").arg(ss).arg(cs,2,10,QChar('0'));
    p.setPen(QColor(255,255,200));
    p.setFont(QFont("Monospace", 8, QFont::Bold));
    int tx = std::min(x+10, width()-80);
    p.drawText(tx, topY+20, ts);
}

// ════════════════════════════════════════════════════════════════════════════
//  Mode 1 : Spectrogram
// ════════════════════════════════════════════════════════════════════════════

void SpectrogramWidget::paintSpectrogram(QPainter &p)
{
    const int tlH      = timelineHeight();
    const int spectroH = height() - tlH;
    const QRect spectroRect(0, 0, width(), spectroH);

    if (!m_spectrogramImage.isNull())
    {
        p.drawImage(spectroRect, m_spectrogramImage, m_spectrogramImage.rect());

        // Frequency labels
        if (m_sampleRate > 0)
        {
            double nyq = m_sampleRate / 2.0;
            p.setFont(QFont("Monospace", 7));
            for (double fHz : {100.,250.,500.,1000.,2000.,4000.,8000.,16000.})
            {
                if (fHz > nyq) break;
                int y = spectroH - int(fHz/nyq * spectroH);
                p.setPen(QColor(180,180,180,100));
                p.drawLine(0, y, 4, y);
                p.setPen(QColor(180,180,180));
                p.drawText(6, y+4,
                    fHz>=1000. ? QString("%1k").arg(int(fHz/1000))
                               : QString::number(int(fHz)));
            }
        }
    }
    else
    {
        p.setPen(QColor(60,60,70));
        p.setFont(QFont("Sans", 11));
        p.drawText(spectroRect, Qt::AlignCenter, "Aucun fichier chargé\nFichier > Ouvrir");
    }

    drawTimelineStrip(p, spectroH);

    if (m_duration > 0.0)
        drawPlayhead(p, timeToPixelX(m_playheadPos), 0, spectroH + tlH);
}

// ════════════════════════════════════════════════════════════════════════════
//  Mode 2 : Frequency spectrum
// ════════════════════════════════════════════════════════════════════════════

void SpectrogramWidget::paintFrequency(QPainter &p)
{
    if (m_spec.empty() || m_smoothSpec.empty() || m_sampleRate == 0)
    {
        p.setPen(QColor(70,70,80));
        p.setFont(QFont("Sans", 11));
        p.drawText(rect(), Qt::AlignCenter, "Aucun fichier chargé\nFichier > Ouvrir");
        return;
    }

    const int tlH = timelineHeight();
    constexpr float DB_MIN = -100.f, DB_MAX = 0.f;

    const QRect plotRect(AXIS_LEFT_DB, 10,
                         width() - AXIS_LEFT_DB - 8,
                         height() - tlH - 10);
    if (plotRect.width() <= 0 || plotRect.height() <= 0) return;

    auto dbToY = [&](float db) -> int {
        float t = std::clamp((DB_MAX-db)/(DB_MAX-DB_MIN), 0.f, 1.f);
        return plotRect.top() + int(t * plotRect.height());
    };

    double nyq = m_sampleRate / 2.0;
    p.fillRect(plotRect, QColor(10,10,16));

    // dB grid
    p.setFont(QFont("Monospace", 7));
    for (float db = DB_MAX; db >= DB_MIN; db -= 10.f)
    {
        int y = dbToY(db);
        p.setPen(QPen(QColor(35,38,50), 1, Qt::DotLine));
        p.drawLine(plotRect.left(), y, plotRect.right(), y);
        p.setPen(QColor(100,100,120));
        p.drawText(0, y-5, AXIS_LEFT_DB-4, 14,
                   Qt::AlignRight|Qt::AlignVCenter, QString::number(int(db)));
    }

    // Frequency grid
    static const double FG[] = {50,100,200,500,1000,2000,5000,10000,20000};
    for (double fHz : FG)
    {
        if (fHz > nyq) break;
        int x = freqToPixelX(fHz, plotRect);
        if (x<=plotRect.left()||x>=plotRect.right()) continue;
        p.setPen(QPen(QColor(35,38,50), 1, Qt::DotLine));
        p.drawLine(x, plotRect.top(), x, plotRect.bottom());
        p.setPen(QColor(90,90,110));
        p.drawLine(x, plotRect.bottom(), x, plotRect.bottom()+4);
        p.setFont(QFont("Monospace",7));
        QString lbl = fHz>=1000. ? QString("%1k").arg(int(fHz/1000)) : QString::number(int(fHz));
        p.drawText(x-18, plotRect.bottom()+6, 38, 14, Qt::AlignCenter, lbl);
    }

    // Filled gradient area
    {
        QPainterPath area;
        bool first = true;
        for (int b = 1; b < FREQ_BINS; ++b)
        {
            double hz = double(b) / FREQ_BINS * nyq;
            if (hz < 10.0) continue;
            int x = std::clamp(freqToPixelX(hz,plotRect), plotRect.left(), plotRect.right());
            int y = std::clamp(dbToY(m_smoothSpec[b]), plotRect.top(), plotRect.bottom());
            if (first) { area.moveTo(x, plotRect.bottom()); area.lineTo(x,y); first=false; }
            else area.lineTo(x,y);
        }
        if (!first)
        {
            area.lineTo(plotRect.right(), plotRect.bottom());
            area.closeSubpath();
            QLinearGradient g(0,plotRect.top(),0,plotRect.bottom());
            g.setColorAt(0.00, QColor(255,220,60,230));
            g.setColorAt(0.35, QColor(255,90,20,200));
            g.setColorAt(0.70, QColor(80,20,180,170));
            g.setColorAt(1.00, QColor(20,10,50,100));
            p.fillPath(area, g);
        }
    }
    // Top edge line
    {
        QPainterPath line;
        bool first = true;
        for (int b=1;b<FREQ_BINS;++b)
        {
            double hz = double(b)/FREQ_BINS*nyq;
            if (hz<10.0) continue;
            int x = std::clamp(freqToPixelX(hz,plotRect),plotRect.left(),plotRect.right());
            int y = std::clamp(dbToY(m_smoothSpec[b]),plotRect.top(),plotRect.bottom());
            if(first){line.moveTo(x,y);first=false;} else line.lineTo(x,y);
        }
        p.setPen(QPen(QColor(255,230,120,240),1.5f));
        p.setBrush(Qt::NoBrush);
        p.drawPath(line);
    }
    // Peak-hold dots
    p.setPen(QPen(QColor(255,255,255,200),1.5f));
    for (int b=1;b<FREQ_BINS;++b)
    {
        double hz=double(b)/FREQ_BINS*nyq;
        if(hz<10.0||m_peakHold[b]<DB_MIN+2.f) continue;
        int x=std::clamp(freqToPixelX(hz,plotRect),plotRect.left(),plotRect.right());
        int y=std::clamp(dbToY(m_peakHold[b]),plotRect.top(),plotRect.bottom());
        p.drawPoint(x,y);
    }

    // dB axis label
    { p.save(); p.translate(10, plotRect.center().y()+10); p.rotate(-90);
      p.setPen(QColor(130,130,150)); p.setFont(QFont("Monospace",7,QFont::Bold));
      p.drawText(-20,-2,40,12,Qt::AlignCenter,"dB"); p.restore(); }

    // Position
    { int mm=int(m_playheadPos)/60,ss=int(m_playheadPos)%60,cs=int((m_playheadPos-std::floor(m_playheadPos))*100);
      QString ts = mm>0
          ? QString("@ %1:%2.%3").arg(mm).arg(ss,2,10,QChar('0')).arg(cs,2,10,QChar('0'))
          : QString("@ %1.%2s").arg(ss).arg(cs,2,10,QChar('0'));
      p.setPen(QColor(180,180,200)); p.setFont(QFont("Monospace",8,QFont::Bold));
      p.drawText(AXIS_LEFT_DB+4, 22, ts); }

    p.setPen(QColor(90,90,110));
    p.setFont(QFont("Monospace",7));
    p.drawText(AXIS_LEFT_DB, height()-tlH-4,
               m_logFreq ? "Fréquence (Hz, log)" : "Fréquence (Hz, lin)");

    p.setPen(QPen(QColor(55,55,70),1)); p.setBrush(Qt::NoBrush); p.drawRect(plotRect);

    drawTimelineStrip(p, height() - tlH);
}

// ════════════════════════════════════════════════════════════════════════════
//  Mode 3 : Amplitude (waveform)
// ════════════════════════════════════════════════════════════════════════════

void SpectrogramWidget::paintAmplitude(QPainter &p)
{
    const int tlH   = timelineHeight();
    const int waveH = height() - tlH;
    const QRect waveRect(0,0,width(),waveH);

    if (m_mono.empty())
    {
        p.setPen(QColor(70,70,80)); p.setFont(QFont("Sans",11));
        p.drawText(rect(), Qt::AlignCenter, "Aucun fichier chargé\nFichier > Ouvrir");
        return;
    }

    // Background gradient
    QLinearGradient bg(0,0,0,waveH);
    bg.setColorAt(0.0, QColor(10,10,20));
    bg.setColorAt(0.5, QColor(16,16,28));
    bg.setColorAt(1.0, QColor(10,10,20));
    p.fillRect(waveRect, bg);

    int midY = waveH/2;

    // Grid
    p.setPen(QPen(QColor(28,30,42),1,Qt::DotLine));
    for (int q=1;q<=3;++q) p.drawLine(0,waveH*q/4,width(),waveH*q/4);
    p.setPen(QPen(QColor(45,48,65),1));
    p.drawLine(0,midY,width(),midY);

    // Labels
    p.setFont(QFont("Monospace",7));
    p.setPen(QColor(70,70,90));
    p.drawText(4,midY-13,"+1");
    p.drawText(4,midY+6,"-1");

    // RMS envelope shadow
    if (!m_rmsEnv.empty())
    {
        double rmsBlocks = double(m_mono.size())/512.0;
        QPainterPath rp;
        bool rf=true;
        for (int px=0;px<width();++px)
        {
            int ri=std::clamp(int(double(px)/width()*rmsBlocks),0,int(m_rmsEnv.size())-1);
            float r=m_rmsEnv[size_t(ri)];
            int y=midY-int(r*(waveH/2)*0.95f);
            if(rf){rp.moveTo(px,y);rf=false;} else rp.lineTo(px,y);
        }
        for (int px=width()-1;px>=0;--px)
        {
            int ri=std::clamp(int(double(px)/width()*rmsBlocks),0,int(m_rmsEnv.size())-1);
            rp.lineTo(px, midY+int(m_rmsEnv[size_t(ri)]*(waveH/2)*0.95f));
        }
        rp.closeSubpath();
        p.fillPath(rp, QColor(40,90,180,45));
    }

    // Waveform min/max per pixel
    int64_t total = int64_t(m_mono.size());
    double spp = double(total)/width();

    QLinearGradient wg(0,0,0,waveH);
    wg.setColorAt(0.0, QColor(80,160,255,210));
    wg.setColorAt(0.45, QColor(120,200,255,240));
    wg.setColorAt(0.5,  QColor(130,210,255,255));
    wg.setColorAt(0.55, QColor(120,200,255,240));
    wg.setColorAt(1.0, QColor(80,160,255,210));
    p.setPen(QPen(QBrush(wg),1.0f));

    for (int px=0;px<width();++px)
    {
        int64_t s0=int64_t(px*spp), s1=std::min(int64_t((px+1)*spp)+1, total);
        s0=std::clamp(s0,int64_t(0),total-1);
        float mn=1.f,mx=-1.f;
        for(int64_t si=s0;si<s1;++si){float v=m_mono[size_t(si)];mn=std::min(mn,v);mx=std::max(mx,v);}
        int yt=std::clamp(midY-int(mx*(waveH/2-2)),0,waveH);
        int yb=std::clamp(midY-int(mn*(waveH/2-2)),0,waveH);
        if(yt==yb) p.drawPoint(px,yt); else p.drawLine(px,yt,px,yb);
    }

    drawTimelineStrip(p, waveH);
    if (m_duration > 0.0)
        drawPlayhead(p, timeToPixelX(m_playheadPos), 0, waveH + tlH);
}
