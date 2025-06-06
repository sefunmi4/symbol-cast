#include "core/input/InputManager.hpp"
#include <cassert>

int main() {
    sc::InputManager mgr;
    mgr.startCapture();
    mgr.addPoint(0.f, 0.f);
    mgr.addPoint(1.f, 1.f);
    assert(mgr.points().size() == 2);
    return 0;
}
