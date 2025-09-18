#include "core/recognition/ModelRunner.hpp"
#include <cassert>
#include <fstream>

int main() {
    sc::ModelRunner runner;
    assert(runner.commandForSymbol("triangle") == "copy");
    assert(runner.commandForSymbol("circle") == "paste");
    assert(runner.commandForSymbol("square") == "custom");
    assert(runner.commandForSymbol("dot") == "paste");
    assert(runner.commandForSymbol("unknown").empty());

    std::ofstream out("commands_test.json");
    out << "{ \"triangle\": \"launch\", \"circle\": \"insert\", \"new\": \"custom\" }";
    out.close();

    sc::ModelRunner custom("commands_test.json");
    assert(custom.commandForSymbol("triangle") == "launch");
    assert(custom.commandForSymbol("square") == "custom");
    assert(custom.commandForSymbol("circle") == "insert");
    assert(custom.commandForSymbol("new") == "custom");
    return 0;
}
