#include "core/input/InputManager.hpp"
#include <cassert>

int main() {
    sc::InputManager mgr;
    bool dbl = mgr.onTap(10);
    assert(!dbl && !mgr.capturing());
    dbl = mgr.onTap(20); // second tap -> start capture
    assert(dbl && mgr.capturing());
    // single tap while capturing should do nothing until the second tap.
    dbl = mgr.onTap(50);
    assert(!dbl && mgr.capturing());
    dbl = mgr.onTap(80);
    assert(dbl && !mgr.capturing());
    // start again
    dbl = mgr.onTap(130);
    assert(!dbl && !mgr.capturing());
    dbl = mgr.onTap(160);
    assert(dbl && mgr.capturing());
    // Need a double tap to end the capture.
    dbl = mgr.onTap(210);
    assert(!dbl && mgr.capturing());
    dbl = mgr.onTap(240);
    assert(dbl && !mgr.capturing());
    return 0;
}
