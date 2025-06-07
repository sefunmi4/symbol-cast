#pragma once
#include <string>
#include <vector>
#include <optional>
#include "../input/InputManager.hpp"
#ifdef SC_USE_ONNXRUNTIME
#  include <onnxruntime_cxx_api.h>
#endif

namespace sc {

// Stub model runner for loading and running gesture recognition models.
class ModelRunner {
public:
    bool loadModel(const std::string& path) {
        m_modelPath = path;
#ifdef SC_USE_ONNXRUNTIME
        session.reset();
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(1);
        try {
            session.emplace(env, path.c_str(), opts);
        } catch (...) {
            return false;
        }
#endif
        return true;
    }

    // Returns the predicted symbol name (dummy implementation).
    std::string run(const std::vector<Point>& points) {
        if (points.empty()) return "";
#ifdef SC_USE_ONNXRUNTIME
        if (!session) return "";
        // Real inference code omitted for brevity
#endif
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
#ifdef SC_USE_ONNXRUNTIME
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "symbolcast"};
    std::optional<Ort::Session> session;
#endif
};

} // namespace sc
