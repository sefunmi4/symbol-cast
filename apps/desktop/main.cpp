#include <QApplication>
#include "CanvasWindow.hpp"
#include "utils/Logger.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    sc::log(sc::LogLevel::Info, "SymbolCast Desktop starting");
    CanvasWindow win;
    win.show();
    return app.exec();
}
