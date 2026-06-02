#include "kaos_reactor/app.h"

#include <QApplication>
#include <QColor>
#include <QCommandLineParser>
#include <QPalette>
#include <QTimer>

static void apply_dark_theme(QApplication& app) {
    app.setStyle("Fusion");

    QPalette p;
    p.setColor(QPalette::Window,          QColor(28,  28,  28));
    p.setColor(QPalette::WindowText,      QColor(220, 220, 220));
    p.setColor(QPalette::Base,            QColor(18,  18,  18));
    p.setColor(QPalette::AlternateBase,   QColor(34,  34,  34));
    p.setColor(QPalette::ToolTipBase,     QColor(40,  40,  40));
    p.setColor(QPalette::ToolTipText,     QColor(220, 220, 220));
    p.setColor(QPalette::Text,            QColor(220, 220, 220));
    p.setColor(QPalette::Button,          QColor(45,  45,  45));
    p.setColor(QPalette::ButtonText,      QColor(220, 220, 220));
    p.setColor(QPalette::BrightText,      Qt::red);
    p.setColor(QPalette::Link,            QColor(80,  140, 220));
    p.setColor(QPalette::Highlight,       QColor(60,  100, 180));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Disabled, QPalette::Text,       QColor(80, 80, 80));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(80, 80, 80));
    app.setPalette(p);
}

int main(int argc, char* argv[]) {
    qputenv("QT_RHI_BACKEND", "vulkan");
    qputenv("QT_LOGGING_RULES", "qt.multimedia.ffmpeg=false");
    qputenv("PIPEWIRE_DEBUG", "0");  // suppress SPA/PipeWire parse warnings on stderr
    QApplication app(argc, argv);
    apply_dark_theme(app);

    QCommandLineParser parser;
    parser.addOption({"image", "Image file to load at startup.", "path"});
    parser.addOption({"audio", "Audio file to load at startup.", "path"});
    parser.process(app);

    kaos::reactor::App window;
    window.show();

    if (parser.isSet("image") || parser.isSet("audio")) {
        QTimer::singleShot(200, &window, [&window, &parser]() {
            if (parser.isSet("image"))
                window.load_image(parser.value("image"));
            if (parser.isSet("audio"))
                window.load_audio(parser.value("audio"));
        });
    }

    return app.exec();
}
