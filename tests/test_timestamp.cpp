#include "core/input/InputManager.hpp"
#include <cassert>

int main() {
    sc::InputManager mgr(100);
    mgr.onTap(100);
    // going backwards should reset internal state
    assert(!mgr.onTap(50));
    mgr.onTap(200);
    assert(mgr.onTap(250));
    return 0;
}
