#include "core/input/InputManager.hpp"
#include <cassert>

int main() {
    sc::InputManager mgr(100);
    mgr.onTap(100);
    // going backwards should reset internal state and not start capture
    assert(!mgr.onTap(50) && !mgr.capturing());
    mgr.onTap(200);           // first tap
    assert(mgr.onTap(250) && mgr.capturing());
    return 0;
}
