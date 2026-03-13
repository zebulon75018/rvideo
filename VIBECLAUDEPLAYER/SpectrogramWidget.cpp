#include "SpectrogramWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QLinearGradient>
#include <QDebug>

#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ════════════════════════════════════════════════════════════════════════════
//  Ctor / Dtor
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
    m_mono.clear(); m_spec.clear(); m_rmsEnv.clear();
    m_spectrogramImage = QImage();
    m_smoothSpec.clear(); m_peakHold.clear(); m_peakTimer.clear();
    m_playheadPos = 0.0; m_sampleRate = 0; m_duration = 0.0;
    m_numCols = m_numRows = 0;
    clearThumbnails();
    resetYZoom();
    update();
}

void SpectrogramWidget::clearThumbnails()
{
    m_thumbnails.clear(); m_hasThumbs = false; update();
}

void SpectrogramWidget::setThumbnails(const QList<QPair<double,QImage>> &thumbs)
{
    m_thumbnails = thumbs; m_hasThumbs = !thumbs.isEmpty(); update();
}

void SpectrogramWidget::setDisplayMode(DisplayMode m)
{
    if (m_mode == m) return;
    m_mode = m;
    resetYZoom();
    if (m == DisplayMode::Frequency) refreshFreqBuffers(m_playheadPos);
    update();
}

void SpectrogramWidget::setLogFrequency(bool on)
{
    if (m_logFreq != on) { m_logFreq = on; update(); }
}

void SpectrogramWidget::resetYZoom()
{
    m_yZoom = 1.f;
    m_yPan  = 0.f;
    emit yZoomChanged(m_yZoom, m_yPan);
    update();
}

void SpectrogramWidget::setAudioData(const std::vector<float> &samples,
                                     int sampleRate, int channels)
{
    m_sampleRate = sampleRate;
    m_playheadPos = 0.0;

    int64_t numFrames = int64_t(samples.size()) / std::max(1, channels);
    m_mono.resize(size_t(numFrames));
    if (channels == 1)
        m_mono.assign(samples.begin(), samples.end());
    else
        for (int64_t f = 0; f < numFrames; ++f)
        {
            float s = 0.f;
            for (int c = 0; c < channels; ++c)
                s += samples[size_t(f*channels+c)];
            m_mono[size_t(f)] = s / channels;
        }

    m_duration = double(numFrames) / sampleRate;
    m_smoothSpec.assign(FREQ_BINS,-120.f);
    m_peakHold.assign  (FREQ_BINS,-120.f);
    m_peakTimer.assign (FREQ_BINS, 0);

    computeSpectrogram();
    buildSpectrogramImage();
    buildRmsEnvelope();
    refreshFreqBuffers(0.0);
    resetYZoom();
    update();
}

void SpectrogramWidget::setPlaybackPosition(double seconds)
{
    m_playheadPos = std::clamp(seconds, 0.0, m_duration);
    if (!m_spec.empty()) refreshFreqBuffers(m_playheadPos);
    update();
}

// ════════════════════════════════════════════════════════════════════════════
//  Y-zoom helpers
// ════════════════════════════════════════════════════════════════════════════

void SpectrogramWidget::clampPan()
{
    float maxPan = std::max(0.f, 1.f - 1.f / m_yZoom);
    m_yPan = std::clamp(m_yPan, 0.f, maxPan);
}

void SpectrogramWidget::applyZoom(float newZoom, double pivotNorm)
{
    newZoom = std::clamp(newZoom, 1.f, MAX_ZOOM);

    // Keep the point under the cursor fixed
    // pivot = m_yPan + pivotNorm / m_yZoom   →   pivotNorm' / newZoom + newPan
    double anchorFrac = pivotNorm / double(m_yZoom); // fraction from bottom inside visible
    double anchor     = double(m_yPan) + anchorFrac; // absolute normalized position

    m_yZoom = newZoom;
    m_yPan  = float(anchor - pivotNorm / double(m_yZoom));
    clampPan();

    emit yZoomChanged(m_yZoom, m_yPan);
    update();
}

// ════════════════════════════════════════════════════════════════════════════
//  Layout helpers
// ════════════════════════════════════════════════════════════════════════════

int SpectrogramWidget::timelineHeight() const
{
    return m_hasThumbs ? (THUMB_H + AXIS_H) : AXIS_H;
}

int SpectrogramWidget::mainWidth()  const { return std::max(1, width()  - ZOOM_W); }
int SpectrogramWidget::mainHeight() const { return std::max(1, height() - timelineHeight()); }

// ════════════════════════════════════════════════════════════════════════════
//  Coordinate helpers
// ════════════════════════════════════════════════════════════════════════════

int SpectrogramWidget::timeToPixelX(double t) const
{
    if (m_duration <= 0.0) return 0;
    return int(t / m_duration * mainWidth());
}

double SpectrogramWidget::pixelXToTime(int x) const
{
    if (mainWidth() == 0 || m_duration <= 0.0) return 0.0;
    return std::clamp(double(x) / mainWidth() * m_duration, 0.0, m_duration);
}

int SpectrogramWidget::timeToColumn(double seconds) const
{
    if (m_duration <= 0.0 || m_numCols == 0) return 0;
    return std::clamp(int(seconds/m_duration*(m_numCols-1)), 0, m_numCols-1);
}

int SpectrogramWidget::freqToPixelX(double hz, const QRect &r) const
{
    if (m_sampleRate == 0 || r.width() <= 0) return r.left();
    double nyq = m_sampleRate / 2.0, norm;
    if (m_logFreq)
    { double lo=std::log10(20.0),hi=std::log10(nyq);
      norm=(std::log10(std::max(hz,20.0))-lo)/(hi-lo); }
    else norm = hz/nyq;
    return r.left() + int(std::clamp(norm,0.0,1.0)*r.width());
}

double SpectrogramWidget::pixelXToFreq(int x, const QRect &r) const
{
    if (r.width()==0||m_sampleRate==0) return 0.0;
    double norm=double(x-r.left())/r.width(), nyq=m_sampleRate/2.0;
    if (m_logFreq) { double lo=std::log10(20.0),hi=std::log10(nyq);
                     return std::pow(10.0,lo+norm*(hi-lo)); }
    return norm*nyq;
}

/** norm=0 → plotRect.bottom(), norm=1 → plotRect.top() — with zoom/pan applied */
int SpectrogramWidget::normToPixelY(double norm, const QRect &r) const
{
    // visible window: [m_yPan … m_yPan + 1/m_yZoom]
    double visBot = double(m_yPan);
    double visTop = visBot + 1.0 / double(m_yZoom);
    double t = (norm - visBot) / (visTop - visBot);   // 0=bottom, 1=top
    return r.bottom() - int(std::clamp(t, 0.0, 1.0) * r.height());
}

double SpectrogramWidget::pixelYToNorm(int y, const QRect &r) const
{
    if (r.height() == 0) return 0.0;
    double t = 1.0 - double(y - r.top()) / r.height();   // 0=bottom, 1=top in plot
    double visBot = double(m_yPan);
    double visTop = visBot + 1.0 / double(m_yZoom);
    return visBot + t * (visTop - visBot);
}

// ════════════════════════════════════════════════════════════════════════════
//  DSP
// ════════════════════════════════════════════════════════════════════════════

float SpectrogramWidget::hann(int n, int N)
{ return 0.5f*(1.f-std::cos(2.f*float(M_PI)*n/(N-1))); }

void SpectrogramWidget::computeSpectrogram()
{
    m_spec.clear();
    if (m_mono.empty()||m_sampleRate==0) return;
    int64_t total=int64_t(m_mono.size());
    m_numRows=FREQ_BINS;
    std::vector<float> win(FFT_SIZE);
    for(int i=0;i<FFT_SIZE;++i) win[i]=hann(i,FFT_SIZE);
    m_specMinDb=0.f; m_specMaxDb=-200.f;
    for(int64_t start=0;start+FFT_SIZE<=total;start+=HOP_SIZE)
    {
        for(int i=0;i<FFT_SIZE;++i)
            m_fftwIn[i]=m_mono[size_t(start+i)]*win[i];
        fftwf_execute(m_fftwPlan);
        std::vector<float> col(FREQ_BINS);
        for(int k=0;k<FREQ_BINS;++k)
        { float re=m_fftwOut[k][0],im=m_fftwOut[k][1];
          float mag=std::sqrt(re*re+im*im)/FFT_SIZE;
          float db=(mag>1e-10f)?20.f*std::log10(mag):-120.f;
          col[k]=db; m_specMinDb=std::min(m_specMinDb,db); m_specMaxDb=std::max(m_specMaxDb,db); }
        m_spec.push_back(std::move(col));
    }
    m_numCols=int(m_spec.size());
    if(m_specMaxDb<=m_specMinDb) m_specMaxDb=m_specMinDb+1.f;
}

QRgb SpectrogramWidget::dbToColor(float db,float minDb,float maxDb)
{
    float t=std::clamp((db-minDb)/(maxDb-minDb),0.f,1.f);
    struct Stop{float pos;uint8_t r,g,b;};
    static constexpr Stop s[]={
        {0.00f,0,0,4},{0.13f,27,12,65},{0.25f,71,15,100},{0.38f,122,23,84},
        {0.50f,175,54,34},{0.63f,218,112,31},{0.75f,252,180,25},
        {0.88f,252,230,100},{1.00f,252,252,164}};
    int i=0; while(i<7&&t>s[i+1].pos)++i;
    float f=(t-s[i].pos)/(s[i+1].pos-s[i].pos);
    auto L=[&](uint8_t a,uint8_t b){return int(a+f*(b-a));};
    return qRgb(L(s[i].r,s[i+1].r),L(s[i].g,s[i+1].g),L(s[i].b,s[i+1].b));
}

void SpectrogramWidget::buildSpectrogramImage()
{
    if(m_spec.empty()){m_spectrogramImage=QImage();return;}
    m_spectrogramImage=QImage(m_numCols,m_numRows,QImage::Format_RGB32);
    for(int col=0;col<m_numCols;++col)
        for(int bin=0;bin<m_numRows;++bin)
            m_spectrogramImage.setPixel(col,m_numRows-1-bin,
                dbToColor(m_spec[col][bin],m_specMinDb,m_specMaxDb));
}

void SpectrogramWidget::buildRmsEnvelope()
{
    m_rmsEnv.clear(); if(m_mono.empty()) return;
    constexpr int BLK=512;
    int64_t total=int64_t(m_mono.size()), nb=total/BLK+1;
    m_rmsEnv.resize(size_t(nb));
    for(int64_t b=0;b<nb;++b)
    { int64_t s0=b*BLK,s1=std::min(s0+BLK,total); double sum=0;
      for(int64_t s=s0;s<s1;++s){double v=m_mono[size_t(s)];sum+=v*v;}
      m_rmsEnv[size_t(b)]=float(std::sqrt(sum/(s1-s0))); }
}

void SpectrogramWidget::refreshFreqBuffers(double seconds)
{
    if(m_spec.empty()||m_smoothSpec.empty()) return;
    int col=timeToColumn(seconds);
    if(col<0||col>=m_numCols) return;
    constexpr float ALPHA=0.45f; constexpr int HOLD=20; constexpr float DEC=1.2f;
    const auto &frame=m_spec[col];
    for(int b=0;b<FREQ_BINS;++b)
    { float v=frame[b];
      m_smoothSpec[b]=ALPHA*v+(1.f-ALPHA)*m_smoothSpec[b];
      if(v>=m_peakHold[b]){m_peakHold[b]=v;m_peakTimer[b]=HOLD;}
      else{if(m_peakTimer[b]>0)--m_peakTimer[b];else m_peakHold[b]-=DEC;}
      m_peakHold[b]=std::max(m_peakHold[b],-120.f); }
}

// ════════════════════════════════════════════════════════════════════════════
//  Zoom panel geometry & hit-testing
// ════════════════════════════════════════════════════════════════════════════

QRect SpectrogramWidget::zoomPanelRect() const
{
    return QRect(mainWidth(), 0, ZOOM_W, mainHeight());
}

QRect SpectrogramWidget::zoomSliderTrack() const
{
    QRect p = zoomPanelRect();
    int bh  = zoomBtnH();
    // Track sits between [+] at top, [-] then [⊙] at bottom
    return QRect(p.left()+4, p.top()+bh+4, p.width()-8, p.height()-3*bh-12);
}

QRect SpectrogramWidget::zoomThumbRect() const
{
    QRect track = zoomSliderTrack();
    if (track.height() <= 0) return track;

    float maxPan = std::max(0.f, 1.f - 1.f/m_yZoom);
    float thumbH_norm = 1.f / m_yZoom;                     // proportion of track
    int   thumbH = std::max(14, int(thumbH_norm * track.height()));
    // pan within [0, maxPan] → thumb bottom within track
    float panFrac = (maxPan > 0.f) ? m_yPan / maxPan : 0.f; // 0=bottom,1=top
    int   thumbTop = track.bottom() - int(panFrac * (track.height() - thumbH)) - thumbH;
    thumbTop = std::clamp(thumbTop, track.top(), track.bottom()-thumbH);
    return QRect(track.left(), thumbTop, track.width(), thumbH);
}

SpectrogramWidget::ZoomHit SpectrogramWidget::zoomHitTest(const QPoint &pos) const
{
    QRect panel = zoomPanelRect();
    if (!panel.contains(pos)) return ZoomHit::None;

    int bh = zoomBtnH();
    QRect btnPlus (panel.left(), panel.top(),             panel.width(), bh);
    QRect btnMinus(panel.left(), panel.bottom()-2*bh+1,   panel.width(), bh);
    QRect btnReset(panel.left(), panel.bottom()-bh+1,     panel.width(), bh);

    if (btnPlus .contains(pos)) return ZoomHit::BtnPlus;
    if (btnMinus.contains(pos)) return ZoomHit::BtnMinus;
    if (btnReset.contains(pos)) return ZoomHit::BtnReset;
    if (zoomThumbRect().contains(pos)) return ZoomHit::SliderThumb;
    if (zoomSliderTrack().contains(pos)) return ZoomHit::SliderTrack;
    return ZoomHit::None;
}

// ════════════════════════════════════════════════════════════════════════════
//  Mouse / Wheel events
// ════════════════════════════════════════════════════════════════════════════

void SpectrogramWidget::mousePressEvent(QMouseEvent *e)
{
    QPoint pos = e->pos();

    // ── Zoom panel clicks ─────────────────────────────────────────────────────
    switch (zoomHitTest(pos))
    {
    case ZoomHit::BtnPlus:
        applyZoom(m_yZoom * 2.f, 0.5); return;
    case ZoomHit::BtnMinus:
        applyZoom(m_yZoom / 2.f, 0.5); return;
    case ZoomHit::BtnReset:
        resetYZoom(); return;
    case ZoomHit::SliderThumb:
        m_thumbDragging     = true;
        m_thumbDragStartY   = pos.y();
        m_thumbDragStartPan = m_yPan;
        return;
    case ZoomHit::SliderTrack:
    {
        // Click on track → jump pan to clicked position
        QRect track = zoomSliderTrack();
        if (track.height() > 0)
        {
            float t   = 1.f - float(pos.y() - track.top()) / track.height();
            float maxP= std::max(0.f, 1.f - 1.f/m_yZoom);
            m_yPan    = std::clamp(t * maxP, 0.f, maxP);
            clampPan();
            emit yZoomChanged(m_yZoom, m_yPan);
            update();
        }
        return;
    }
    default: break;
    }

    if (e->button() != Qt::LeftButton) return;

    // ── Shift+click → start Y-pan drag on main area ───────────────────────────
    if (e->modifiers() & Qt::ShiftModifier)
    {
        m_panDragging     = true;
        m_panDragStartY   = pos.y();
        m_panDragStartPan = m_yPan;
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    // ── Normal click → seek playhead (Spectro / Amplitude) ───────────────────
    if (m_duration > 0.0 && m_mode != DisplayMode::Frequency
        && pos.x() < mainWidth())
    {
        m_dragging    = true;
        m_playheadPos = pixelXToTime(pos.x());
        update();
        emit seekRequested(m_playheadPos);
    }
}

void SpectrogramWidget::mouseMoveEvent(QMouseEvent *e)
{
    QPoint pos = e->pos();

    // ── Thumb drag ────────────────────────────────────────────────────────────
    if (m_thumbDragging)
    {
        QRect track = zoomSliderTrack();
        float maxP  = std::max(0.f, 1.f - 1.f/m_yZoom);
        if (track.height() > 0 && maxP > 0.f)
        {
            int   dy   = m_thumbDragStartY - pos.y();   // up = positive
            float dPan = float(dy) / track.height() * maxP;
            m_yPan     = std::clamp(m_thumbDragStartPan + dPan, 0.f, maxP);
            emit yZoomChanged(m_yZoom, m_yPan);
            update();
        }
        return;
    }

    // ── Y-pan drag (shift+drag) ───────────────────────────────────────────────
    if (m_panDragging)
    {
        int   mh   = mainHeight();
        if (mh > 0)
        {
            int   dy   = m_panDragStartY - pos.y();    // up = increase pan
            float dPan = float(dy) / mh / m_yZoom;
            float maxP = std::max(0.f, 1.f - 1.f/m_yZoom);
            m_yPan     = std::clamp(m_panDragStartPan + dPan, 0.f, maxP);
            emit yZoomChanged(m_yZoom, m_yPan);
            update();
        }
        return;
    }

    // ── Seek drag ─────────────────────────────────────────────────────────────
    if (m_dragging && (e->buttons() & Qt::LeftButton)
        && m_mode != DisplayMode::Frequency)
    {
        m_playheadPos = pixelXToTime(pos.x());
        update();
        emit seekRequested(m_playheadPos);
        return;
    }

    // ── Cursor updates ────────────────────────────────────────────────────────
    ZoomHit zh = zoomHitTest(pos);
    if (zh == ZoomHit::SliderThumb || zh == ZoomHit::SliderTrack)
    { setCursor(Qt::PointingHandCursor); return; }
    if (zh != ZoomHit::None)
    { setCursor(Qt::ArrowCursor); return; }

    if (m_mode != DisplayMode::Frequency && pos.x() < mainWidth())
    {
        int phX = timeToPixelX(m_playheadPos);
        if (std::abs(pos.x() - phX) <= PLAYHEAD_HIT)
            { setCursor(Qt::SizeHorCursor); return; }
        if (e->modifiers() & Qt::ShiftModifier)
            { setCursor(Qt::OpenHandCursor); return; }
    }
    setCursor(Qt::ArrowCursor);
}

void SpectrogramWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
    {
        m_dragging = m_panDragging = m_thumbDragging = false;
        setCursor(Qt::ArrowCursor);
    }
}

void SpectrogramWidget::wheelEvent(QWheelEvent *e)
{
    if (e->position().x() >= mainWidth()) { e->ignore(); return; }

    // Determine pivot: normalised Y position under cursor
    int     mh    = mainHeight();
    QRect   plot(0, 0, mainWidth(), mh);
    double  pivot = (mh > 0)
                  ? pixelYToNorm(int(e->position().y()), plot)
                  : 0.5;
    pivot = std::clamp(pivot, 0.0, 1.0);

    float delta = (e->angleDelta().y() > 0) ? 1.25f : 1.f/1.25f;
    applyZoom(m_yZoom * delta, pivot);
    e->accept();
}

void SpectrogramWidget::resizeEvent(QResizeEvent *) { update(); }

// ════════════════════════════════════════════════════════════════════════════
//  Zoom panel painter
// ════════════════════════════════════════════════════════════════════════════

void SpectrogramWidget::drawZoomPanel(QPainter &p)
{
    QRect panel = zoomPanelRect();
    if (panel.isEmpty()) return;

    const int bh = zoomBtnH();

    // ── Background ────────────────────────────────────────────────────────────
    p.fillRect(panel, QColor(16, 16, 24));
    p.setPen(QPen(QColor(40, 42, 58), 1));
    p.drawLine(panel.left(), 0, panel.left(), panel.bottom());

    // ── Helper to paint a square button ──────────────────────────────────────
    auto drawBtn = [&](const QRect &r, const QString &label,
                       bool highlight = false)
    {
        QColor bg  = highlight ? QColor(55, 80, 130) : QColor(28, 30, 42);
        QColor bdr = highlight ? QColor(80,120,200)  : QColor(50, 52, 70);
        p.fillRect(r.adjusted(2,2,-2,-2), bg);
        p.setPen(bdr);
        p.drawRect(r.adjusted(2,2,-2,-2));
        p.setPen(highlight ? Qt::white : QColor(160,165,190));
        p.setFont(QFont("Sans", 11, QFont::Bold));
        p.drawText(r, Qt::AlignCenter, label);
    };

    // ── [+] button ────────────────────────────────────────────────────────────
    QRect btnPlus(panel.left(), panel.top(), panel.width(), bh);
    drawBtn(btnPlus, "+", m_yZoom < MAX_ZOOM);

    // ── [−] button ────────────────────────────────────────────────────────────
    QRect btnMinus(panel.left(), panel.bottom()-2*bh+1, panel.width(), bh);
    drawBtn(btnMinus, "−", m_yZoom > 1.f);

    // ── [⊙] reset button ─────────────────────────────────────────────────────
    QRect btnReset(panel.left(), panel.bottom()-bh+1, panel.width(), bh);
    drawBtn(btnReset, "⊙", m_yZoom > 1.f);

    // ── Slider track ──────────────────────────────────────────────────────────
    QRect track = zoomSliderTrack();
    if (track.height() > 4)
    {
        // Track background
        p.fillRect(track, QColor(20, 22, 32));
        p.setPen(QColor(40, 42, 60));
        p.drawRect(track);

        // Zoom level text inside track
        p.setPen(QColor(70, 75, 100));
        p.setFont(QFont("Monospace", 6));
        QString zStr = (m_yZoom >= 2.f)
            ? QString("×%1").arg(int(m_yZoom))
            : QString("×1");
        p.drawText(track, Qt::AlignHCenter | Qt::AlignTop, zStr);

        // Thumb (only useful when zoomed in)
        if (m_yZoom > 1.f)
        {
            QRect thumb = zoomThumbRect();
            QLinearGradient tg(thumb.left(), 0, thumb.right(), 0);
            tg.setColorAt(0.0, QColor(55, 85, 160));
            tg.setColorAt(1.0, QColor(80,120,210));
            p.fillRect(thumb.adjusted(1,0,-1,0), tg);
            p.setPen(QColor(100,140,240));
            p.drawRect(thumb.adjusted(1,0,-1,0));

            // Grip lines
            p.setPen(QColor(140,170,255,150));
            int mid = thumb.center().y();
            for (int dy : {-3,0,3})
                p.drawLine(thumb.left()+4, mid+dy, thumb.right()-4, mid+dy);
        }
        else
        {
            // Show "scroll to zoom" hint
            p.save();
            p.translate(track.center().x(), track.center().y());
            p.rotate(-90);
            p.setPen(QColor(55, 58, 80));
            p.setFont(QFont("Sans", 6));
            p.drawText(-30, -4, 60, 10, Qt::AlignCenter, "wheel=zoom");
            p.restore();
        }
    }

    // ── Visible range indicator (tiny text at the bottom of panel) ───────────
    p.setPen(QColor(70, 75, 100));
    p.setFont(QFont("Monospace", 6));
    // Show current pan range as percentage
    float visBot = m_yPan * 100.f;
    float visTop = std::min((m_yPan + 1.f/m_yZoom) * 100.f, 100.f);
    QString range = QString("%1–%2%").arg(int(visBot)).arg(int(visTop));
    p.drawText(panel.left(), panel.top()+bh+1, panel.width(), 12,
               Qt::AlignCenter, range);
}

// ════════════════════════════════════════════════════════════════════════════
//  Paint dispatch
// ════════════════════════════════════════════════════════════════════════════

void SpectrogramWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(12,12,18));

    switch (m_mode)
    {
    case DisplayMode::Spectrogram: paintSpectrogram(p); break;
    case DisplayMode::Frequency:   paintFrequency(p);   break;
    case DisplayMode::Amplitude:   paintAmplitude(p);   break;
    }

    drawZoomPanel(p);
}

// ════════════════════════════════════════════════════════════════════════════
//  Common: timeline strip + playhead
// ════════════════════════════════════════════════════════════════════════════

void SpectrogramWidget::drawTimelineStrip(QPainter &p, int y)
{
    const int tlH  = timelineHeight();
    const int tickY = y + (m_hasThumbs ? THUMB_H : 0);
    const int mw    = mainWidth();

    p.fillRect(0, y, mw, tlH, QColor(10,10,16));

    if (m_hasThumbs && !m_thumbnails.isEmpty())
    {
        for (int i = 0; i < m_thumbnails.size(); ++i)
        {
            double t0 = m_thumbnails[i].first;
            double t1 = (i+1 < m_thumbnails.size())
                        ? m_thumbnails[i+1].first : m_duration;
            int x0 = (m_duration>0) ? int(t0/m_duration*mw) : 0;
            int x1 = (m_duration>0) ? int(t1/m_duration*mw) : mw;
            x1 = std::min(x1, mw);
            if (x1 <= x0) continue;
            QRect dest(x0, y, x1-x0, THUMB_H);
            const QImage &img = m_thumbnails[i].second;
            if (!img.isNull()) p.drawImage(dest, img, img.rect());
        }
        p.setPen(QPen(QColor(0,0,0,120),1));
        for (auto &th : m_thumbnails)
        { int x=(m_duration>0)?int(th.first/m_duration*mw):0;
          p.drawLine(x,y,x,y+THUMB_H); }
        p.setPen(QPen(QColor(0,0,0,180),1));
        p.drawLine(0,y,mw,y); p.drawLine(0,y+THUMB_H,mw,y+THUMB_H);
    }

    // Time ticks
    p.setFont(QFont("Monospace",7));
    p.setPen(QColor(90,90,105));
    if (m_duration > 0.0)
    {
        double approx = std::max(1.0, double(mw)/80.0);
        double step   = std::max(1.0, std::pow(10.0,
            std::ceil(std::log10(m_duration/approx))));
        for (double t=0.0; t<=m_duration+1e-6; t+=step)
        {
            int x=timeToPixelX(t); if(x<0||x>=mw) continue;
            p.setPen(QColor(80,80,95));
            p.drawLine(x,tickY,x,tickY+4);
            int mm=int(t)/60, ss=int(t)%60;
            p.setPen(QColor(120,120,140));
            p.drawText(x+2, tickY+4, mw, AXIS_H,
                       Qt::AlignLeft|Qt::AlignVCenter,
                       mm>0?QString("%1:%2").arg(mm).arg(ss,2,10,QChar('0'))
                           :QString("%1s").arg(ss));
        }
    }
}

void SpectrogramWidget::drawPlayhead(QPainter &p, int x, int topY, int bottomY)
{
    p.setPen(QPen(QColor(0,0,0,160),3));
    p.drawLine(x+1,topY,x+1,bottomY);
    p.setPen(QPen(QColor(255,255,255,220),1.5f));
    p.drawLine(x,topY,x,bottomY);
    QPolygonF tri; tri<<QPointF(x-6,topY)<<QPointF(x+6,topY)<<QPointF(x,topY+10);
    p.setBrush(Qt::white); p.setPen(QPen(QColor(0,0,0,80),1)); p.drawPolygon(tri);

    int mm=int(m_playheadPos)/60, ss=int(m_playheadPos)%60;
    int cs=int((m_playheadPos-std::floor(m_playheadPos))*100);
    QString ts=mm>0?QString("%1:%2.%3").arg(mm).arg(ss,2,10,QChar('0')).arg(cs,2,10,QChar('0'))
                   :QString("%1.%2s").arg(ss).arg(cs,2,10,QChar('0'));
    p.setPen(QColor(255,255,200));
    p.setFont(QFont("Monospace",8,QFont::Bold));
    p.drawText(std::min(x+10,mainWidth()-80), topY+20, ts);
}

// ════════════════════════════════════════════════════════════════════════════
//  Mode 1 : Spectrogram — Y zoom applied to frequency axis
// ════════════════════════════════════════════════════════════════════════════

void SpectrogramWidget::paintSpectrogram(QPainter &p)
{
    const int tlH      = timelineHeight();
    const int mw       = mainWidth();
    const int spectroH = height() - tlH;
    const QRect plotRect(0, 0, mw, spectroH);

    if (m_spectrogramImage.isNull())
    {
        p.setPen(QColor(60,60,70)); p.setFont(QFont("Sans",11));
        p.drawText(plotRect, Qt::AlignCenter, "Aucun fichier chargé\nFichier > Ouvrir");
    }
    else
    {
        // Visible frequency range [visBot … visTop] as fraction of Nyquist
        double visBot = double(m_yPan);
        double visTop = std::min(visBot + 1.0/double(m_yZoom), 1.0);

        // Source pixel rows in the spectrogram image (image rows: 0=Nyquist, max=0Hz)
        int srcRowTop    = int((1.0 - visTop) * m_numRows);   // image row for top freq
        int srcRowBottom = int((1.0 - visBot) * m_numRows);   // image row for bottom freq
        srcRowTop    = std::clamp(srcRowTop,    0, m_numRows-1);
        srcRowBottom = std::clamp(srcRowBottom, srcRowTop+1, m_numRows);

        QRect srcRect(0, srcRowTop, m_numCols, srcRowBottom - srcRowTop);
        p.drawImage(plotRect, m_spectrogramImage, srcRect);

        // ── Frequency labels (recomputed for visible range) ───────────────────
        if (m_sampleRate > 0)
        {
            double nyq = m_sampleRate / 2.0;
            double freqBot = visBot * nyq;
            double freqTop = visTop * nyq;
            p.setFont(QFont("Monospace",7));
            for (double fHz : {20.,50.,100.,200.,500.,1000.,2000.,4000.,8000.,16000.,20000.})
            {
                if (fHz < freqBot || fHz > freqTop) continue;
                double norm = fHz / nyq;
                int y = normToPixelY(norm, plotRect);
                p.setPen(QColor(180,180,180,110));
                p.drawLine(0,y,4,y);
                p.setPen(QColor(180,180,180));
                p.drawText(6,y+4,
                    fHz>=1000.?QString("%1k").arg(int(fHz/1000)):QString::number(int(fHz)));
            }
        }

        // ── Zoom range indicator (left edge badge) ────────────────────────────
        if (m_yZoom > 1.f && m_sampleRate > 0)
        {
            double nyq = m_sampleRate / 2.0;
            int    fBot = int(visBot * nyq), fTop = int(visTop * nyq);
            QString badge = QString("%1–%2 Hz").arg(fBot).arg(fTop);
            p.setPen(QColor(0,0,0,140));
            p.setBrush(QColor(0,0,0,140));
            QFontMetrics fm(QFont("Monospace",7));
            QRect br = fm.boundingRect(badge).adjusted(-3,-2,3,2);
            br.moveTopLeft({4, spectroH - 18});
            p.drawRoundedRect(br,3,3);
            p.setPen(QColor(200,230,255));
            p.setFont(QFont("Monospace",7));
            p.drawText(br, Qt::AlignCenter, badge);
        }
    }

    drawTimelineStrip(p, spectroH);
    if (m_duration > 0.0)
        drawPlayhead(p, timeToPixelX(m_playheadPos), 0, spectroH + tlH);
}

// ════════════════════════════════════════════════════════════════════════════
//  Mode 2 : Frequency spectrum — Y zoom applied to dB axis
// ════════════════════════════════════════════════════════════════════════════

void SpectrogramWidget::paintFrequency(QPainter &p)
{
    if (m_spec.empty()||m_smoothSpec.empty()||m_sampleRate==0)
    {
        p.setPen(QColor(70,70,80)); p.setFont(QFont("Sans",11));
        p.drawText(rect(), Qt::AlignCenter, "Aucun fichier chargé\nFichier > Ouvrir");
        return;
    }

    const int tlH = timelineHeight();
    const int mw  = mainWidth();

    // Visible dB range based on zoom/pan
    constexpr float DB_FULL_MIN = -100.f;
    constexpr float DB_FULL_MAX =    0.f;
    constexpr float DB_RANGE    = DB_FULL_MAX - DB_FULL_MIN;

    float dbBot = DB_FULL_MIN + float(m_yPan)             * DB_RANGE;
    float dbTop = DB_FULL_MIN + float(m_yPan + 1.f/m_yZoom) * DB_RANGE;
    dbTop = std::min(dbTop, DB_FULL_MAX);

    const QRect plotRect(AXIS_LEFT_DB, 10, mw - AXIS_LEFT_DB - 6, height()-tlH-10);
    if (plotRect.width()<=0||plotRect.height()<=0) return;

    auto dbToY = [&](float db) -> int {
        float t = std::clamp((dbTop-db)/(dbTop-dbBot), 0.f,1.f);
        return plotRect.top() + int(t*plotRect.height());
    };

    double nyq = m_sampleRate / 2.0;
    p.fillRect(plotRect, QColor(10,10,16));

    // dB grid lines
    p.setFont(QFont("Monospace",7));
    float gridStep = (dbTop - dbBot > 30.f) ? 10.f : 5.f;
    float firstLine = std::ceil(dbBot / gridStep) * gridStep;
    for (float db = firstLine; db <= dbTop + 0.1f; db += gridStep)
    {
        int y = dbToY(db);
        p.setPen(QPen(QColor(35,38,50),1,Qt::DotLine));
        p.drawLine(plotRect.left(),y,plotRect.right(),y);
        p.setPen(QColor(100,100,120));
        p.drawText(0,y-5,AXIS_LEFT_DB-4,14,Qt::AlignRight|Qt::AlignVCenter,
                   QString::number(int(db)));
    }

    // Frequency grid
    static const double FG[]={50,100,200,500,1000,2000,5000,10000,20000};
    for (double fHz:FG)
    { if(fHz>nyq) break;
      int x=freqToPixelX(fHz,plotRect);
      if(x<=plotRect.left()||x>=plotRect.right()) continue;
      p.setPen(QPen(QColor(35,38,50),1,Qt::DotLine));
      p.drawLine(x,plotRect.top(),x,plotRect.bottom());
      p.setPen(QColor(90,90,110));
      p.drawLine(x,plotRect.bottom(),x,plotRect.bottom()+4);
      p.setFont(QFont("Monospace",7));
      QString lbl=fHz>=1000.?QString("%1k").arg(int(fHz/1000)):QString::number(int(fHz));
      p.drawText(x-18,plotRect.bottom()+6,38,14,Qt::AlignCenter,lbl); }

    // Filled gradient area (clipped to visible dB)
    {
        QPainterPath area; bool first=true;
        for (int b=1;b<FREQ_BINS;++b)
        {
            double hz=double(b)/FREQ_BINS*nyq; if(hz<10.0) continue;
            int x=std::clamp(freqToPixelX(hz,plotRect),plotRect.left(),plotRect.right());
            int y=std::clamp(dbToY(m_smoothSpec[b]),plotRect.top(),plotRect.bottom());
            if(first){area.moveTo(x,plotRect.bottom());area.lineTo(x,y);first=false;}
            else area.lineTo(x,y);
        }
        if(!first)
        { area.lineTo(plotRect.right(),plotRect.bottom());area.closeSubpath();
          QLinearGradient g(0,plotRect.top(),0,plotRect.bottom());
          g.setColorAt(0.00,QColor(255,220,60,230));
          g.setColorAt(0.35,QColor(255,90,20,200));
          g.setColorAt(0.70,QColor(80,20,180,170));
          g.setColorAt(1.00,QColor(20,10,50,100));
          p.fillPath(area,g); }
    }
    // Edge line
    { QPainterPath line; bool first=true;
      for(int b=1;b<FREQ_BINS;++b)
      { double hz=double(b)/FREQ_BINS*nyq; if(hz<10.0) continue;
        int x=std::clamp(freqToPixelX(hz,plotRect),plotRect.left(),plotRect.right());
        int y=std::clamp(dbToY(m_smoothSpec[b]),plotRect.top(),plotRect.bottom());
        if(first){line.moveTo(x,y);first=false;}else line.lineTo(x,y); }
      p.setPen(QPen(QColor(255,230,120,240),1.5f)); p.setBrush(Qt::NoBrush); p.drawPath(line); }
    // Peak-hold dots
    p.setPen(QPen(QColor(255,255,255,200),1.5f));
    for(int b=1;b<FREQ_BINS;++b)
    { double hz=double(b)/FREQ_BINS*nyq; if(hz<10.0) continue;
      if(m_peakHold[b]<dbBot) continue;
      int x=std::clamp(freqToPixelX(hz,plotRect),plotRect.left(),plotRect.right());
      int y=std::clamp(dbToY(m_peakHold[b]),plotRect.top(),plotRect.bottom());
      p.drawPoint(x,y); }

    // dB axis label
    { p.save(); p.translate(10,plotRect.center().y()+10); p.rotate(-90);
      p.setPen(QColor(130,130,150)); p.setFont(QFont("Monospace",7,QFont::Bold));
      p.drawText(-20,-2,40,12,Qt::AlignCenter,"dB"); p.restore(); }

    // Visible range badge
    { QString badge=QString("dB %1 → %2").arg(int(dbBot)).arg(int(dbTop));
      p.setPen(QColor(180,180,200)); p.setFont(QFont("Monospace",7,QFont::Bold));
      p.drawText(AXIS_LEFT_DB+4,22,badge); }

    p.setPen(QColor(90,90,110)); p.setFont(QFont("Monospace",7));
    p.drawText(AXIS_LEFT_DB,height()-tlH-4,
               m_logFreq?"Fréquence (Hz, log)":"Fréquence (Hz, lin)");

    p.setPen(QPen(QColor(55,55,70),1)); p.setBrush(Qt::NoBrush); p.drawRect(plotRect);

    drawTimelineStrip(p, height()-tlH);
}

// ════════════════════════════════════════════════════════════════════════════
//  Mode 3 : Amplitude — Y zoom applied to sample amplitude axis
// ════════════════════════════════════════════════════════════════════════════

void SpectrogramWidget::paintAmplitude(QPainter &p)
{
    const int tlH   = timelineHeight();
    const int mw    = mainWidth();
    const int waveH = height() - tlH;
    const QRect plotRect(0, 0, mw, waveH);

    if (m_mono.empty())
    {
        p.setPen(QColor(70,70,80)); p.setFont(QFont("Sans",11));
        p.drawText(plotRect, Qt::AlignCenter, "Aucun fichier chargé\nFichier > Ouvrir");
        return;
    }

    // Visible amplitude range in [-1, 1] mapped from [0,1] norm
    // norm=0 → -1.0, norm=1 → +1.0
    double ampBot = double(m_yPan)              * 2.0 - 1.0;
    double ampTop = double(m_yPan + 1.f/m_yZoom) * 2.0 - 1.0;
    ampTop = std::min(ampTop, 1.0);

    // sample value → pixel Y inside plotRect
    auto ampToY = [&](float v) -> int {
        double t = (ampTop - v) / (ampTop - ampBot);   // 0=top, 1=bottom
        return plotRect.top() + int(std::clamp(t,0.0,1.0) * plotRect.height());
    };

    // Background gradient
    QLinearGradient bg(0,0,0,waveH);
    bg.setColorAt(0.0, QColor(10,10,20));
    bg.setColorAt(0.5, QColor(16,16,28));
    bg.setColorAt(1.0, QColor(10,10,20));
    p.fillRect(plotRect, bg);

    // Grid lines (amplitude)
    p.setPen(QPen(QColor(28,30,42),1,Qt::DotLine));
    for (float v = std::ceil(ampBot*4.f)/4.f; v <= ampTop+0.01f; v += 0.25f)
        p.drawLine(0, ampToY(v), mw, ampToY(v));

    // Zero line
    if (0.0 >= ampBot && 0.0 <= ampTop)
    {
        p.setPen(QPen(QColor(45,48,65),1));
        int y0 = ampToY(0.f);
        p.drawLine(0, y0, mw, y0);

        // Amplitude labels
        p.setPen(QColor(70,70,90)); p.setFont(QFont("Monospace",7));
        p.drawText(4, y0-13, "+1");
        p.drawText(4, y0+5,  "−1");
    }

    // Visible range badge
    { QString badge=QString("%1 → %2")
          .arg(float(ampBot),5,'f',2).arg(float(ampTop),5,'f',2);
      p.setPen(QColor(0,0,0,140)); p.setBrush(QColor(0,0,0,140));
      QFontMetrics fm(QFont("Monospace",7));
      QRect br=fm.boundingRect(badge).adjusted(-3,-2,3,2);
      br.moveTopLeft({4,4});
      p.drawRoundedRect(br,3,3);
      p.setPen(QColor(180,210,255));
      p.setFont(QFont("Monospace",7));
      p.drawText(br,Qt::AlignCenter,badge); }

    // RMS envelope shadow
    if (!m_rmsEnv.empty())
    {
        double rmsBlocks=double(m_mono.size())/512.0;
        QPainterPath rp; bool rf=true;
        for(int px=0;px<mw;++px)
        { int ri=std::clamp(int(double(px)/mw*rmsBlocks),0,int(m_rmsEnv.size())-1);
          float r=m_rmsEnv[size_t(ri)];
          int y=ampToY(r);
          if(rf){rp.moveTo(px,y);rf=false;}else rp.lineTo(px,y); }
        for(int px=mw-1;px>=0;--px)
        { int ri=std::clamp(int(double(px)/mw*rmsBlocks),0,int(m_rmsEnv.size())-1);
          rp.lineTo(px,ampToY(-m_rmsEnv[size_t(ri)])); }
        rp.closeSubpath();
        p.fillPath(rp, QColor(40,90,180,45));
    }

    // Waveform min/max per pixel
    int64_t total = int64_t(m_mono.size());
    double  spp   = double(total) / mw;

    QLinearGradient wg(0,0,0,waveH);
    wg.setColorAt(0.0,  QColor(80,160,255,210));
    wg.setColorAt(0.45, QColor(120,200,255,240));
    wg.setColorAt(0.5,  QColor(130,210,255,255));
    wg.setColorAt(0.55, QColor(120,200,255,240));
    wg.setColorAt(1.0,  QColor(80,160,255,210));
    p.setPen(QPen(QBrush(wg),1.0f));

    for(int px=0;px<mw;++px)
    {
        int64_t s0=int64_t(px*spp), s1=std::min(int64_t((px+1)*spp)+1,total);
        s0=std::clamp(s0,int64_t(0),total-1);
        float mn=1.f,mx=-1.f;
        for(int64_t si=s0;si<s1;++si){float v=m_mono[size_t(si)];mn=std::min(mn,v);mx=std::max(mx,v);}
        // Only draw if any part of the segment is in the visible range
        if (mx < float(ampBot) || mn > float(ampTop)) continue;
        int yt=ampToY(std::min(mx, float(ampTop)));
        int yb=ampToY(std::max(mn, float(ampBot)));
        if(yt==yb) p.drawPoint(px,yt); else p.drawLine(px,yt,px,yb);
    }

    drawTimelineStrip(p, waveH);
    if (m_duration > 0.0)
        drawPlayhead(p, timeToPixelX(m_playheadPos), 0, waveH + tlH);
}
