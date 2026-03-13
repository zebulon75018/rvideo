#pragma once

#include <QMainWindow>
#include <QString>

#include "SpectrogramWidget.h"

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
 * Main test application.
 *
 * Layout (when a video file is loaded):
 *
 *   ┌─────────────────────────────────────────┐
 *   │  top bar  (file name | mode switcher)   │
 *   ├───────────────────┬─────────────────────┤
 *   │  VideoPlayerWidget│                     │
 *   │  (video frames)   │  SpectrogramWidget  │
 *   │                   │  (spectrogram /     │
 *   │                   │   frequency /       │
 *   │                   │   amplitude)        │
 *   │                   │  ┌─────────────────┐│
 *   │                   │  │thumbnail strip  ││
 *   │                   │  └─────────────────┘│
 *   ├───────────────────┴─────────────────────┤
 *   │  transport  ▶ ⏸ ■   time label         │
 *   └─────────────────────────────────────────┘
 *
 * When an audio-only file is loaded the VideoPlayerWidget is hidden
 * and the QSplitter collapses to a single column.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

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

private:
    void setupUi();
    void setupMenuBar();
    void connectSignals();
    void updateTransportButtons();
    void updateModeButtons();
    void updateVideoFrame(double seconds);
    QString formatTime(double seconds) const;
    static QString segBtnStyle(bool active, const QString &accent = "#5b8dee");

    // ── Sub-systems ──────────────────────────────────────────────────────────
    AudioDecoder      *m_audioDecoder = nullptr;
    AudioPlayer       *m_player       = nullptr;
    VideoDecoder      *m_videoDecoder = nullptr;

    // ── Widgets ───────────────────────────────────────────────────────────────
    QSplitter         *m_splitter         = nullptr;
    VideoPlayerWidget *m_videoPlayer      = nullptr;
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
