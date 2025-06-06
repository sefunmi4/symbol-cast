#include "core/input/InputManager.hpp"
#include "utils/Logger.hpp"

int main() {
    sc::log(sc::LogLevel::Info, "SymbolCast VR starting");
    sc::InputManager input;
    input.startCapture();
    input.addPoint(0, 0);
    input.addPoint(0, 1);
    sc::log(sc::LogLevel::Info, "Captured simple vertical gesture");
    return 0;
}
