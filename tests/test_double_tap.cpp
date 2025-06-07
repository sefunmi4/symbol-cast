#include "core/input/InputManager.hpp"
#include <cassert>

int main() {
    sc::InputManager mgr;
    bool start = mgr.onTap(10);
    assert(!start && !mgr.capturing());
    start = mgr.onTap(20); // within interval -> start capture
    assert(start && mgr.capturing());
    mgr.stopCapture();
    start = mgr.onTap(40);
    assert(!start);
    start = mgr.onTap(400); // far apart -> should not start
    assert(!start);
    return 0;
}
