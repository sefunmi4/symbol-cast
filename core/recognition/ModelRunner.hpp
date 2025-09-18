#pragma once
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <fstream>
#include <cctype>
#include <algorithm>
#include <cmath>
#include "../input/InputManager.hpp"
#include "utils/Logger.hpp"
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
        m_modelLoaded = false;
        m_warnedFallback = false;
        m_modelFilePresent = false;

        // Ensure the model file actually exists before proceeding. Without
        // ONNX Runtime enabled the function previously returned `true`
        // unconditionally, which meant callers had no way of detecting a
        // missing model and CI tests expecting a failure would pass
        // incorrectly.
        std::ifstream file(path, std::ios::binary);
        if (!file.good())
            return false;
        m_modelFilePresent = true;

#ifdef SC_USE_ONNXRUNTIME
        session.reset();
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(1);
        try {
            session.emplace(env, path.c_str(), opts);
            m_modelLoaded = true;
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
            case 0: return "circle";
            case 1: return "triangle";
            case 2: return "square";
            default: return "";
            }
        }
#endif
        if (!m_warnedFallback) {
            if (!m_runtimeEnabled && m_modelFilePresent)
                SC_LOG(sc::LogLevel::Warn,
                       "ONNX Runtime support is not enabled. Falling back to heuristic detection for " +
                           m_modelPath + ".");
            else if (m_modelFilePresent && !m_modelLoaded)
                SC_LOG(sc::LogLevel::Warn,
                       "ONNX model " + m_modelPath +
                           " could not be loaded by ONNX Runtime. Falling back to heuristic detection.");
            else if (!m_modelPath.empty())
                SC_LOG(sc::LogLevel::Warn,
                       "Model " + m_modelPath +
                           " not available. Falling back to heuristic detection.");
            m_warnedFallback = true;
        }
        return classifyHeuristic(points);
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
            {"triangle", "copy"},
            {"circle", "paste"},
            {"square", "custom"},
            {"dot", "paste"}
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
            std::string normalized = (key == "dot") ? "circle" : key;
            m_commands[normalized] = val;
            if (normalized == "circle")
                m_commands["dot"] = val;
            pos = endVal + 1;
        }
    }

    std::string classifyHeuristic(const std::vector<Point>& points) const {
        if (points.empty())
            return std::string();

        if (points.size() <= 2)
            return "circle";

        float minX = points.front().x;
        float maxX = points.front().x;
        float minY = points.front().y;
        float maxY = points.front().y;
        float sumX = 0.f;
        float sumY = 0.f;
        for (const auto& p : points) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
            sumX += p.x;
            sumY += p.y;
        }

        const float width = std::max(1e-3f, maxX - minX);
        const float height = std::max(1e-3f, maxY - minY);
        const float aspect = width > height ? width / height : height / width;
        const float cx = sumX / static_cast<float>(points.size());
        const float cy = sumY / static_cast<float>(points.size());

        float meanRadius = 0.f;
        std::vector<float> radii;
        radii.reserve(points.size());
        for (const auto& p : points) {
            float dx = p.x - cx;
            float dy = p.y - cy;
            float dist = std::sqrt(dx * dx + dy * dy);
            radii.push_back(dist);
            meanRadius += dist;
        }
        meanRadius /= static_cast<float>(radii.size());

        float variance = 0.f;
        for (float r : radii) {
            float diff = r - meanRadius;
            variance += diff * diff;
        }
        variance /= static_cast<float>(std::max<size_t>(1, radii.size() - 1));
        float radiusStdDev = std::sqrt(variance);
        float radialUniformity = meanRadius > 1e-3f ? radiusStdDev / meanRadius : 1.f;

        auto cross = [](const Point& o, const Point& a, const Point& b) {
            return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
        };

        std::vector<Point> sorted = points;
        std::sort(sorted.begin(), sorted.end(), [](const Point& a, const Point& b) {
            if (a.x == b.x)
                return a.y < b.y;
            return a.x < b.x;
        });
        sorted.erase(std::unique(sorted.begin(), sorted.end(), [](const Point& a, const Point& b) {
                         return std::fabs(a.x - b.x) < 1e-3f && std::fabs(a.y - b.y) < 1e-3f;
                     }),
                     sorted.end());

        size_t hullSize = sorted.size();
        if (sorted.size() >= 3) {
            std::vector<Point> hull(2 * sorted.size());
            size_t k = 0;
            for (const auto& pt : sorted) {
                while (k >= 2 && cross(hull[k - 2], hull[k - 1], pt) <= 0.f)
                    --k;
                hull[k++] = pt;
            }
            for (int i = static_cast<int>(sorted.size()) - 2, t = static_cast<int>(k) + 1; i >= 0; --i) {
                const auto& pt = sorted[static_cast<size_t>(i)];
                while (k >= static_cast<size_t>(t) && cross(hull[k - 2], hull[k - 1], pt) <= 0.f)
                    --k;
                hull[k++] = pt;
            }
            hullSize = k > 1 ? k - 1 : k;
        }

        if (hullSize >= 4 && aspect < 1.3f)
            return "square";
        if (hullSize == 3)
            return "triangle";
        if (radialUniformity < 0.25f)
            return "circle";
        if (aspect < 1.2f && points.size() > 12)
            return "square";
        if (points.size() > 10)
            return "triangle";
        return "circle";
    }

    std::string m_modelPath;
    bool m_modelLoaded{false};
    bool m_modelFilePresent{false};
    mutable bool m_warnedFallback{false};
#ifdef SC_USE_ONNXRUNTIME
    bool m_runtimeEnabled{true};
#else
    bool m_runtimeEnabled{false};
#endif
    std::unordered_map<std::string, std::string> m_commands;
#ifdef SC_USE_ONNXRUNTIME
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "symbolcast"};
    std::optional<Ort::Session> session;
#endif
};

} // namespace sc
