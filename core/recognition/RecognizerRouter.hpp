#pragma once
#include "ModelRunner.hpp"
#include <unordered_map>
#include <string>
#include <fstream>
#include <cctype>

namespace sc {

class RecognizerRouter {
public:
    explicit RecognizerRouter(const std::string& configFile = "config/models.json") {
        loadConfig(configFile);
    }

    bool loadConfig(const std::string& path) {
        m_models.clear();
        std::ifstream in(path);
        if (!in.is_open())
            return false;
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
            m_models[key] = ModelRunner();
            m_models[key].loadModel(val);
            pos = endVal + 1;
        }
        return !m_models.empty();
    }

    std::string recognize(const std::vector<Point>& pts, const std::string& mode = "auto") const {
        std::string chosen = mode;
        if (mode == "auto") {
            if (pts.size() <= 6)
                chosen = "shape_model";
            else
                chosen = "letter_model";
        }
        auto it = m_models.find(chosen);
        if (it == m_models.end())
            return std::string();
        return it->second.run(pts);
    }

    std::string commandForSymbol(const std::string& sym) const {
        for (const auto& kv : m_models) {
            std::string cmd = kv.second.commandForSymbol(sym);
            if (!cmd.empty())
                return cmd;
        }
        return std::string();
    }

private:
    std::unordered_map<std::string, ModelRunner> m_models;
};

} // namespace sc
