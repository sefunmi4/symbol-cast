#include "core/input/InputManager.hpp"
#include "core/recognition/ModelRunner.hpp"
#include <cassert>

int main() {
    sc::InputManager mgr;
    mgr.onTap(0);
    bool started = mgr.onTap(100);
    assert(started && mgr.capturing());
    mgr.addPoint(0.f, 0.f);
    mgr.addPoint(1.f, 1.f);
    mgr.stopCapture();
    sc::ModelRunner model;
    model.loadModel("model.onnx");
    auto symbol = model.run(mgr.points());
    assert(!symbol.empty());
    return 0;
}
