#include "core/recognition/HybridRecognizer.hpp"
#include <cassert>

int main() {
    sc::HybridRecognizer hybrid;
    std::vector<sc::Point> tri{{0.f,0.f},{1.f,0.f},{0.f,1.f}};
    hybrid.addCustomSample("mytri", tri, "custom-cmd");
    std::vector<sc::Point> tri2{{0.f,0.f},{0.9f,0.1f},{0.1f,0.9f}};
    assert(hybrid.predict(tri2) == "mytri");
    assert(hybrid.commandForGesture(tri2) == "custom-cmd");

    std::vector<sc::Point> square{{0.f,0.f},{1.f,0.f},{1.f,1.f},{0.f,1.f}};
    assert(hybrid.predict(square) == "square");
    assert(hybrid.commandForGesture(square) == "open-menu");
    return 0;
}
