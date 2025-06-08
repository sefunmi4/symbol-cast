#include <QApplication>
#include "CanvasWindow.hpp"
#include <QCommandLineOption>
#include <QCommandLineParser>
#include "utils/Logger.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription("SymbolCast desktop app");
    parser.addHelpOption();

    QCommandLineOption rippleGrowthOpt({"g", "ripple-growth"},
        "Ripple growth per frame", "rate", "2.0");
    QCommandLineOption rippleMaxOpt({"m", "ripple-max"},
        "Maximum ripple radius", "radius", "80.0");
    QCommandLineOption rippleColorOpt({"c", "ripple-color"},
        "Ripple color (hex)", "color", "#fffbe096");
    QCommandLineOption strokeWidthOpt({"w", "stroke-width"},
        "Stroke width", "width", "3");
    QCommandLineOption strokeColorOpt({"s", "stroke-color"},
        "Stroke color (hex)", "color", "#fffbe0");
    QCommandLineOption fadeRateOpt({"f", "fade-rate"},
        "Stroke fade per frame", "rate", "0.005");
    QCommandLineOption detectionColorOpt({"d", "detection-color"},
        "Detection box color (hex)", "color", "#ffffff66");

    parser.addOption(rippleGrowthOpt);
    parser.addOption(rippleMaxOpt);
    parser.addOption(rippleColorOpt);
    parser.addOption(strokeWidthOpt);
    parser.addOption(strokeColorOpt);
    parser.addOption(fadeRateOpt);
    parser.addOption(detectionColorOpt);

    parser.process(app);

    CanvasWindowOptions opts;
    opts.rippleGrowthRate = parser.value(rippleGrowthOpt).toFloat();
    opts.rippleMaxRadius = parser.value(rippleMaxOpt).toFloat();
    opts.rippleColor = QColor(parser.value(rippleColorOpt));
    if (!opts.rippleColor.isValid()) opts.rippleColor = QColor("#fffbe096");
    opts.strokeWidth = parser.value(strokeWidthOpt).toInt();
    opts.strokeColor = QColor(parser.value(strokeColorOpt));
    if (!opts.strokeColor.isValid()) opts.strokeColor = QColor("#fffbe0");
    opts.fadeRate = parser.value(fadeRateOpt).toFloat();
    opts.detectionColor = QColor(parser.value(detectionColorOpt));
    if (!opts.detectionColor.isValid())
        opts.detectionColor = QColor("#ffffff66");

    sc::log(sc::LogLevel::Info, "SymbolCast Desktop starting");
    CanvasWindow win(opts);
    win.show();
    return app.exec();
}
