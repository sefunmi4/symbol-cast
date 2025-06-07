#include "core/input/InputManager.hpp"
#include <cassert>

int main() {
    sc::InputManager mgr;
    bool start = mgr.onTap(10);
    assert(!start && !mgr.capturing());
    start = mgr.onTap(20); // within interval -> start capture
    assert(start && mgr.capturing());
    // second double tap should stop capture
    start = mgr.onTap(30);
    assert(!start); // single tap
    start = mgr.onTap(40); // double tap -> stop
    assert(start && !mgr.capturing());
    // far apart -> not a double tap
    start = mgr.onTap(400);
    assert(!start && !mgr.capturing());
    return 0;
}
