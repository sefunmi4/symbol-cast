#include "core/recognition/ModelRunner.hpp"
#include <cassert>
#include <fstream>

int main() {
    sc::ModelRunner runner;
    assert(runner.commandForSymbol("triangle") == "open-settings");
    assert(runner.commandForSymbol("dot") == "click");
    assert(runner.commandForSymbol("square") == "open-menu");
    assert(runner.commandForSymbol("unknown").empty());

    std::ofstream out("commands_test.json");
    out << "{ \"triangle\": \"launch\", \"new\": \"custom\" }";
    out.close();

    sc::ModelRunner custom("commands_test.json");
    assert(custom.commandForSymbol("triangle") == "launch");
    assert(custom.commandForSymbol("square") == "open-menu");
    assert(custom.commandForSymbol("new") == "custom");
    return 0;
}
