#include "core/recognition/ModelRunner.hpp"
#include <cassert>

int main() {
    sc::ModelRunner runner;
    assert(runner.commandForSymbol("triangle") == "open-settings");
    assert(runner.commandForSymbol("dot") == "click");
    assert(runner.commandForSymbol("unknown").empty());
    return 0;
}
