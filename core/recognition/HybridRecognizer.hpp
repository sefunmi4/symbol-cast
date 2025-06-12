#pragma once
#include "GestureRecognizer.hpp"
#include "ModelRunner.hpp"

namespace sc {

// Hybrid recognizer that first checks custom gestures then falls back to the core model.
class HybridRecognizer {
public:
    explicit HybridRecognizer(size_t maxPoints = 16,
                              const std::string& commandFile = "config/commands.json")
        : m_custom(maxPoints), m_model(commandFile) {}

    bool loadModel(const std::string& path) { return m_model.loadModel(path); }

    bool loadCustomProfile(const std::string& path) { return m_custom.loadProfile(path); }
    bool saveCustomProfile(const std::string& path) const { return m_custom.saveProfile(path); }

    void addCustomSample(const std::string& label, const std::vector<Point>& pts,
                         const std::string& command) {
        m_custom.addSample(label, pts, command);
    }

    std::string predict(const std::vector<Point>& pts) const {
        if (!m_custom.empty()) {
            std::string lbl = m_custom.predict(pts);
            if (!lbl.empty())
                return lbl;
        }
        return m_model.run(pts);
    }

    std::string commandForSymbol(const std::string& symbol) const {
        std::string cmd = m_custom.commandForLabel(symbol);
        if (!cmd.empty())
            return cmd;
        return m_model.commandForSymbol(symbol);
    }

    std::string commandForGesture(const std::vector<Point>& pts) const {
        std::string sym = predict(pts);
        return commandForSymbol(sym);
    }

private:
    GestureRecognizer m_custom;
    ModelRunner m_model;
};

} // namespace sc

