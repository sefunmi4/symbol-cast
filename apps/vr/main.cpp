#include "core/input/InputManager.hpp"
#include "core/recognition/ModelRunner.hpp"
#include "utils/Logger.hpp"

int main() {
    sc::log(sc::LogLevel::Info, "SymbolCast VR starting");
    sc::InputManager input;
    sc::ModelRunner model;
    model.loadModel("models/lite/symbolcast-v1.onnx");

    // Simulate double tap to begin capture in VR
    input.onTap(0);
    input.onTap(200);

    input.addPoint(0.f, 0.f);
    input.addPoint(0.f, 1.f);
    input.stopCapture();

    sc::log(sc::LogLevel::Info, "Playing back captured path:");
    input.playbackPath();
    auto symbol = model.run(input.points());
    sc::log(sc::LogLevel::Info, std::string("Detected symbol: ") + symbol);
    return 0;
}
