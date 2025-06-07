#pragma once
#include <vector>
#include <cstdint>

namespace sc {
struct Point3D {
    float x;
    float y;
    float z;
};

class VRInputManager {
public:
    VRInputManager() : m_capturing(false) {}

    void startCapture() { m_points.clear(); m_capturing = true; }
    void stopCapture() { m_capturing = false; }
    bool capturing() const { return m_capturing; }

    void addPoint(float x, float y, float z) {
        if (m_capturing) m_points.push_back({x, y, z});
    }

    const std::vector<Point3D>& points() const { return m_points; }

private:
    bool m_capturing;
    std::vector<Point3D> m_points;
};
} // namespace sc
