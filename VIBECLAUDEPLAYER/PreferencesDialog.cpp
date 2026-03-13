#include "PreferencesDialog.h"

#include <QTabWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QStandardPaths>
#include <QSettings>
#include <QDir>
#include <QFrame>
#include <QDebug>

// ════════════════════════════════════════════════════════════════════════════
//  AppPrefs  — load / save via QSettings
// ════════════════════════════════════════════════════════════════════════════

static QString settingsPath()
{
    return QDir(QStandardPaths::writableLocation(
                    QStandardPaths::AppConfigLocation))
               .filePath("preferences.ini");
}

AppPrefs AppPrefs::load()
{
    QSettings s(settingsPath(), QSettings::IniFormat);
    AppPrefs p;

    s.beginGroup("General");
    p.lastOpenDir    = s.value("lastOpenDir",
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation)).toString();
    p.rememberLastDir = s.value("rememberLastDir", true).toBool();
    s.endGroup();

    s.beginGroup("Analysis");
    p.fftSize    = s.value("fftSize",   2048).toInt();
    p.hopSize    = s.value("hopSize",    512).toInt();
    p.windowFunc = s.value("windowFunc",   0).toInt();
    p.colorMap   = s.value("colorMap",     0).toInt();
    p.specDbMin  = s.value("specDbMin", -90.f).toFloat();
    p.specDbMax  = s.value("specDbMax",   0.f).toFloat();
    s.endGroup();

    s.beginGroup("Playback");
    p.arrowStepSec = s.value("arrowStepSec", 0.5).toDouble();
    p.arrowLoops   = s.value("arrowLoops",  false).toBool();
    p.masterVolume = s.value("masterVolume",  100).toInt();
    p.autoPlay     = s.value("autoPlay",    false).toBool();
    s.endGroup();

    s.beginGroup("Video");
    p.thumbCount  = s.value("thumbCount",  80).toInt();
    p.thumbHeight = s.value("thumbHeight", 52).toInt();
    p.videoPos    = s.value("videoPos",     0).toInt();
    s.endGroup();

    s.beginGroup("Interface");
    p.zoomStep    = s.value("zoomStep",   1.25f).toFloat();
    p.showPeakHold= s.value("showPeakHold", true).toBool();
    p.showRmsEnv  = s.value("showRmsEnv",   true).toBool();
    p.timelineH   = s.value("timelineH",     52).toInt();
    s.endGroup();

    return p;
}

void AppPrefs::save() const
{
    // Make sure directory exists
    QDir().mkpath(QFileInfo(settingsPath()).absolutePath());

    QSettings s(settingsPath(), QSettings::IniFormat);

    s.beginGroup("General");
    s.setValue("lastOpenDir",    lastOpenDir);
    s.setValue("rememberLastDir",rememberLastDir);
    s.endGroup();

    s.beginGroup("Analysis");
    s.setValue("fftSize",    fftSize);
    s.setValue("hopSize",    hopSize);
    s.setValue("windowFunc", windowFunc);
    s.setValue("colorMap",   colorMap);
    s.setValue("specDbMin",  specDbMin);
    s.setValue("specDbMax",  specDbMax);
    s.endGroup();

    s.beginGroup("Playback");
    s.setValue("arrowStepSec", arrowStepSec);
    s.setValue("arrowLoops",   arrowLoops);
    s.setValue("masterVolume", masterVolume);
    s.setValue("autoPlay",     autoPlay);
    s.endGroup();

    s.beginGroup("Video");
    s.setValue("thumbCount",  thumbCount);
    s.setValue("thumbHeight", thumbHeight);
    s.setValue("videoPos",    videoPos);
    s.endGroup();

    s.beginGroup("Interface");
    s.setValue("zoomStep",     zoomStep);
    s.setValue("showPeakHold", showPeakHold);
    s.setValue("showRmsEnv",   showRmsEnv);
    s.setValue("timelineH",    timelineH);
    s.endGroup();

    s.sync();
}

// ════════════════════════════════════════════════════════════════════════════
//  PreferencesDialog
// ════════════════════════════════════════════════════════════════════════════

// ── Dark style helpers ────────────────────────────────────────────────────

static QGroupBox *makeGroup(const QString &title, QWidget *parent = nullptr)
{
    auto *g = new QGroupBox(title, parent);
    g->setStyleSheet(
        "QGroupBox { border: 1px solid #2e3040; border-radius: 6px;"
        " margin-top: 10px; padding-top: 8px; color: #aab; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px;"
        " padding: 0 4px; color: #7090cc; font-weight: bold; }");
    return g;
}

static QFrame *makeSep()
{
    auto *f = new QFrame;
    f->setFrameShape(QFrame::HLine);
    f->setStyleSheet("color: #2a2c3e;");
    return f;
}

static void styleCombo(QComboBox *c)
{
    c->setStyleSheet(
        "QComboBox { background:#1e2030; border:1px solid #38394a;"
        " border-radius:4px; padding:3px 8px; color:#ccd; min-width:120px; }"
        "QComboBox::drop-down { border:none; }"
        "QComboBox QAbstractItemView { background:#1a1c28; color:#ccd;"
        " selection-background-color:#2a3a6a; }");
}

static void styleSpin(QAbstractSpinBox *s)
{
    s->setStyleSheet(
        "QAbstractSpinBox { background:#1e2030; border:1px solid #38394a;"
        " border-radius:4px; padding:3px 8px; color:#ccd; min-width:80px; }");
}

// ─────────────────────────────────────────────────────────────────────────────

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Préférences — Spectrogram Viewer");
    setMinimumSize(520, 460);
    setModal(true);
    setStyleSheet(
        "QDialog { background:#14151f; color:#ccd; }"
        "QTabWidget::pane { border:1px solid #2a2c3e; background:#14151f; }"
        "QTabBar::tab { background:#1a1c2a; color:#888; padding:6px 16px;"
        "  border:1px solid #2a2c3e; border-bottom:none; border-radius:4px 4px 0 0; }"
        "QTabBar::tab:selected { background:#14151f; color:#aac; }"
        "QLabel { color:#bbc; }"
        "QCheckBox { color:#bbc; spacing:6px; }"
        "QCheckBox::indicator { width:14px; height:14px; border:1px solid #444;"
        "  border-radius:3px; background:#1a1c2a; }"
        "QCheckBox::indicator:checked { background:#3a5acc; border-color:#5070ee; }"
        "QDialogButtonBox QPushButton { background:#1e2030; border:1px solid #38394a;"
        "  border-radius:5px; padding:5px 16px; color:#ccd; min-width:80px; }"
        "QDialogButtonBox QPushButton:hover { background:#262840; }"
        "QDialogButtonBox QPushButton:default { border-color:#4060cc; color:#7099ff; }");

    buildUi();
    loadIntoUi(AppPrefs::load());

    connect(m_buttons, &QDialogButtonBox::accepted, this, &PreferencesDialog::onAccepted);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

// ─────────────────────────────────────────────────────────────────────────────

void PreferencesDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(14, 12, 14, 10);
    root->setSpacing(10);

    // ── Title bar ─────────────────────────────────────────────────────────────
    auto *titleRow = new QHBoxLayout;
    auto *titleLbl = new QLabel("⚙  Préférences", this);
    titleLbl->setStyleSheet("font-size:15px; font-weight:bold; color:#7090cc;");
    titleRow->addWidget(titleLbl);
    titleRow->addStretch();
    auto *restoreBtn = new QPushButton("Restaurer les défauts", this);
    restoreBtn->setStyleSheet(
        "QPushButton{background:#1a1c28;border:1px solid #333;border-radius:4px;"
        "padding:4px 12px;color:#778;font-size:11px;}"
        "QPushButton:hover{background:#22243a;color:#aab;}");
    connect(restoreBtn, &QPushButton::clicked, this, &PreferencesDialog::onRestoreDefaults);
    titleRow->addWidget(restoreBtn);
    root->addLayout(titleRow);
    root->addWidget(makeSep());

    // ── Tabs ──────────────────────────────────────────────────────────────────
    auto *tabs = new QTabWidget(this);
    buildTabGeneral  (tabs);
    buildTabAnalyse  (tabs);
    buildTabLecture  (tabs);
    buildTabVideo    (tabs);
    buildTabInterface(tabs);
    root->addWidget(tabs, 1);

    root->addWidget(makeSep());

    // ── Buttons ───────────────────────────────────────────────────────────────
    m_buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttons->button(QDialogButtonBox::Ok)->setText("Appliquer");
    m_buttons->button(QDialogButtonBox::Cancel)->setText("Annuler");
    m_buttons->button(QDialogButtonBox::Ok)->setDefault(true);
    root->addWidget(m_buttons);
}

// ── Tab: Général ──────────────────────────────────────────────────────────────

void PreferencesDialog::buildTabGeneral(QTabWidget *tabs)
{
    auto *w   = new QWidget;
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 10, 12, 10);
    lay->setSpacing(12);

    auto *grpDir = makeGroup("Dossier par défaut", w);
    auto *fl     = new QFormLayout(grpDir);
    fl->setSpacing(8);

    m_chkRememberDir = new QCheckBox("Mémoriser le dernier dossier ouvert", grpDir);

    auto *dirRow = new QHBoxLayout;
    m_edLastDir = new QLineEdit(grpDir);
    m_edLastDir->setStyleSheet(
        "QLineEdit{background:#1e2030;border:1px solid #38394a;border-radius:4px;"
        "padding:4px 8px;color:#ccd;}");
    auto *browseBtn = new QPushButton("…", grpDir);
    browseBtn->setFixedSize(28, 26);
    browseBtn->setStyleSheet(
        "QPushButton{background:#1e2030;border:1px solid #38394a;border-radius:4px;"
        "color:#aab;font-size:13px;}"
        "QPushButton:hover{background:#262840;}");
    connect(browseBtn, &QPushButton::clicked, this, [this]{
        QString d = QFileDialog::getExistingDirectory(
            this, "Choisir le dossier par défaut", m_edLastDir->text());
        if (!d.isEmpty()) m_edLastDir->setText(d);
    });
    dirRow->addWidget(m_edLastDir, 1);
    dirRow->addWidget(browseBtn);

    fl->addRow("Dossier :", dirRow);
    fl->addRow("",           m_chkRememberDir);

    lay->addWidget(grpDir);

    // ── Info box ──────────────────────────────────────────────────────────────
    auto *infoBox = new QGroupBox("À propos", w);
    //infoBox->setStyleSheet(makeGroup("").styleSheet());
    auto *infoLay = new QVBoxLayout(infoBox);
    auto *info = new QLabel(
        "<b>Spectrogram Viewer</b><br>"
        "Version 1.0<br>"
        "<br>"
        "Libraries: Qt, PortAudio, libsndfile, FFTW3, FFmpeg, OpenCV<br>"
        "<br>"
        "<span style='color:#556;'>Fichier de préférences :</span><br>"
        "<span style='color:#448; font-family:monospace; font-size:10px;'>"
        + settingsPath() + "</span>", infoBox);
    info->setWordWrap(true);
    info->setStyleSheet("color:#778; font-size:11px;");
    infoLay->addWidget(info);

    lay->addWidget(infoBox);
    lay->addStretch();
    tabs->addTab(w, "🗂  Général");
}

// ── Tab: Analyse ──────────────────────────────────────────────────────────────

void PreferencesDialog::buildTabAnalyse(QTabWidget *tabs)
{
    auto *w   = new QWidget;
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 10, 12, 10); lay->setSpacing(12);

    // FFT group
    auto *grpFft = makeGroup("Paramètres FFT", w);
    auto *fl     = new QFormLayout(grpFft); fl->setSpacing(8);

    m_cmbFftSize = new QComboBox(grpFft);
    m_cmbFftSize->addItems({"512", "1024", "2048", "4096"});
    styleCombo(m_cmbFftSize);

    m_cmbHopSize = new QComboBox(grpFft);
    m_cmbHopSize->addItems({"128", "256", "512", "1024", "2048"});
    styleCombo(m_cmbHopSize);

    m_cmbWindow = new QComboBox(grpFft);
    m_cmbWindow->addItems({"Hann", "Hamming", "Blackman", "Rectangulaire"});
    styleCombo(m_cmbWindow);

    auto *noteFFT = new QLabel(
        "⚠  Modifier la taille FFT nécessite de recharger le fichier.", grpFft);
    noteFFT->setStyleSheet("color:#8870a0; font-size:10px;");

    fl->addRow("Taille FFT :",   m_cmbFftSize);
    fl->addRow("Pas (hop) :",    m_cmbHopSize);
    fl->addRow("Fenêtre :",      m_cmbWindow);
    fl->addRow("",               noteFFT);
    lay->addWidget(grpFft);

    // Colour group
    auto *grpCol = makeGroup("Affichage colorimétrique", w);
    auto *fl2    = new QFormLayout(grpCol); fl2->setSpacing(8);

    m_cmbColorMap = new QComboBox(grpCol);
    m_cmbColorMap->addItems({"Inferno (défaut)", "Viridis", "Hot", "Cool", "Gris"});
    styleCombo(m_cmbColorMap);

    m_spnDbMin = new QDoubleSpinBox(grpCol);
    m_spnDbMin->setRange(-160.0, 0.0); m_spnDbMin->setSuffix(" dB");
    m_spnDbMin->setSingleStep(5.0); styleSpin(m_spnDbMin);

    m_spnDbMax = new QDoubleSpinBox(grpCol);
    m_spnDbMax->setRange(-80.0, 0.0); m_spnDbMax->setSuffix(" dB");
    m_spnDbMax->setSingleStep(5.0); styleSpin(m_spnDbMax);

    fl2->addRow("Palette de couleurs :", m_cmbColorMap);
    fl2->addRow("dB minimum :",          m_spnDbMin);
    fl2->addRow("dB maximum :",          m_spnDbMax);
    lay->addWidget(grpCol);
    lay->addStretch();

    tabs->addTab(w, "📊  Analyse");
}

// ── Tab: Lecture ─────────────────────────────────────────────────────────────

void PreferencesDialog::buildTabLecture(QTabWidget *tabs)
{
    auto *w   = new QWidget;
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(12,10,12,10); lay->setSpacing(12);

    // Navigation group
    auto *grpNav = makeGroup("Navigation au clavier", w);
    auto *fl     = new QFormLayout(grpNav); fl->setSpacing(8);

    m_cmbArrowStep = new QComboBox(grpNav);
    m_cmbArrowStep->addItems({
        "0.1 s  (très précis)", "0.5 s  (précis)",
        "1 s    (normal)",      "5 s    (rapide)",
        "10 s   (très rapide)", "30 s   (scène)"});
    styleCombo(m_cmbArrowStep);

    m_chkArrowLoop = new QCheckBox("Boucler en fin / début de fichier", grpNav);

    auto *kbHint = new QLabel(
        "← →  déplace le playhead\n"
        "Espace  play / pause\n"
        "Echap   stop + retour au début", grpNav);
    kbHint->setStyleSheet("color:#556; font-family:monospace; font-size:10px;");

    fl->addRow("Pas des flèches :", m_cmbArrowStep);
    fl->addRow("",                  m_chkArrowLoop);
    fl->addRow("Raccourcis :",      kbHint);
    lay->addWidget(grpNav);

    // Volume group
    auto *grpVol = makeGroup("Volume", w);
    auto *vlLay  = new QHBoxLayout(grpVol);
    vlLay->setSpacing(10);

    auto *volIcon = new QLabel("🔊", grpVol);
    volIcon->setFixedWidth(22);
    m_sldVolume = new QSlider(Qt::Horizontal, grpVol);
    m_sldVolume->setRange(0, 100);
    m_sldVolume->setStyleSheet(
        "QSlider::groove:horizontal{height:4px;background:#2a2c3e;border-radius:2px;}"
        "QSlider::handle:horizontal{width:14px;height:14px;margin:-5px 0;"
        "border-radius:7px;background:#4466cc;}"
        "QSlider::sub-page:horizontal{background:#3355aa;border-radius:2px;}");

    m_lblVolume = new QLabel("100%", grpVol);
    m_lblVolume->setFixedWidth(38);
    m_lblVolume->setStyleSheet("color:#aab; font-family:monospace;");
    connect(m_sldVolume, &QSlider::valueChanged, this, [this](int v){
        m_lblVolume->setText(QString("%1%").arg(v));
    });

    vlLay->addWidget(volIcon);
    vlLay->addWidget(m_sldVolume, 1);
    vlLay->addWidget(m_lblVolume);
    lay->addWidget(grpVol);

    // Auto-play
    auto *grpAp = makeGroup("Démarrage", w);
    auto *apLay = new QVBoxLayout(grpAp);
    m_chkAutoPlay = new QCheckBox("Démarrer la lecture automatiquement après le chargement", grpAp);
    apLay->addWidget(m_chkAutoPlay);
    lay->addWidget(grpAp);
    lay->addStretch();

    tabs->addTab(w, "▶  Lecture");
}

// ── Tab: Vidéo ────────────────────────────────────────────────────────────────

void PreferencesDialog::buildTabVideo(QTabWidget *tabs)
{
    auto *w   = new QWidget;
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(12,10,12,10); lay->setSpacing(12);

    auto *grpTh = makeGroup("Vignettes de la timeline", w);
    auto *fl    = new QFormLayout(grpTh); fl->setSpacing(8);

    m_spnThumbCount = new QSpinBox(grpTh);
    m_spnThumbCount->setRange(4, 300);
    m_spnThumbCount->setSuffix(" vignettes max");
    styleSpin(m_spnThumbCount);

    m_spnThumbHeight = new QSpinBox(grpTh);
    m_spnThumbHeight->setRange(24, 120);
    m_spnThumbHeight->setSuffix(" px");
    styleSpin(m_spnThumbHeight);

    auto *noteThumb = new QLabel(
        "Les vignettes sont générées en arrière-plan après le chargement.\n"
        "Plus de vignettes = génération plus longue.", grpTh);
    noteThumb->setStyleSheet("color:#556; font-size:10px;");

    fl->addRow("Nombre de vignettes :", m_spnThumbCount);
    fl->addRow("Hauteur des vignettes :", m_spnThumbHeight);
    fl->addRow("", noteThumb);
    lay->addWidget(grpTh);
    lay->addStretch();

    tabs->addTab(w, "🎬  Vidéo");
}

// ── Tab: Interface ────────────────────────────────────────────────────────────

void PreferencesDialog::buildTabInterface(QTabWidget *tabs)
{
    auto *w   = new QWidget;
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(12,10,12,10); lay->setSpacing(12);

    auto *grpZoom = makeGroup("Zoom", w);
    auto *fl      = new QFormLayout(grpZoom); fl->setSpacing(8);

    m_cmbZoomStep = new QComboBox(grpZoom);
    m_cmbZoomStep->addItems({
        "×1.1  (très doux)", "×1.25 (doux — défaut)",
        "×1.5  (normal)",    "×2.0  (rapide)"});
    styleCombo(m_cmbZoomStep);
    fl->addRow("Vitesse molette :", m_cmbZoomStep);
    lay->addWidget(grpZoom);

    auto *grpViz = makeGroup("Visualisation", w);
    auto *vLay   = new QVBoxLayout(grpViz);
    m_chkPeakHold = new QCheckBox("Afficher les pics (peak-hold) en mode Fréquence", grpViz);
    m_chkRmsEnv   = new QCheckBox("Afficher l'enveloppe RMS en mode Amplitude", grpViz);
    vLay->addWidget(m_chkPeakHold);
    vLay->addWidget(m_chkRmsEnv);
    lay->addWidget(grpViz);
    lay->addStretch();

    tabs->addTab(w, "🖥  Interface");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Load prefs → UI
// ─────────────────────────────────────────────────────────────────────────────

static int comboIndexForFft(int v)
{
    const int vals[] = {512,1024,2048,4096};
    for (int i=0;i<4;++i) if(vals[i]==v) return i;
    return 2; // default 2048
}
static int comboIndexForHop(int v)
{
    const int vals[] = {128,256,512,1024,2048};
    for (int i=0;i<5;++i) if(vals[i]==v) return i;
    return 2;
}
static int comboIndexForArrow(double v)
{
    const double vals[] = {0.1,0.5,1.0,5.0,10.0,30.0};
    for (int i=0;i<6;++i) if(std::abs(vals[i]-v)<1e-6) return i;
    return 1;
}
static int comboIndexForZoom(float v)
{
    if (v < 1.15f)  return 0;
    if (v < 1.35f)  return 1;
    if (v < 1.75f)  return 2;
    return 3;
}

void PreferencesDialog::loadIntoUi(const AppPrefs &p)
{
    m_edLastDir->setText(p.lastOpenDir);
    m_chkRememberDir->setChecked(p.rememberLastDir);

    m_cmbFftSize->setCurrentIndex(comboIndexForFft(p.fftSize));
    m_cmbHopSize->setCurrentIndex(comboIndexForHop(p.hopSize));
    m_cmbWindow->setCurrentIndex(std::clamp(p.windowFunc,0,3));
    m_cmbColorMap->setCurrentIndex(std::clamp(p.colorMap,0,4));
    m_spnDbMin->setValue(double(p.specDbMin));
    m_spnDbMax->setValue(double(p.specDbMax));

    m_cmbArrowStep->setCurrentIndex(comboIndexForArrow(p.arrowStepSec));
    m_chkArrowLoop->setChecked(p.arrowLoops);
    m_sldVolume->setValue(p.masterVolume);
    m_chkAutoPlay->setChecked(p.autoPlay);

    m_spnThumbCount->setValue(p.thumbCount);
    m_spnThumbHeight->setValue(p.thumbHeight);

    m_cmbZoomStep->setCurrentIndex(comboIndexForZoom(p.zoomStep));
    m_chkPeakHold->setChecked(p.showPeakHold);
    m_chkRmsEnv->setChecked(p.showRmsEnv);
}

// ─────────────────────────────────────────────────────────────────────────────
//  UI → prefs
// ─────────────────────────────────────────────────────────────────────────────

AppPrefs PreferencesDialog::uiToPrefs() const
{
    AppPrefs p = AppPrefs::load();   // start from saved to keep non-UI fields

    p.lastOpenDir    = m_edLastDir->text();
    p.rememberLastDir = m_chkRememberDir->isChecked();

    const int fftVals[] = {512,1024,2048,4096};
    const int hopVals[] = {128,256,512,1024,2048};
    p.fftSize    = fftVals[std::clamp(m_cmbFftSize->currentIndex(),0,3)];
    p.hopSize    = hopVals[std::clamp(m_cmbHopSize->currentIndex(),0,4)];
    p.windowFunc = m_cmbWindow->currentIndex();
    p.colorMap   = m_cmbColorMap->currentIndex();
    p.specDbMin  = float(m_spnDbMin->value());
    p.specDbMax  = float(m_spnDbMax->value());

    const double stepVals[] = {0.1,0.5,1.0,5.0,10.0,30.0};
    p.arrowStepSec = stepVals[std::clamp(m_cmbArrowStep->currentIndex(),0,5)];
    p.arrowLoops   = m_chkArrowLoop->isChecked();
    p.masterVolume = m_sldVolume->value();
    p.autoPlay     = m_chkAutoPlay->isChecked();

    p.thumbCount   = m_spnThumbCount->value();
    p.thumbHeight  = m_spnThumbHeight->value();

    const float zoomVals[] = {1.1f,1.25f,1.5f,2.0f};
    p.zoomStep    = zoomVals[std::clamp(m_cmbZoomStep->currentIndex(),0,3)];
    p.showPeakHold = m_chkPeakHold->isChecked();
    p.showRmsEnv   = m_chkRmsEnv->isChecked();
    p.timelineH    = p.thumbHeight;

    return p;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Slots
// ─────────────────────────────────────────────────────────────────────────────

void PreferencesDialog::onAccepted()
{
    AppPrefs cur = AppPrefs::load();
    AppPrefs neu = uiToPrefs();

    // Simple change detection (compare key fields)
    m_changed = (cur.fftSize      != neu.fftSize      ||
                 cur.hopSize      != neu.hopSize      ||
                 cur.windowFunc   != neu.windowFunc   ||
                 cur.colorMap     != neu.colorMap     ||
                 cur.arrowStepSec != neu.arrowStepSec ||
                 cur.arrowLoops   != neu.arrowLoops   ||
                 cur.masterVolume != neu.masterVolume ||
                 cur.autoPlay     != neu.autoPlay     ||
                 cur.thumbCount   != neu.thumbCount   ||
                 cur.thumbHeight  != neu.thumbHeight  ||
                 cur.showPeakHold != neu.showPeakHold ||
                 cur.showRmsEnv   != neu.showRmsEnv);

    neu.save();
    accept();
}

void PreferencesDialog::onRestoreDefaults()
{
    loadIntoUi(AppPrefs{});   // construct default-initialised prefs
}
