#pragma once

#include <QWidget>
#include <QImage>
#include <QList>
#include <QPair>
#include <vector>
#include <cstdint>

#include <fftw3.h>

/**
 * SpectrogramWidget
 * -----------------
 * Three visualization modes (Spectrogram / Frequency / Amplitude)
 * with a full Y-axis zoom + pan system.
 *
 * ── Y-axis zoom / pan ─────────────────────────────────────────────────────
 *  m_yZoom  : float  [1 … MAX_ZOOM]   current zoom factor
 *  m_yPan   : float  [0 … 1]          normalized bottom of the visible window
 *
 *  Visible window in normalized [0,1] space:
 *    bottom = m_yPan
 *    top    = m_yPan + 1/m_yZoom   (clamped to 1)
 *
 *  Meaning per mode:
 *    Spectrogram  0=0 Hz         1=Nyquist
 *    Amplitude    0=−1.0 sample  1=+1.0 sample
 *    Frequency    0=DB_MIN       1=DB_MAX (0 dB)
 *
 * ── Controls ─────────────────────────────────────────────────────────────
 *  • Mouse wheel over the main area  → zoom in/out (zoom point at cursor Y)
 *  • Shift + left-drag               → pan Y
 *  • Left-drag (no shift)            → seek playhead (Spectro / Amplitude)
 *  • Zoom panel (right edge, 42 px wide):
 *      [+]  zoom in        [−]  zoom out       [⊙] reset
 *      vertical slider thumb → pan
 *
 * ── Timeline strip (bottom) ──────────────────────────────────────────────
 *  When video thumbnails are provided the strip is THUMB_H + AXIS_H px tall.
 */
class SpectrogramWidget : public QWidget
{
    Q_OBJECT

public:
    // ── FFT parameters ────────────────────────────────────────────────────────
    static constexpr int FFT_SIZE  = 2048;
    static constexpr int HOP_SIZE  = 512;
    static constexpr int FREQ_BINS = FFT_SIZE / 2 + 1;

    static constexpr float MAX_ZOOM = 32.f;

    enum class DisplayMode { Spectrogram, Frequency, Amplitude };

    explicit SpectrogramWidget(QWidget *parent = nullptr);
    ~SpectrogramWidget() override;

    // ── Audio ─────────────────────────────────────────────────────────────────
    void setAudioData(const std::vector<float> &samples,
                      int sampleRate, int channels);

    // ── Video thumbnails ──────────────────────────────────────────────────────
    void setThumbnails(const QList<QPair<double, QImage>> &thumbs);
    void clearThumbnails();

    void clear();

    // ── Transport ─────────────────────────────────────────────────────────────
    void setPlaybackPosition(double seconds);

    // ── Mode ──────────────────────────────────────────────────────────────────
    void        setDisplayMode(DisplayMode m);
    DisplayMode displayMode()  const { return m_mode; }
    void        setLogFrequency(bool on);
    bool        logFrequency()  const { return m_logFreq; }

    // ── Y-zoom API ────────────────────────────────────────────────────────────
    void  resetYZoom();
    void  applyZoom(float newZoom, double pivotNorm = 0.5);
    float yZoom() const { return m_yZoom; }
    float yPan()  const { return m_yPan;  }

    double duration() const { return m_duration; }

    // ── Layout ────────────────────────────────────────────────────────────────
    static constexpr int AXIS_H       = 24;
    static constexpr int THUMB_H      = 52;
    static constexpr int AXIS_LEFT_DB = 50;
    static constexpr int PLAYHEAD_HIT =  8;
    static constexpr int ZOOM_W       = 42;   ///< zoom panel width (px)
signals:
    void seekRequested(double seconds);
    void yZoomChanged(float zoom, float pan);   ///< emitted whenever zoom/pan changes

protected:
    void paintEvent        (QPaintEvent  *) override;
    void mousePressEvent   (QMouseEvent  *) override;
    void mouseMoveEvent    (QMouseEvent  *) override;
    void mouseReleaseEvent (QMouseEvent  *) override;
    void wheelEvent        (QWheelEvent  *) override;
    void resizeEvent       (QResizeEvent *) override;

private:
    // ── DSP ───────────────────────────────────────────────────────────────────
    void computeSpectrogram();
    void buildSpectrogramImage();
    void buildRmsEnvelope();
    void refreshFreqBuffers(double seconds);

    int         timeToColumn(double seconds) const;
    static float hann(int n, int N);
    static QRgb  dbToColor(float db, float minDb, float maxDb);

    // ── Painters ──────────────────────────────────────────────────────────────
    void paintSpectrogram(QPainter &p);
    void paintFrequency  (QPainter &p);
    void paintAmplitude  (QPainter &p);
    void drawTimelineStrip(QPainter &p, int y);
    void drawPlayhead     (QPainter &p, int x, int topY, int bottomY);
    void drawZoomPanel    (QPainter &p);

    // ── Zoom panel hit-testing ────────────────────────────────────────────────
    enum class ZoomHit { None, BtnPlus, BtnMinus, BtnReset, SliderTrack, SliderThumb };
    ZoomHit  zoomHitTest(const QPoint &pos) const;
    QRect    zoomPanelRect()  const;
    QRect    zoomSliderTrack()const;   ///< full slider track rect (inside panel)
    QRect    zoomThumbRect()  const;   ///< draggable thumb rect
    int      zoomBtnH()       const { return 28; }  ///< height of +/−/reset buttons

    // ── Coordinate helpers ────────────────────────────────────────────────────
    int    mainWidth()    const;   ///< widget width minus zoom panel
    int    mainHeight()   const;   ///< widget height minus timeline strip
    int    timelineHeight() const;

    int    timeToPixelX (double t)                  const;
    double pixelXToTime (int    x)                  const;
    int    freqToPixelX (double hz, const QRect &r) const;
    double pixelXToFreq (int    x,  const QRect &r) const;

    /** Convert a normalised Y value in [0,1] to a pixel Y in the plot rect.
     *  norm=0 → bottom of plot, norm=1 → top. Applies current zoom/pan. */
    int    normToPixelY (double norm, const QRect &plotRect) const;
    double pixelYToNorm (int    y,    const QRect &plotRect) const;

    /** Clamp m_yPan so the visible window stays inside [0,1]. */
    void   clampPan();

    // ── Audio data ────────────────────────────────────────────────────────────
    std::vector<float>               m_mono;
    int                              m_sampleRate = 0;
    double                           m_duration   = 0.0;

    std::vector<std::vector<float>>  m_spec;
    int   m_numCols   = 0;
    int   m_numRows   = 0;
    float m_specMinDb = -120.f;
    float m_specMaxDb =    0.f;

    QImage             m_spectrogramImage;
    std::vector<float> m_rmsEnv;

    // ── Frequency buffers ─────────────────────────────────────────────────────
    std::vector<float> m_smoothSpec;
    std::vector<float> m_peakHold;
    std::vector<int>   m_peakTimer;

    // ── Video thumbnails ──────────────────────────────────────────────────────
    QList<QPair<double, QImage>> m_thumbnails;
    bool                         m_hasThumbs = false;

    // ── State ─────────────────────────────────────────────────────────────────
    double      m_playheadPos = 0.0;
    bool        m_dragging    = false;   ///< seek drag (playhead)
    bool        m_panDragging = false;   ///< Y-pan drag (shift+drag or thumb drag)
    int         m_panDragStartY = 0;
    float       m_panDragStartPan = 0.f;
    bool        m_thumbDragging = false;
    int         m_thumbDragStartY = 0;
    float       m_thumbDragStartPan = 0.f;

    DisplayMode m_mode     = DisplayMode::Spectrogram;
    bool        m_logFreq  = true;

    // ── Y-zoom state ──────────────────────────────────────────────────────────
    float m_yZoom = 1.f;    ///< 1 = full range visible
    float m_yPan  = 0.f;    ///< normalized bottom of visible window [0,1-1/zoom]

    // ── FFTW ─────────────────────────────────────────────────────────────────
    float         *m_fftwIn  = nullptr;
    fftwf_complex *m_fftwOut = nullptr;
    fftwf_plan     m_fftwPlan{};

};
