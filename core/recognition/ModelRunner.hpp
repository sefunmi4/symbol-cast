#pragma once
#include <string>
#include <vector>
#include "../input/InputManager.hpp"

namespace sc {

// Stub model runner for loading and running gesture recognition models.
class ModelRunner {
public:
    bool loadModel(const std::string& path) {
        m_modelPath = path;
        // In a real implementation we would load the ONNX model here.
        return true;
    }

    // Returns the predicted symbol name (dummy implementation).
    std::string run(const std::vector<Point>& points) {
        if (points.empty()) return "";
        // Real inference would happen here.
        return points.size() > 2 ? "triangle" : "dot";
    }

    std::string commandForSymbol(const std::string& symbol) const {
        if (symbol == "triangle") return "open-settings";
        if (symbol == "dot") return "click";
        return "";
    }

    const std::string& modelPath() const { return m_modelPath; }

private:
    std::string m_modelPath;
};

} // namespace sc
