#include "core/recognition/ModelRunner.hpp"
#include <cassert>
#include <fstream>

int main() {
    sc::ModelRunner runner;

    // Loading a non-existent model should fail.
    assert(!runner.loadModel("missing-model.onnx"));

    // Create a dummy file to simulate a real model and ensure loading succeeds
    // when the file exists.
    const char* temp = "dummy-model.onnx";
    std::ofstream out(temp, std::ios::binary);
    out << '0';
    out.close();

    assert(runner.loadModel(temp));

    return 0;
}

