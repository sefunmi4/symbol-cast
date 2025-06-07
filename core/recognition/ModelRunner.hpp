#pragma once
#include <string>
#include <vector>
#include <optional>
#include "../input/InputManager.hpp"
#ifdef SC_USE_ONNXRUNTIME
#  include <onnxruntime_cxx_api.h>
#endif

// TODO: refactor for dynamic input size and model versioning
#ifdef SC_USE_ONNXRUNTIME
#  include <onnxruntime_cxx_api.h>
#endif

// TODO: refactor for dynamic input size and model versioning
#ifdef SC_USE_ONNXRUNTIME
#  include <onnxruntime_cxx_api.h>
#endif

// TODO: refactor for dynamic input size and model versioning
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

    // Returns the predicted symbol name.
    std::string run(const std::vector<Point>& points) {
        if (points.empty()) return "";
#ifdef SC_USE_ONNXRUNTIME
        if (session) {
            std::vector<float> input;
            for (const auto& p : points) {
                input.push_back(p.x);
                input.push_back(p.y);
            }
            while (input.size() < 6)
                input.push_back(0.f);
            std::array<int64_t, 2> shape{1, 6};
            Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
            Ort::Value tensor = Ort::Value::CreateTensor<float>(mem, input.data(), input.size(), shape.data(), 2);
            Ort::AllocatorWithDefaultOptions allocator;
            auto inputName = session->GetInputNameAllocated(0, allocator);
            auto outputName = session->GetOutputNameAllocated(0, allocator);
            auto outputTensors = session->Run(Ort::RunOptions{nullptr}, &inputName.get(), &tensor, 1, &outputName.get(), 1);
            auto& out = outputTensors.front();
            int64_t idx = out.GetTensorMutableData<int64_t>()[0];
            switch (idx) {
            case 0: return "dot";
            case 1: return "triangle";
            case 2: return "square";
            default: return "";
            }
        }
#endif
        return points.size() > 3 ? "square" : (points.size() > 2 ? "triangle" : "dot");
    }

    std::string commandForSymbol(const std::string& symbol) const {
        if (symbol == "triangle") return "open-settings";
        if (symbol == "dot") return "click";
        if (symbol == "square") return "open-menu";
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
