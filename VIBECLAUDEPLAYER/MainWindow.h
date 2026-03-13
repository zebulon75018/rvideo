#pragma once

#include <QMainWindow>
#include <QString>

#include "SpectrogramWidget.h"
#include "PreferencesDialog.h"

class AudioDecoder;
class AudioPlayer;
class VideoDecoder;
class VideoPlayerWidget;
class QPushButton;
class QLabel;
class QProgressBar;
class QSplitter;

/**
 * MainWindow
 * ----------
 * Main application window.
 *
 * New features:
 *   • loadFile(path)         — public, used by main() for CLI argument
 *   • keyPressEvent          — ← / → move playhead by arrowStepSec
 *                              Space = play/pause,  Escape = stop
 *   • Fichier > Préférences  — opens PreferencesDialog
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /** Load a file programmatically (e.g. from command-line argument). */
    void loadFile(const QString &path);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onLoadFile();
    void onPlay();
    void onPause();
    void onStop();
    void onPositionChanged(double seconds);
    void onSeekRequested(double seconds);
    void onPlaybackFinished();
    void onLoadProgress(int percent);
    void onLoadFinished(bool ok, const QString &error);
    void onThumbnailsReady(QList<QPair<double,QImage>> thumbs);

    void onModeSpectro();
    void onModeFreq();
    void onModeAmp();
    void onToggleLogLin();
    void onOpenPreferences();

private:
    void setupUi();
    void setupMenuBar();
    void connectSignals();
    void updateTransportButtons();
    void updateModeButtons();
    void updateVideoFrame(double seconds);
    void seekBy(double deltaSec);       ///< relative seek (arrow keys)
    void applyPreferences();            ///< re-read QSettings and push to widgets
    QString formatTime(double seconds) const;
    static QString segBtnStyle(bool active, const QString &accent = "#5b8dee");

    // ── Sub-systems ──────────────────────────────────────────────────────────
    AudioDecoder      *m_audioDecoder = nullptr;
    AudioPlayer       *m_player       = nullptr;
    VideoDecoder      *m_videoDecoder = nullptr;

    // ── Widgets ───────────────────────────────────────────────────────────────
    QSplitter         *m_splitter          = nullptr;
    VideoPlayerWidget *m_videoPlayer       = nullptr;
    SpectrogramWidget *m_spectrogramWidget = nullptr;

    // Transport
    QPushButton  *m_btnPlay    = nullptr;
    QPushButton  *m_btnPause   = nullptr;
    QPushButton  *m_btnStop    = nullptr;
    QLabel       *m_lblTime    = nullptr;
    QLabel       *m_lblFile    = nullptr;
    QProgressBar *m_progress   = nullptr;

    // Mode switcher
    QPushButton  *m_btnModeSpectro = nullptr;
    QPushButton  *m_btnModeFreq    = nullptr;
    QPushButton  *m_btnModeAmp     = nullptr;
    QPushButton  *m_btnLogLin      = nullptr;
};
