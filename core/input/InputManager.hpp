#pragma once
#include <chrono>
#include <iostream>
#include <thread>
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
    InputManager() : m_capturing(false), m_lastTap(0) {}

    // Handle a tap event with timestamp in milliseconds. Returns true if
    // a double tap was detected and capture started.
    bool onTap(uint64_t timestamp) {
        if (timestamp - m_lastTap < 300) {
            startCapture();
            m_lastTap = 0;
            return true;
        }
        m_lastTap = timestamp;
        return false;
    }

    void startCapture() {
        m_points.clear();
        m_capturing = true;
    }

    void stopCapture() { m_capturing = false; }

    bool capturing() const { return m_capturing; }

    void addPoint(float x, float y) {
        if (m_capturing)
            m_points.push_back({x, y});
    }

    const std::vector<Point>& points() const { return m_points; }

    void clear() { m_points.clear(); }

    // Simple console animation to visualize the path.
    void playbackPath() const {
        for (const auto& p : m_points) {
            std::cout << "Point(" << p.x << ", " << p.y << ")" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

private:
    bool m_capturing;
    uint64_t m_lastTap;
    std::vector<Point> m_points;
};

} // namespace sc
