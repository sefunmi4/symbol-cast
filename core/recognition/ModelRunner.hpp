#pragma once
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <fstream>
#include <cctype>
#include "../input/InputManager.hpp"
#ifdef SC_USE_ONNXRUNTIME
#  include <onnxruntime_cxx_api.h>
#endif

// TODO: refactor for dynamic input size and model versioning

namespace sc {

// Stub model runner for loading and running gesture recognition models.
class ModelRunner {
public:
    explicit ModelRunner(const std::string& commandFile = "config/commands.json") {
        loadCommands(commandFile);
    }

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
    std::string run(const std::vector<Point>& points) const {
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
        auto it = m_commands.find(symbol);
        if (it != m_commands.end())
            return it->second;
        return "";
    }

    const std::string& modelPath() const { return m_modelPath; }

private:
    void loadCommands(const std::string& path) {
        m_commands = {
            {"triangle", "open-settings"},
            {"dot", "click"},
            {"square", "open-menu"}
        };
        std::ifstream in(path);
        if (!in.is_open())
            return;
        std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
        size_t pos = 0;
        while ((pos = content.find('"', pos)) != std::string::npos) {
            size_t endKey = content.find('"', pos + 1);
            if (endKey == std::string::npos) break;
            std::string key = content.substr(pos + 1, endKey - pos - 1);
            pos = content.find(':', endKey);
            if (pos == std::string::npos) break;
            ++pos;
            while (pos < content.size() && std::isspace(static_cast<unsigned char>(content[pos]))) ++pos;
            if (pos >= content.size() || content[pos] != '"') continue;
            size_t endVal = content.find('"', pos + 1);
            if (endVal == std::string::npos) break;
            std::string val = content.substr(pos + 1, endVal - pos - 1);
            m_commands[key] = val;
            pos = endVal + 1;
        }
    }

    std::string m_modelPath;
    std::unordered_map<std::string, std::string> m_commands;
#ifdef SC_USE_ONNXRUNTIME
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "symbolcast"};
    std::optional<Ort::Session> session;
#endif
};

} // namespace sc
