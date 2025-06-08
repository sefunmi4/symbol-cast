#include "core/input/VRInputManager.hpp"
#include <cassert>

int main() {
    sc::VRInputManager vr;
    vr.startCapture();
    vr.addPoint(1.f, 2.f, 3.f);
    vr.addPoint(4.f, 5.f, 6.f);
    assert(vr.capturing());
    vr.stopCapture();
    assert(!vr.capturing());
    assert(vr.points().size() == 2);
    return 0;
}
