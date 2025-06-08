#include "core/input/InputManager.hpp"
#include <cassert>

int main() {
    sc::InputManager mgr;
    bool dbl = mgr.onTap(10);
    assert(!dbl && !mgr.capturing());
    dbl = mgr.onTap(20); // second tap -> start capture
    assert(dbl && mgr.capturing());
    // single tap stops capture
    dbl = mgr.onTap(50);
    assert(!dbl && !mgr.capturing());
    // start again
    dbl = mgr.onTap(100);
    assert(!dbl && !mgr.capturing());
    dbl = mgr.onTap(140);
    assert(dbl && mgr.capturing());
    // another single tap end
    dbl = mgr.onTap(200);
    assert(!dbl && !mgr.capturing());
    return 0;
}
