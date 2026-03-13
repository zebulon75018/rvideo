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
 * Three visualization modes:
 *   Spectrogram  – 2-D heat-map (time→X, freq→Y, energy→colour)
 *   Frequency    – FFT magnitude spectrum at playhead (FFTW3)
 *   Amplitude    – Full PCM waveform + RMS envelope
 *
 * Timeline strip (bottom):
 *   • When video thumbnails are loaded via setThumbnails(), the strip
 *     grows to THUMB_H + AXIS_H pixels and shows the thumbnail images.
 *   • The draggable playhead works identically in all modes.
 */
class SpectrogramWidget : public QWidget
{
    Q_OBJECT

public:
    // ── FFT parameters ────────────────────────────────────────────────────────
    static constexpr int FFT_SIZE  = 2048;
    static constexpr int HOP_SIZE  = 512;
    static constexpr int FREQ_BINS = FFT_SIZE / 2 + 1;

    enum class DisplayMode { Spectrogram, Frequency, Amplitude };

    explicit SpectrogramWidget(QWidget *parent = nullptr);
    ~SpectrogramWidget() override;

    // ── Audio data ────────────────────────────────────────────────────────────
    void setAudioData(const std::vector<float> &samples,
                      int sampleRate,
                      int channels);

    // ── Video thumbnails (optional) ───────────────────────────────────────────
    /**
     * Set the thumbnail strip for the timeline.
     * @param thumbs  list of (timestamp_seconds, thumbnail_image) pairs
     *                sorted by timestamp ascending.
     */
    void setThumbnails(const QList<QPair<double, QImage>> &thumbs);
    void clearThumbnails();

    void clear();

    // ── Transport ─────────────────────────────────────────────────────────────
    void setPlaybackPosition(double seconds);

    // ── Mode ──────────────────────────────────────────────────────────────────
    void        setDisplayMode(DisplayMode m);
    DisplayMode displayMode()  const { return m_mode; }

    void setLogFrequency(bool on);
    bool logFrequency()        const { return m_logFreq; }

    double duration() const { return m_duration; }
    static constexpr int AXIS_H       = 24;   ///< time-tick label height (px)
    static constexpr int THUMB_H      = 52;   ///< thumbnail height (px)
    static constexpr int AXIS_LEFT_DB = 50;
    static constexpr int PLAYHEAD_HIT =  8;

signals:
    void seekRequested(double seconds);

protected:
    void paintEvent        (QPaintEvent  *) override;
    void mousePressEvent   (QMouseEvent  *) override;
    void mouseMoveEvent    (QMouseEvent  *) override;
    void mouseReleaseEvent (QMouseEvent  *) override;
    void resizeEvent       (QResizeEvent *) override;

private:
    // ── DSP ───────────────────────────────────────────────────────────────────
    void computeSpectrogram();
    void buildSpectrogramImage();
    void buildRmsEnvelope();
    void refreshFreqBuffers(double seconds);

    int    timeToColumn(double seconds) const;
    static float hann(int n, int N);
    static QRgb  dbToColor(float db, float minDb, float maxDb);

    // ── Painters ──────────────────────────────────────────────────────────────
    void paintSpectrogram(QPainter &p);
    void paintFrequency  (QPainter &p);
    void paintAmplitude  (QPainter &p);
    void drawTimelineStrip(QPainter &p, int y);   // thumbnails + time axis
    void drawPlayhead     (QPainter &p, int x, int topY, int bottomY);

    // ── Coordinates ───────────────────────────────────────────────────────────
    int    timeToPixelX (double t)                  const;
    double pixelXToTime (int    x)                  const;
    int    freqToPixelX (double hz, const QRect &r) const;
    double pixelXToFreq (int    x,  const QRect &r) const;

    /** Total height of the bottom timeline strip (time axis + optional thumbs). */
    int timelineHeight() const;

    // ── Audio ─────────────────────────────────────────────────────────────────
    std::vector<float>               m_mono;
    int                              m_sampleRate = 0;
    double                           m_duration   = 0.0;

    std::vector<std::vector<float>>  m_spec;
    int   m_numCols   = 0;
    int   m_numRows   = 0;
    float m_specMinDb = -120.f;
    float m_specMaxDb =    0.f;

    QImage              m_spectrogramImage;
    std::vector<float>  m_rmsEnv;

    // ── Frequency buffers ─────────────────────────────────────────────────────
    std::vector<float>  m_smoothSpec;
    std::vector<float>  m_peakHold;
    std::vector<int>    m_peakTimer;

    // ── Video thumbnails ──────────────────────────────────────────────────────
    QList<QPair<double, QImage>> m_thumbnails;
    bool                         m_hasThumbs = false;

    // ── State ─────────────────────────────────────────────────────────────────
    double      m_playheadPos = 0.0;
    bool        m_dragging    = false;
    DisplayMode m_mode        = DisplayMode::Spectrogram;
    bool        m_logFreq     = true;

    // ── FFTW ─────────────────────────────────────────────────────────────────
    float         *m_fftwIn   = nullptr;
    fftwf_complex *m_fftwOut  = nullptr;
    fftwf_plan     m_fftwPlan{};

    // ── Layout constants ──────────────────────────────────────────────────────
};
