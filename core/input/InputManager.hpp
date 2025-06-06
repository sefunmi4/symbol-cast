#pragma once
#include <utility>
#include <vector>

namespace sc {

// Simple 2D point
struct Point {
    float x;
    float y;
};

// InputManager records gesture points for later recognition or training.
class InputManager {
public:
    void startCapture() { m_points.clear(); }

    void addPoint(float x, float y) {
        m_points.push_back({x, y});
    }

    const std::vector<Point>& points() const { return m_points; }

    void clear() { m_points.clear(); }

private:
    std::vector<Point> m_points;
};

} // namespace sc
