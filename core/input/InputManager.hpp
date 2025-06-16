#pragma once
#include <chrono>
#include <iostream>
#include <limits>
#include <thread>
#include <utility>
#include <vector>

// TODO: hook InputManager into real OS events and add gesture smoothing

namespace sc {

// Simple 2D point
struct Point {
    float x;
    float y;
};

// InputManager records gesture points for later recognition or training.
enum class TapAction {
    None,
    StartSequence,
    EndSymbol,
    EndSequence,
    LabelSymbol,
    RecordStream
};

class InputManager {
public:
    explicit InputManager(uint64_t intervalMs = 300)
        : m_capturing(false),
          m_lastTap(std::numeric_limits<uint64_t>::max()),
          m_doubleTapInterval(intervalMs),
          m_tapCount(0) {}

    // Handle a tap event with timestamp in milliseconds. Returns true if
    // a double tap was detected. Two taps start capture, a single tap
    // while capturing stops it.
    bool onTap(uint64_t timestamp) {
        if (timestamp < m_lastTap)
            m_lastTap = std::numeric_limits<uint64_t>::max(); // reset if timestamps go backwards

        if (m_capturing) {
            // any tap while capturing ends the capture
            stopCapture();
            m_lastTap = std::numeric_limits<uint64_t>::max();
            return false;
        }

        if (m_lastTap != std::numeric_limits<uint64_t>::max() &&
            timestamp - m_lastTap < m_doubleTapInterval) {
            startCapture();
            m_lastTap = std::numeric_limits<uint64_t>::max();
            return true;
        }

        m_lastTap = timestamp;
        return false;
    }

    TapAction onTapSequence(uint64_t timestamp) {
        if (timestamp < m_lastTap)
            m_tapCount = 0;

        if (!m_capturing) {
            if (m_lastTap != std::numeric_limits<uint64_t>::max() &&
                timestamp - m_lastTap < m_doubleTapInterval) {
                startCapture();
                m_lastTap = std::numeric_limits<uint64_t>::max();
                m_tapCount = 0;
                return TapAction::StartSequence;
            }
            m_lastTap = timestamp;
            return TapAction::None;
        }

        if (timestamp - m_lastTap > m_doubleTapInterval) {
            m_tapCount = 1;
        } else {
            ++m_tapCount;
        }
        m_lastTap = timestamp;

        switch (m_tapCount) {
        case 1:
            return TapAction::EndSymbol;
        case 2:
            stopCapture();
            m_tapCount = 0;
            m_lastTap = std::numeric_limits<uint64_t>::max();
            return TapAction::EndSequence;
        case 3:
            stopCapture();
            m_tapCount = 0;
            m_lastTap = std::numeric_limits<uint64_t>::max();
            return TapAction::LabelSymbol;
        case 4:
            stopCapture();
            m_tapCount = 0;
            m_lastTap = std::numeric_limits<uint64_t>::max();
            return TapAction::RecordStream;
        default:
            m_tapCount = 0;
            return TapAction::None;
        }
    }

    void startCapture() {
        m_points.clear();
        m_capturing = true;
    }

    void stopCapture() { m_capturing = false; }

    bool capturing() const { return m_capturing; }

    void setDoubleTapInterval(uint64_t interval) { m_doubleTapInterval = interval; }

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
    uint64_t m_doubleTapInterval;
    std::vector<Point> m_points;
    unsigned m_tapCount;
};

} // namespace sc
