#pragma once

#include <QDialog>
#include <QSettings>

class QTabWidget;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QSlider;
class QLabel;
class QLineEdit;
class QDialogButtonBox;
class QGroupBox;

/**
 * PreferencesDialog
 * -----------------
 * Modal dialog accessed via Fichier > Préférences.
 * Persists all settings via QSettings (INI file, same directory as the app).
 *
 * ── Tabs ──────────────────────────────────────────────────────────────────
 *   Général      – language hints, recent files, default folder
 *   Analyse      – FFT size, window function, color map, dB range
 *   Lecture      – arrow key step, PortAudio device, volume
 *   Vidéo        – thumbnail count, thumbnail height, player position
 *   Interface    – zoom speed, timeline height, show peak-hold
 *
 * Usage:
 *   AppPrefs prefs = AppPrefs::load();
 *   ...
 *   PreferencesDialog dlg(this);
 *   if (dlg.exec() == QDialog::Accepted)
 *       AppPrefs prefs = AppPrefs::load();  // reload after save
 */

// ── Flat preference bag (POD-friendly, loaded/saved via QSettings) ──────────
struct AppPrefs
{
    // ── Général ───────────────────────────────────────────────────────────────
    QString lastOpenDir;           ///< remembered open-file directory
    bool    rememberLastDir = true;

    // ── Analyse ───────────────────────────────────────────────────────────────
    int     fftSize      = 2048;   ///< 512 / 1024 / 2048 / 4096
    int     hopSize      = 512;    ///< frame step in samples
    int     windowFunc   = 0;      ///< 0=Hann, 1=Hamming, 2=Blackman, 3=Rect
    int     colorMap     = 0;      ///< 0=Inferno, 1=Viridis, 2=Hot, 3=Cool
    float   specDbMin    = -90.f;  ///< lower dB clamp for color mapping
    float   specDbMax    =   0.f;  ///< upper dB clamp

    // ── Lecture ───────────────────────────────────────────────────────────────
    double  arrowStepSec = 0.5;    ///< seconds moved per arrow key press
    bool    arrowLoops   = false;  ///< wrap around at end/start
    int     masterVolume = 100;    ///< 0–100 %
    bool    autoPlay     = false;  ///< auto-start on file load

    // ── Vidéo ─────────────────────────────────────────────────────────────────
    int     thumbCount   = 80;     ///< max thumbnails to generate
    int     thumbHeight  = 52;     ///< thumbnail strip height in px
    int     videoPos     = 0;      ///< 0=left panel, 1=floating window (future)

    // ── Interface ─────────────────────────────────────────────────────────────
    float   zoomStep     = 1.25f;  ///< wheel zoom step factor (e.g. 1.25 = 25%)
    bool    showPeakHold = true;
    bool    showRmsEnv   = true;
    int     timelineH    = 52;     ///< thumbnail strip height (mirrors thumbHeight)

    static AppPrefs load();
    void            save() const;
};

// ─────────────────────────────────────────────────────────────────────────────

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent = nullptr);

    /** Returns true if the user accepted and settings were changed. */
    bool settingsChanged() const { return m_changed; }

private slots:
    void onAccepted();
    void onRestoreDefaults();

private:
    void buildUi();
    void buildTabGeneral (QTabWidget *tabs);
    void buildTabAnalyse (QTabWidget *tabs);
    void buildTabLecture (QTabWidget *tabs);
    void buildTabVideo   (QTabWidget *tabs);
    void buildTabInterface(QTabWidget *tabs);

    void loadIntoUi (const AppPrefs &p);
    AppPrefs uiToPrefs() const;

    // ── Général ───────────────────────────────────────────────────────────────
    QLineEdit   *m_edLastDir      = nullptr;
    QCheckBox   *m_chkRememberDir = nullptr;

    // ── Analyse ───────────────────────────────────────────────────────────────
    QComboBox   *m_cmbFftSize   = nullptr;
    QComboBox   *m_cmbHopSize   = nullptr;
    QComboBox   *m_cmbWindow    = nullptr;
    QComboBox   *m_cmbColorMap  = nullptr;
    QDoubleSpinBox *m_spnDbMin  = nullptr;
    QDoubleSpinBox *m_spnDbMax  = nullptr;

    // ── Lecture ───────────────────────────────────────────────────────────────
    QComboBox   *m_cmbArrowStep = nullptr;
    QCheckBox   *m_chkArrowLoop = nullptr;
    QSlider     *m_sldVolume    = nullptr;
    QLabel      *m_lblVolume    = nullptr;
    QCheckBox   *m_chkAutoPlay  = nullptr;

    // ── Vidéo ─────────────────────────────────────────────────────────────────
    QSpinBox    *m_spnThumbCount  = nullptr;
    QSpinBox    *m_spnThumbHeight = nullptr;

    // ── Interface ─────────────────────────────────────────────────────────────
    QComboBox   *m_cmbZoomStep    = nullptr;
    QCheckBox   *m_chkPeakHold    = nullptr;
    QCheckBox   *m_chkRmsEnv      = nullptr;

    QDialogButtonBox *m_buttons = nullptr;
    bool m_changed = false;
};
