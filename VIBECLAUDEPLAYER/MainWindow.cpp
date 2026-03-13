#include "MainWindow.h"
#include "AudioDecoder.h"
#include "AudioPlayer.h"
#include "VideoDecoder.h"
#include "VideoPlayerWidget.h"
#include "SpectrogramWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QProgressBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QApplication>
#include <QSplitter>
#include <QtConcurrent/QtConcurrent>
#include <QKeyEvent>
#include <QDebug>
#include <cmath>
#include "PreferencesDialog.h"

// ════════════════════════════════════════════════════════════════════════════

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Spectrogram Viewer");
    resize(1100, 620);

    m_audioDecoder = new AudioDecoder(this);
    m_player       = new AudioPlayer(this);
    m_videoDecoder = new VideoDecoder(this);

    setupUi();
    setupMenuBar();
    connectSignals();
    updateTransportButtons();
    updateModeButtons();
}

MainWindow::~MainWindow()
{
    m_player->stop();
}

// ════════════════════════════════════════════════════════════════════════════
//  Style helpers
// ════════════════════════════════════════════════════════════════════════════

QString MainWindow::segBtnStyle(bool active, const QString &accent)
{
    QString tpl = R"(
        QPushButton {
            border: 1px solid %1; border-radius: 5px;
            padding: 3px 10px; font-size: 12px; font-weight: %2;
            color: %3; background: %4;
        }
        QPushButton:hover   { background: %5; }
        QPushButton:pressed { background: %6; }
    )";
    return active
        ? tpl.arg(accent).arg("bold").arg("#fff").arg("#243060").arg("#2c3a78").arg("#1a2248")
        : tpl.arg("#38384a").arg("normal").arg("#7a7a8e").arg("#18181f").arg("#22222d").arg("#10101a");
}

// ════════════════════════════════════════════════════════════════════════════
//  UI build
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::setupUi()
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    central->setStyleSheet("background: #12121a;");

    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(8, 6, 8, 4);
    root->setSpacing(5);

    // ── Top bar (file name + mode switcher) ───────────────────────────────────
    {
        auto *topBar = new QHBoxLayout;
        topBar->setSpacing(6);

        m_lblFile = new QLabel("Aucun fichier chargé", this);
        m_lblFile->setStyleSheet("color:#666; font-style:italic; font-size:11px;");
        topBar->addWidget(m_lblFile, 1);

        auto *lbl = new QLabel("Vue :", this);
        lbl->setStyleSheet("color:#555; font-size:11px;");
        topBar->addWidget(lbl);

        m_btnModeSpectro = new QPushButton("🌈 Spectrogramme", this);
        m_btnModeFreq    = new QPushButton("📊 Fréquence",      this);
        m_btnModeAmp     = new QPushButton("〰 Amplitude",       this);
        m_btnLogLin      = new QPushButton("Log",               this);

        for (auto *b : {m_btnModeSpectro, m_btnModeFreq, m_btnModeAmp})
            b->setFixedHeight(28);
        m_btnLogLin->setFixedSize(44, 28);
        m_btnLogLin->setToolTip("Axe log / linéaire — mode Fréquence (Alt+L)");

        topBar->addWidget(m_btnModeSpectro);
        topBar->addWidget(m_btnModeFreq);
        topBar->addWidget(m_btnModeAmp);
        topBar->addSpacing(6);
        topBar->addWidget(m_btnLogLin);

        m_btnModeSpectro->setFocusPolicy(Qt::NoFocus);
        m_btnModeFreq->setFocusPolicy(Qt::NoFocus);
        m_btnModeAmp->setFocusPolicy(Qt::NoFocus);
        m_btnLogLin->setFocusPolicy(Qt::NoFocus);

        root->addLayout(topBar);
    }

    // ── Main area: video player (left) + spectrogram (right) via splitter ─────
    m_splitter = new QSplitter(Qt::Vertical, this);
    m_splitter->setStyleSheet(
        "QSplitter::handle { background: #252530; width: 4px; }");

    // Video player (hidden until a video is loaded)
    m_videoPlayer = new VideoPlayerWidget(m_splitter);
    m_videoPlayer->setMinimumWidth(180);
    m_videoPlayer->hide();

    // Spectrogram / freq / amplitude widget
    m_spectrogramWidget = new SpectrogramWidget(m_splitter);
    m_spectrogramWidget->setMinimumHeight(100);

    m_splitter->addWidget(m_videoPlayer);
    m_splitter->addWidget(m_spectrogramWidget);
    m_splitter->setStretchFactor(0, 2);   // video: 2 parts
    m_splitter->setStretchFactor(1, 3);   // spectrogram: 3 parts

    root->addWidget(m_splitter, 1);

    // ── Loading progress ──────────────────────────────────────────────────────
    m_progress = new QProgressBar(this);
    m_progress->setRange(0,100);
    m_progress->setFixedHeight(4);
    m_progress->setTextVisible(false);
    m_progress->setStyleSheet(
        "QProgressBar { background:#1a1a24; border:none; }"
        "QProgressBar::chunk { background:#4c9eed; border-radius:2px; }");
    m_progress->hide();
    root->addWidget(m_progress);

    // ── Transport ─────────────────────────────────────────────────────────────
    {
        auto *transport = new QHBoxLayout;
        transport->setSpacing(6);

        auto mkBtn = [&](const QString &label, const QString &clr) {
            auto *b = new QPushButton(label, this);
            b->setFixedSize(48, 38);
            b->setStyleSheet(QString(R"(
                QPushButton {
                    background:#1e1e28; border:1px solid #3a3a4a;
                    border-radius:6px; font-size:17px; color:%1;
                }
                QPushButton:hover   { background:#28283a; }
                QPushButton:pressed { background:#0e0e18; }
                QPushButton:disabled{ color:#2e2e3e; border-color:#242430; }
            )").arg(clr));
            return b;
        };

        m_btnPlay  = mkBtn("▶", "#5ccc6c");
        m_btnPause = mkBtn("⏸", "#e8c040");
        m_btnStop  = mkBtn("■", "#e84040");


        m_btnPlay ->setFocusPolicy(Qt::NoFocus);
        m_btnPause->setFocusPolicy(Qt::NoFocus);
        m_btnStop->setFocusPolicy(Qt::NoFocus);
        transport->addWidget(m_btnPlay);
        transport->addWidget(m_btnPause);
        transport->addWidget(m_btnStop);
        transport->addSpacing(14);

        m_lblTime = new QLabel("0:00.00 / 0:00.00", this);
        m_lblTime->setStyleSheet("color:#9090b0; font-family:Monospace; font-size:12px;");
        m_lblTime->setFixedWidth(175);
        transport->addWidget(m_lblTime);
        transport->addStretch();

        root->addLayout(transport);
    }

    // ── Status bar ────────────────────────────────────────────────────────────
    statusBar()->setStyleSheet("QStatusBar{background:#0c0c14;color:#585868;font-size:11px;}");
    statusBar()->showMessage("Prêt — Fichier > Ouvrir pour charger un fichier audio ou vidéo.");
}

void MainWindow::setupMenuBar()
{
    menuBar()->setStyleSheet(
        "QMenuBar{background:#0c0c14;color:#999;}"
        "QMenuBar::item:selected{background:#24243a;}"
        "QMenu{background:#1a1a28;color:#ccc;border:1px solid #333;}"
        "QMenu::item:selected{background:#34344e;}");

    auto *fileMenu = menuBar()->addMenu("&Fichier");
    auto *openAct  = new QAction("&Ouvrir…", this);
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::onLoadFile);
    fileMenu->addAction(openAct);
    fileMenu->addSeparator();
    auto *prefAct = new QAction("⚙  Préférences…\tCtrl+,", this);
    prefAct->setShortcut(QKeySequence("Ctrl+,"));
    connect(prefAct, &QAction::triggered, this, &MainWindow::onOpenPreferences);
    fileMenu->addAction(prefAct);
    fileMenu->addSeparator();
    auto *quitAct = new QAction("&Quitter", this);
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(quitAct);

    auto *viewMenu  = menuBar()->addMenu("&Vue");
    auto *aSpectro  = new QAction("Spectrogramme\tAlt+S", this);
    aSpectro->setShortcut(QKeySequence("Alt+S"));
    connect(aSpectro, &QAction::triggered, this, &MainWindow::onModeSpectro);
    viewMenu->addAction(aSpectro);

    auto *aFreq = new QAction("Fréquence\tAlt+F", this);
    aFreq->setShortcut(QKeySequence("Alt+F"));
    connect(aFreq, &QAction::triggered, this, &MainWindow::onModeFreq);
    viewMenu->addAction(aFreq);

    auto *aAmp = new QAction("Amplitude\tAlt+A", this);
    aAmp->setShortcut(QKeySequence("Alt+A"));
    connect(aAmp, &QAction::triggered, this, &MainWindow::onModeAmp);
    viewMenu->addAction(aAmp);

    viewMenu->addSeparator();
    auto *aLog = new QAction("Axe log / linéaire\tAlt+L", this);
    aLog->setShortcut(QKeySequence("Alt+L"));
    connect(aLog, &QAction::triggered, this, &MainWindow::onToggleLogLin);
    viewMenu->addAction(aLog);
}

void MainWindow::connectSignals()
{
    connect(m_btnPlay,  &QPushButton::clicked, this, &MainWindow::onPlay);
    connect(m_btnPause, &QPushButton::clicked, this, &MainWindow::onPause);
    connect(m_btnStop,  &QPushButton::clicked, this, &MainWindow::onStop);

    connect(m_btnModeSpectro, &QPushButton::clicked, this, &MainWindow::onModeSpectro);
    connect(m_btnModeFreq,    &QPushButton::clicked, this, &MainWindow::onModeFreq);
    connect(m_btnModeAmp,     &QPushButton::clicked, this, &MainWindow::onModeAmp);
    connect(m_btnLogLin,      &QPushButton::clicked, this, &MainWindow::onToggleLogLin);

    connect(m_player, &AudioPlayer::positionChanged,
            this, &MainWindow::onPositionChanged);
    connect(m_player, &AudioPlayer::playbackFinished,
            this, &MainWindow::onPlaybackFinished);
    connect(m_player, &AudioPlayer::stateChanged,
            this, [this](AudioPlayer::State){ updateTransportButtons(); });

    connect(m_spectrogramWidget, &SpectrogramWidget::seekRequested,
            this, &MainWindow::onSeekRequested);

    connect(m_audioDecoder, &AudioDecoder::loadProgress,
            this, &MainWindow::onLoadProgress);
    connect(m_audioDecoder, &AudioDecoder::loadFinished,
            this, &MainWindow::onLoadFinished);

    // Y-zoom changes \u2192 update status bar
    connect(m_spectrogramWidget, &SpectrogramWidget::yZoomChanged,
            this, [this](float zoom, float /*pan*/){
                if (zoom > 1.f)
                    statusBar()->showMessage(
                        QString("Zoom Y \u00d7%1  \u2014  molette pour zoomer, Shift+glisser pour d\u00e9placer, \u229a pour r\u00e9initialiser")
                        .arg(zoom, 0, 'f', 1));
            });

    // Thumbnails arrive from background thread → queued connection (auto-default)
    connect(m_videoDecoder, &VideoDecoder::thumbnailsReady,
            this, &MainWindow::onThumbnailsReady, Qt::QueuedConnection);
}

// ════════════════════════════════════════════════════════════════════════════
//  Load
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::onLoadFile()
{
    AppPrefs prefs = AppPrefs::load();
    QString startDir = prefs.rememberLastDir
                     ? prefs.lastOpenDir
                     : QString();

    QString path = QFileDialog::getOpenFileName(
        this,
        "Ouvrir un fichier audio / vidéo",
        startDir,
        "Audio/Vidéo (*.wav *.aif *.aiff *.flac *.ogg *.mp3 *.mp4 *.m4a *.aac *.mov *.mkv *.avi *.webm);;"
        "Tous les fichiers (*)");

    if (!path.isEmpty())
        loadFile(path);
}

void MainWindow::onLoadProgress(int pct) { m_progress->setValue(pct); }

void MainWindow::onLoadFinished(bool ok, const QString &error)
{
    m_progress->hide();
    if (!ok)
    {
        statusBar()->showMessage("Erreur : " + error);
        QMessageBox::critical(this, "Erreur de chargement", error);
        return;
    }

    m_player->setSamples(&m_audioDecoder->samples(),
                         m_audioDecoder->sampleRate(),
                         m_audioDecoder->channels());

    m_spectrogramWidget->setAudioData(m_audioDecoder->samples(),
                                      m_audioDecoder->sampleRate(),
                                      m_audioDecoder->channels());

    double dur = m_audioDecoder->duration();
    statusBar()->showMessage(
        QString("%1  |  %2 Hz  |  %3 ch  |  %4%5")
            .arg(QFileInfo(m_audioDecoder->filePath()).fileName())
            .arg(m_audioDecoder->sampleRate())
            .arg(m_audioDecoder->channels())
            .arg(formatTime(dur))
            .arg(m_videoDecoder->hasVideo() ? "  |  🎬 vidéo" : ""));

    m_lblTime->setText("0:00.00 / " + formatTime(dur));
    updateTransportButtons();
}

void MainWindow::onThumbnailsReady(QList<QPair<double, QImage>> thumbs)
{
    m_spectrogramWidget->setThumbnails(thumbs);
    statusBar()->showMessage(
        QString("%1 vignettes générées").arg(thumbs.size()));
}

// ════════════════════════════════════════════════════════════════════════════
//  Transport
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::onPlay()  { m_player->play();  updateTransportButtons(); }
void MainWindow::onPause() { m_player->pause(); updateTransportButtons(); }

void MainWindow::onStop()
{
    m_player->stop();
    m_spectrogramWidget->setPlaybackPosition(0.0);
    updateVideoFrame(0.0);
    m_lblTime->setText("0:00.00 / " + formatTime(m_audioDecoder->duration()));
    updateTransportButtons();
}

void MainWindow::onPositionChanged(double seconds)
{
    m_spectrogramWidget->setPlaybackPosition(seconds);
    updateVideoFrame(seconds);
    m_lblTime->setText(formatTime(seconds) + " / " +
                       formatTime(m_audioDecoder->duration()));
}

void MainWindow::onSeekRequested(double seconds)
{
    m_player->seek(seconds);
    updateVideoFrame(seconds);
    m_lblTime->setText(formatTime(seconds) + " / " +
                       formatTime(m_audioDecoder->duration()));
}

void MainWindow::onPlaybackFinished()
{
    m_spectrogramWidget->setPlaybackPosition(0.0);
    updateVideoFrame(0.0);
    updateTransportButtons();
    statusBar()->showMessage("Lecture terminée.");
}

// ════════════════════════════════════════════════════════════════════════════
//  Video frame update
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::updateVideoFrame(double seconds)
{
    if (m_videoDecoder->hasVideo() && m_videoPlayer->isVisible())
        m_videoPlayer->setPosition(seconds);
}

// ════════════════════════════════════════════════════════════════════════════
//  Mode switcher
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::onModeSpectro()
{
    m_spectrogramWidget->setDisplayMode(SpectrogramWidget::DisplayMode::Spectrogram);
    updateModeButtons();
    statusBar()->showMessage("Vue : Spectrogramme");
}

void MainWindow::onModeFreq()
{
    m_spectrogramWidget->setDisplayMode(SpectrogramWidget::DisplayMode::Frequency);
    updateModeButtons();
    statusBar()->showMessage("Vue : Spectre de fréquences");
}

void MainWindow::onModeAmp()
{
    m_spectrogramWidget->setDisplayMode(SpectrogramWidget::DisplayMode::Amplitude);
    updateModeButtons();
    statusBar()->showMessage("Vue : Amplitude");
}

void MainWindow::onToggleLogLin()
{
    bool nl = !m_spectrogramWidget->logFrequency();
    m_spectrogramWidget->setLogFrequency(nl);
    m_btnLogLin->setText(nl ? "Log" : "Lin");
    updateModeButtons();
    statusBar()->showMessage(nl ? "Axe fréquence : logarithmique"
                                : "Axe fréquence : linéaire");
}

// ════════════════════════════════════════════════════════════════════════════
//  Helpers
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::updateTransportButtons()
{
    bool loaded  = m_audioDecoder->isLoaded();
    bool playing = m_player->isPlaying();
    bool paused  = m_player->isPaused();
    m_btnPlay ->setEnabled(loaded && !playing);
    m_btnPause->setEnabled(loaded &&  playing);
    m_btnStop ->setEnabled(loaded && (playing || paused));
}

void MainWindow::updateModeButtons()
{
    using DM = SpectrogramWidget::DisplayMode;
    DM cur = m_spectrogramWidget->displayMode();
    m_btnModeSpectro->setStyleSheet(segBtnStyle(cur==DM::Spectrogram, "#6a9eff"));
    m_btnModeFreq   ->setStyleSheet(segBtnStyle(cur==DM::Frequency,   "#ffb040"));
    m_btnModeAmp    ->setStyleSheet(segBtnStyle(cur==DM::Amplitude,   "#60d0ff"));

    bool freqMode = (cur == DM::Frequency);
    m_btnLogLin->setEnabled(freqMode);
    m_btnLogLin->setText(m_spectrogramWidget->logFrequency() ? "Log" : "Lin");
    m_btnLogLin->setStyleSheet(
        freqMode
        ? segBtnStyle(m_spectrogramWidget->logFrequency(), "#a0a0ff")
        : "QPushButton{background:#14141c;border:1px solid #28283a;"
          "border-radius:5px;color:#333340;padding:3px 8px;font-size:12px;}");
}

// ════════════════════════════════════════════════════════════════════════════
//  Public: load file (used by CLI argument)
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::loadFile(const QString &path)
{
    if (path.isEmpty()) return;

    m_player->stop();
    m_spectrogramWidget->clear();
    m_videoPlayer->clear();
    m_videoPlayer->hide();
    m_videoDecoder->clear();
    m_progress->setValue(0);
    m_progress->show();

    QString name = QFileInfo(path).fileName();
    m_lblFile->setText(name);
    statusBar()->showMessage(QString("Chargement de %1…").arg(name));

    // Remember directory in prefs
    AppPrefs prefs = AppPrefs::load();
    if (prefs.rememberLastDir)
    {
        prefs.lastOpenDir = QFileInfo(path).absolutePath();
        prefs.save();
    }

    bool hasVideo = m_videoDecoder->loadFile(path);
    if (hasVideo)
    {
        m_videoPlayer->setVideoDecoder(m_videoDecoder);
        m_videoPlayer->setInfoText(name);
        m_videoPlayer->show();
        m_videoPlayer->setPosition(0.0);

        AppPrefs p = AppPrefs::load();
        int thumbCount = std::max(4, std::min(p.thumbCount,
            int(m_videoDecoder->duration() / 5.0) + 1));
        m_videoDecoder->generateThumbnailsAsync(thumbCount, p.thumbHeight);

        statusBar()->showMessage(
            QString("Vidéo détectée (%1×%2, %3 fps) — génération des vignettes…")
                .arg(m_videoDecoder->frameWidth())
                .arg(m_videoDecoder->frameHeight())
                .arg(m_videoDecoder->fps(), 0, 'f', 1));
    }
    else
    {
        m_videoPlayer->hide();
    }

    QtConcurrent::run([this, path]() {
        m_audioDecoder->loadFile(path);
    });
}

// ════════════════════════════════════════════════════════════════════════════
//  Keyboard navigation
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (!m_audioDecoder->isLoaded())
    {
        QMainWindow::keyPressEvent(event);
        return;
    }

    switch (event->key())
    {
    case Qt::Key_Left:
        seekBy(-(AppPrefs::load().arrowStepSec));
        break;

    case Qt::Key_Right:
        seekBy(+(AppPrefs::load().arrowStepSec));
        break;

    case Qt::Key_Space:
        if (m_player->isPlaying()) onPause();
        else                       onPlay();
        break;

    case Qt::Key_Escape:
        onStop();
        break;

    default:
        QMainWindow::keyPressEvent(event);
        break;
    }
}

void MainWindow::seekBy(double deltaSec)
{
    if (!m_audioDecoder->isLoaded()) return;

    double newPos = m_player->currentPositionSeconds() + deltaSec;
    double dur    = m_audioDecoder->duration();
    AppPrefs p    = AppPrefs::load();

    if (p.arrowLoops)
        newPos = std::fmod(newPos + dur, dur);
    else
        newPos = std::clamp(newPos, 0.0, dur);

    m_player->seek(newPos);
    m_spectrogramWidget->setPlaybackPosition(newPos);
    updateVideoFrame(newPos);
    m_lblTime->setText(formatTime(newPos) + " / " + formatTime(dur));

    // Status bar hint
    statusBar()->showMessage(
        QString("Position : %1  (← → : %2 s par pas)")
            .arg(formatTime(newPos))
            .arg(p.arrowStepSec, 0, 'f', 1), 2000);
}

// ════════════════════════════════════════════════════════════════════════════
//  Preferences
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::onOpenPreferences()
{
    PreferencesDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted)
        applyPreferences();
}

void MainWindow::applyPreferences()
{
    AppPrefs p = AppPrefs::load();

    // Volume (simple linear scale)
    // AudioPlayer doesn't yet have volume, but we store it for future use.
    // statusBar hint
    statusBar()->showMessage(
        QString("Préférences appliquées — pas flèche: %1 s  |  vignettes: %2")
            .arg(p.arrowStepSec, 0, 'f', 1)
            .arg(p.thumbCount), 3000);
}

QString MainWindow::formatTime(double seconds) const
{
    if (seconds < 0) seconds = 0;
    int m = int(seconds)/60, s = int(seconds)%60;
    int cs = int((seconds - std::floor(seconds))*100);
    return QString("%1:%2.%3").arg(m).arg(s,2,10,QChar('0')).arg(cs,2,10,QChar('0'));
}
