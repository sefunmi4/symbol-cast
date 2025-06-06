#include "core/input/InputManager.hpp"
#include "core/recognition/ModelRunner.hpp"
#include "utils/Logger.hpp"
#include <chrono>
#include <thread>

int main() {
    sc::log(sc::LogLevel::Info, "SymbolCast Desktop starting");
    sc::InputManager input;
    sc::ModelRunner model;
    model.loadModel("models/lite/symbolcast-v1.onnx");

    // Simulated double tap to begin capture
    input.onTap(0);
    input.onTap(100);

    // Simulated gesture points forming a triangle
    input.addPoint(0.f, 0.f);
    input.addPoint(0.5f, 1.f);
    input.addPoint(1.f, 0.f);
    input.stopCapture();

    sc::log(sc::LogLevel::Info, "Playing back captured path:");
    input.playbackPath();

    auto symbol = model.run(input.points());
    sc::log(sc::LogLevel::Info, std::string("Detected symbol: ") + symbol);
    auto command = model.commandForSymbol(symbol);
    sc::log(sc::LogLevel::Info, std::string("Executing command: ") + command);
    return 0;
}
