#pragma once
#include <vector>
#include <cstdint>
#include <fstream>

// TODO: integrate with real OpenXR controller input

namespace sc {
struct Point3D {
    float x;
    float y;
    float z;
};

class VRInputManager {
public:
    VRInputManager() : m_capturing(false), m_connected(false) {}

    bool connectController() {
        m_connected = true; // in real impl this would initialize OpenXR
        return m_connected;
    }

    void startCapture() { m_points.clear(); m_capturing = true; }
    void stopCapture() { m_capturing = false; }
    bool capturing() const { return m_capturing; }
    bool connected() const { return m_connected; }

    void addPoint(float x, float y, float z) {
        if (m_capturing) m_points.push_back({x, y, z});
    }

    const std::vector<Point3D>& points() const { return m_points; }

    void exportCSV(const std::string& path) const {
        std::ofstream out(path);
        for (const auto& p : m_points) {
            out << p.x << ',' << p.y << ',' << p.z << '\n';
        }
    }

private:
    bool m_capturing;
    bool m_connected;
    std::vector<Point3D> m_points;
};
} // namespace sc
