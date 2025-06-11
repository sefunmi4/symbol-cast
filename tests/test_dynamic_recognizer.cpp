#include "core/recognition/GestureRecognizer.hpp"
#include <cassert>

int main() {
    sc::GestureRecognizer rec(3);
    std::vector<sc::Point> tri{{0.f,0.f},{1.f,0.f},{0.f,1.f}};
    rec.addSample("tri", tri, "launch");
    std::vector<sc::Point> tri2{{0.f,0.f},{0.9f,0.1f},{0.1f,0.9f}};
    assert(rec.predict(tri2) == "tri");
    assert(rec.commandForLabel("tri") == "launch");
    rec.undo();
    assert(rec.predict(tri2).empty());
    rec.redo();
    assert(rec.predict(tri2) == "tri");
    return 0;
}
