#include "core/input/InputManager.hpp"
#include "core/input/VRInputManager.hpp"
#include "core/recognition/ModelRunner.hpp"
#include "utils/Logger.hpp"

// TODO: replace placeholder data with actual OpenXR-based capture loop

int main() {
    sc::log(sc::LogLevel::Info, "SymbolCast VR starting");
    sc::InputManager input;
    sc::VRInputManager vrInput;
    sc::ModelRunner model;
    model.loadModel("models/symbolcast-v1.onnx");

    // Simulate double tap to begin capture in VR
    input.onTap(0);
    input.onTap(200);
    vrInput.startCapture();
    vrInput.addPoint(0.f, 0.f, 0.f);
    vrInput.addPoint(0.f, 1.f, 0.5f);
    vrInput.addPoint(1.f, 1.f, 1.f);
    vrInput.stopCapture();
    vrInput.exportCSV("captured_vr_gesture.csv");

    sc::log(sc::LogLevel::Info, "Playing back captured path:");
    input.playbackPath();
    // VR path would be processed separately; reuse 2D for model demo
    auto symbol = model.run(input.points());
    sc::log(sc::LogLevel::Info, std::string("Detected symbol: ") + symbol);
    return 0;
}
