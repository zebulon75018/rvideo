#include "MainWindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Spectrogram Viewer");
    app.setOrganizationName("SpectroApp");

    // Apply a global dark theme
    app.setStyle("Fusion");
    QPalette dark;
    dark.setColor(QPalette::Window,          QColor(26,  26,  26));
    dark.setColor(QPalette::WindowText,      Qt::white);
    dark.setColor(QPalette::Base,            QColor(18,  18,  18));
    dark.setColor(QPalette::AlternateBase,   QColor(35,  35,  35));
    dark.setColor(QPalette::ToolTipBase,     Qt::white);
    dark.setColor(QPalette::ToolTipText,     Qt::white);
    dark.setColor(QPalette::Text,            Qt::white);
    dark.setColor(QPalette::Button,          QColor(45,  45,  45));
    dark.setColor(QPalette::ButtonText,      Qt::white);
    dark.setColor(QPalette::BrightText,      Qt::red);
    dark.setColor(QPalette::Link,            QColor(42, 130, 218));
    dark.setColor(QPalette::Highlight,       QColor(42, 130, 218));
    dark.setColor(QPalette::HighlightedText, Qt::black);
    app.setPalette(dark);

    MainWindow win;
    win.show();

    return app.exec();
}
