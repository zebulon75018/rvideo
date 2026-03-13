#include "MainWindow.h"
#include "PreferencesDialog.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <QFileInfo>
#include <QPalette>
#include <QColor>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Spectrogram Viewer");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("SpectroApp");
    app.setOrganizationDomain("spectroapp.local");

    // ── Dark Fusion theme ─────────────────────────────────────────────────────
    app.setStyle("Fusion");
    QPalette dark;
    dark.setColor(QPalette::Window,          QColor(22,  22,  30));
    dark.setColor(QPalette::WindowText,      QColor(210, 210, 220));
    dark.setColor(QPalette::Base,            QColor(14,  14,  20));
    dark.setColor(QPalette::AlternateBase,   QColor(30,  30,  40));
    dark.setColor(QPalette::ToolTipBase,     QColor(30,  30,  45));
    dark.setColor(QPalette::ToolTipText,     QColor(210, 210, 220));
    dark.setColor(QPalette::Text,            QColor(210, 210, 220));
    dark.setColor(QPalette::Button,          QColor(38,  38,  52));
    dark.setColor(QPalette::ButtonText,      QColor(210, 210, 220));
    dark.setColor(QPalette::BrightText,      Qt::red);
    dark.setColor(QPalette::Link,            QColor(80, 140, 240));
    dark.setColor(QPalette::Highlight,       QColor(55, 100, 210));
    dark.setColor(QPalette::HighlightedText, Qt::white);
    dark.setColor(QPalette::Disabled, QPalette::WindowText, QColor(80,80,90));
    dark.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(80,80,90));
    dark.setColor(QPalette::Disabled, QPalette::Text,       QColor(80,80,90));
    app.setPalette(dark);

    // ── Command-line parser ───────────────────────────────────────────────────
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Spectrogram Viewer — visualise l'audio et la vidéo\n"
        "Modes : Spectrogramme, Fréquence, Amplitude");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument(
        "fichier",
        "Fichier audio ou vidéo à charger au démarrage.\n"
        "Formats supportés : wav, flac, ogg, mp3, mp4, m4a, mov, mkv, avi, webm…",
        "[fichier]");

    parser.process(app);

    const QStringList args = parser.positionalArguments();

    // ── Main window ───────────────────────────────────────────────────────────
    MainWindow win;
    win.show();

    // ── Auto-load file if given as argument ───────────────────────────────────
    if (!args.isEmpty())
    {
        const QString filePath = args.first();
        if (QFileInfo::exists(filePath))
        {
            // Defer by one event-loop tick so the window is fully painted first
            QTimer::singleShot(0, &win, [&win, filePath]() {
                win.loadFile(filePath);
            });
        }
        else
        {
            qWarning("Fichier introuvable : %s", qPrintable(filePath));
        }
    }

    return app.exec();
}
