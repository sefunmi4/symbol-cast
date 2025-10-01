#include <QApplication>
#include "CanvasWindow.hpp"
#include <QAction>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCursor>
#include <QIcon>
#include <QMenu>
#include <QObject>
#include <QSignalBlocker>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QString>
#include <functional>
#include <utility>
#include "utils/Logger.hpp"

#ifdef Q_OS_MAC
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <objc/message.h>
#include <objc/objc.h>
#include <objc/runtime.h>
#endif

namespace {

QIcon trayIconForApp(const QApplication &app) {
    QIcon icon = QIcon::fromTheme(QStringLiteral("input-keyboard"));
    if (icon.isNull())
        icon = app.style()->standardIcon(QStyle::SP_ComputerIcon);
    return icon;
}

#ifdef Q_OS_MAC
namespace {
std::function<void()> gPresentWindowHandler;
void *const kMacServiceObserver = reinterpret_cast<void *>(1);

void runPresentWindow(void *) {
    if (gPresentWindowHandler)
        gPresentWindowHandler();
}

void macServiceNotification(CFNotificationCenterRef, void *, CFStringRef, const void *, CFDictionaryRef) {
    if (gPresentWindowHandler)
        dispatch_async_f(dispatch_get_main_queue(), nullptr, runPresentWindow);
}
} // namespace

void registerMacServiceHandler(std::function<void()> handler) {
    gPresentWindowHandler = std::move(handler);
    CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter();
    CFNotificationCenterRemoveObserver(center,
                                       kMacServiceObserver,
                                       CFSTR("com.symbolcast.desktop.presentWindow"),
                                       nullptr);
    CFNotificationCenterAddObserver(center,
                                    kMacServiceObserver,
                                    macServiceNotification,
                                    CFSTR("com.symbolcast.desktop.presentWindow"),
                                    nullptr,
                                    CFNotificationSuspensionBehaviorDeliverImmediately);
}

void activateMacApplication() {
    Class nsAppClass = objc_getClass("NSApplication");
    if (!nsAppClass)
        return;
    SEL sharedAppSel = sel_registerName("sharedApplication");
    id nsApp = ((id(*)(Class, SEL))objc_msgSend)(nsAppClass, sharedAppSel);
    if (!nsApp)
        return;
    SEL activateSel = sel_registerName("activateIgnoringOtherApps:");
    ((void (*)(id, SEL, BOOL))objc_msgSend)(nsApp, activateSel, YES);
}
#endif

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription("SymbolCast desktop app");
    parser.addHelpOption();

    QCommandLineOption rippleGrowthOpt({"g", "ripple-growth"},
        "Ripple growth per frame", "rate", "1.0");
    QCommandLineOption rippleMaxOpt({"m", "ripple-max"},
        "Maximum ripple radius", "radius", "30.0");
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
    QCommandLineOption fullscreenOpt({"F", "fullscreen"},
        "Launch the board fullscreen");
    QCommandLineOption disableCursorAnimOpt({"A", "no-cursor-animation"},
        "Disable cursor animation (enabled by default)");

    parser.addOption(rippleGrowthOpt);
    parser.addOption(rippleMaxOpt);
    parser.addOption(rippleColorOpt);
    parser.addOption(strokeWidthOpt);
    parser.addOption(strokeColorOpt);
    parser.addOption(fadeRateOpt);
    parser.addOption(detectionColorOpt);
    parser.addOption(fullscreenOpt);
    parser.addOption(disableCursorAnimOpt);

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
    opts.fullscreen = parser.isSet(fullscreenOpt);
    if (parser.isSet(disableCursorAnimOpt))
        opts.cursorAnimation = false;

    SC_LOG(sc::LogLevel::Info, "SymbolCast Desktop starting");
    CanvasWindow win(opts);

    auto presentWindow = [&]() {
        if (opts.fullscreen) {
            win.showFullScreen();
        } else if (win.isMinimized()) {
            win.showNormal();
        } else {
            win.show();
        }
        win.raise();
        win.activateWindow();
#ifdef Q_OS_MAC
        activateMacApplication();
#endif
    };

#ifdef Q_OS_MAC
    registerMacServiceHandler(presentWindow);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        CFNotificationCenterRemoveObserver(CFNotificationCenterGetDistributedCenter(),
                                           kMacServiceObserver,
                                           CFSTR("com.symbolcast.desktop.presentWindow"),
                                           nullptr);
    });
#endif

    const bool trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    if (trayAvailable)
        QApplication::setQuitOnLastWindowClosed(false);
#endif

    if (!trayAvailable) {
        SC_LOG(sc::LogLevel::Warn,
               "System tray unavailable; window will close the application");
    } else {
        auto *trayIcon = new QSystemTrayIcon(trayIconForApp(app), &app);
        trayIcon->setToolTip(QObject::tr("SymbolCast"));

        auto *trayMenu = new QMenu();
        QAction *toggleAction = trayMenu->addAction(QObject::tr("Symbol Keyboard"));
        toggleAction->setCheckable(true);
        QAction *quitAction = trayMenu->addAction(QObject::tr("Quit"));
        trayIcon->setContextMenu(trayMenu);

        win.setHideOnClose(true);

        auto toggleVisibility = [&]() {
            if (win.isVisible() && !win.isMinimized()) {
                win.hide();
            } else {
                presentWindow();
            }
        };

        QObject::connect(toggleAction, &QAction::triggered, &win,
                         [&](bool) { toggleVisibility(); });
        QObject::connect(quitAction, &QAction::triggered, &app,
                         [](bool) { QCoreApplication::quit(); });

        QObject::connect(&win, &CanvasWindow::visibilityChanged, toggleAction,
                         [toggleAction](bool visible) {
                             QSignalBlocker blocker(toggleAction);
                             toggleAction->setChecked(visible);
                         });

        QObject::connect(trayIcon, &QSystemTrayIcon::activated, &win,
                         [&, trayMenu](QSystemTrayIcon::ActivationReason reason) {
            switch (reason) {
            case QSystemTrayIcon::Trigger:
            case QSystemTrayIcon::DoubleClick:
                toggleVisibility();
                break;
            case QSystemTrayIcon::Context:
                if (trayMenu && !trayMenu->isVisible())
                    trayMenu->popup(QCursor::pos());
                break;
            default:
                break;
            }
        });

        trayIcon->show();
    }

    presentWindow();
    return app.exec();
}
