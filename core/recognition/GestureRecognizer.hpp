#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>
#include <utility>
#include "../input/InputManager.hpp"

namespace sc {

struct GestureSample {
    std::vector<float> points; // flattened x,y
    std::string label;
    std::string command;
};

class GestureRecognizer {
public:
    explicit GestureRecognizer(size_t maxPoints = 16)
        : m_maxPoints(maxPoints) {}

    bool loadProfile(const std::string& path) {
        m_samples.clear();
        m_redo.clear();
        std::ifstream in(path);
        if (!in.is_open()) return false;
        std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
        size_t pos = 0;
        while ((pos = content.find("\"label\"", pos)) != std::string::npos) {
            size_t start = content.find('"', pos + 7);
            if (start == std::string::npos) break;
            size_t end = content.find('"', start + 1);
            if (end == std::string::npos) break;
            std::string label = content.substr(start + 1, end - start - 1);
            pos = content.find("\"command\"", end);
            if (pos == std::string::npos) break;
            start = content.find('"', pos + 9);
            if (start == std::string::npos) break;
            end = content.find('"', start + 1);
            if (end == std::string::npos) break;
            std::string command = content.substr(start + 1, end - start - 1);
            pos = content.find('[', end);
            if (pos == std::string::npos) break;
            size_t endArr = content.find(']', pos);
            if (endArr == std::string::npos) break;
            std::string arr = content.substr(pos + 1, endArr - pos - 1);
            std::vector<float> pts;
            std::stringstream ss(arr);
            std::string num;
            while (std::getline(ss, num, ',')) {
                if (!num.empty())
                    pts.push_back(std::stof(num));
            }
            m_samples.push_back({pts, label, command});
            pos = endArr;
        }
        return true;
    }

    bool saveProfile(const std::string& path) const {
        std::ofstream out(path);
        if (!out.is_open()) return false;
        out << "[";
        for (size_t i = 0; i < m_samples.size(); ++i) {
            if (i) out << ",\n";
            const auto& s = m_samples[i];
            out << "{\"label\":\"" << s.label << "\",\"command\":\"" << s.command << "\",\"points\":";
            out << "[";
            for (size_t j = 0; j < s.points.size(); ++j) {
                if (j) out << ",";
                out << s.points[j];
            }
            out << "]}";
        }
        out << "]";
        return true;
    }

    void addSample(const std::string& label, const std::vector<Point>& pts,
                   const std::string& command) {
        m_redo.clear();
        GestureSample s;
        s.label = label;
        s.command = command;
        s.points = toFeature(pts);
        m_samples.push_back(std::move(s));
    }

    bool undo() {
        if (m_samples.empty()) return false;
        m_redo.push_back(m_samples.back());
        m_samples.pop_back();
        return true;
    }

    bool redo() {
        if (m_redo.empty()) return false;
        m_samples.push_back(m_redo.back());
        m_redo.pop_back();
        return true;
    }

    std::string predict(const std::vector<Point>& pts) const {
        if (m_samples.empty()) return std::string();
        std::vector<float> feat = toFeature(pts);
        float bestDist = std::numeric_limits<float>::max();
        std::string bestLabel;
        for (const auto& s : m_samples) {
            float dist = 0.f;
            for (size_t i = 0; i < feat.size(); ++i) {
                float d = feat[i] - (i < s.points.size() ? s.points[i] : 0.f);
                dist += d * d;
            }
            if (dist < bestDist) {
                bestDist = dist;
                bestLabel = s.label;
            }
        }
        return bestLabel;
    }

    std::string commandForLabel(const std::string& label) const {
        for (const auto& s : m_samples)
            if (s.label == label) return s.command;
        return std::string();
    }

    std::string commandForGesture(const std::vector<Point>& pts) const {
        std::string lbl = predict(pts);
        return commandForLabel(lbl);
    }

    bool empty() const { return m_samples.empty(); }

private:
    std::vector<float> toFeature(const std::vector<Point>& pts) const {
        std::vector<float> feat;
        feat.reserve(m_maxPoints * 2);
        size_t n = std::min<size_t>(pts.size(), m_maxPoints);
        for (size_t i = 0; i < n; ++i) {
            feat.push_back(pts[i].x);
            feat.push_back(pts[i].y);
        }
        feat.resize(m_maxPoints * 2, 0.f);
        return feat;
    }

    size_t m_maxPoints;
    std::vector<GestureSample> m_samples;
    std::vector<GestureSample> m_redo;
};

} // namespace sc

