#include "core/input/InputManager.hpp"
#include "core/recognition/ModelRunner.hpp"
#include "utils/Logger.hpp"

int main() {
    sc::log(sc::LogLevel::Info, "SymbolCast Desktop starting");
    sc::InputManager input;
    sc::ModelRunner model;
    model.loadModel("models/lite/symbolcast-v1.onnx");
    input.startCapture();
    input.addPoint(0, 0);
    input.addPoint(1, 1);
    auto command = model.run(input.points());
    sc::log(sc::LogLevel::Info, std::string("Predicted command: ") + command);
    return 0;
}
