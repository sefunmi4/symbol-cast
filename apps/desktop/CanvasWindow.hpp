#ifndef CANVASWINDOW_HPP
#define CANVASWINDOW_HPP

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QDateTime>
#include <QShortcut>
#include <QKeySequence>
#include <QCoreApplication>
#include <algorithm>
#include <vector>
#include "core/input/InputManager.hpp"
#include "core/recognition/ModelRunner.hpp"
#include "utils/Logger.hpp"

class CanvasWindow : public QWidget {
    Q_OBJECT
public:
    explicit CanvasWindow(QWidget* parent = nullptr)
        : QWidget(parent), m_fadeOpacity(0.0), m_fading(false)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setMouseTracking(true);
        setCursor(Qt::BlankCursor);
        // allow quick exit with Esc or Ctrl+C
        m_exitEsc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
        connect(m_exitEsc, &QShortcut::activated, qApp, &QCoreApplication::quit);
        m_exitCtrlC = new QShortcut(QKeySequence(QStringLiteral("Ctrl+C")), this);
        connect(m_exitCtrlC, &QShortcut::activated, qApp, &QCoreApplication::quit);
        int w = qEnvironmentVariableIntValue("SC_TRACKPAD_WIDTH");
        int h = qEnvironmentVariableIntValue("SC_TRACKPAD_HEIGHT");
        if (w <= 0) w = 400;
        if (h <= 0) h = 300;
        resize(w, h);
        m_button = new QPushButton("Submit", this);
        m_button->setFixedHeight(30);
        connect(m_button, &QPushButton::clicked, this, &CanvasWindow::onSubmit);
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->addStretch(1);
        layout->addWidget(m_button);
        layout->setContentsMargins(0, 0, 0, 0);
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, QOverload<>::of(&CanvasWindow::onFrame));
        m_timer->start(16);
    }

    struct Ripple {
        QPointF pos;
        float radius{0.f};
        float opacity{1.f};
    };

protected:
    void mousePressEvent(QMouseEvent* event) override {
        const uint64_t ts = static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch());
        bool dbl = m_input.onTap(ts);
        m_ripples.push_back({event->pos(), 0.f, 1.f});
        if (dbl) {
            if (m_input.capturing()) {
                m_points.clear();
                m_fadePoints.clear();
                m_fading = false;
                m_points.push_back(event->pos());
            } else {
                m_fadePoints = m_points;
                m_points.clear();
                m_fading = true;
                onSubmit();
            }
        } else if (m_input.capturing()) {
            if (m_points.empty()) m_points.push_back(event->pos());
        }
        update();
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        m_ripples.push_back({event->pos(), 0.f, 1.f});
        if (m_input.capturing()) {
            m_points.push_back(event->pos());
            m_input.addPoint(event->pos().x(), event->pos().y());
        }
        update();
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        Q_UNUSED(event);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(0,0,0,30));
        // ripples
        for (auto& r : m_ripples) {
            QColor c(255,255,255, static_cast<int>(150*r.opacity));
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawEllipse(r.pos, r.radius, r.radius);
        }
        // drawing path
        if (m_points.size() >= 2) {
            QPen pen(QColor(0,255,0,180), 2);
            p.setPen(pen);
            for (size_t i = 1; i < m_points.size(); ++i)
                p.drawLine(m_points[i-1], m_points[i]);
        }
        if (!m_fadePoints.empty()) {
            QPen pen(QColor(0,255,0, static_cast<int>(180*m_fadeOpacity)), 2);
            p.setPen(pen);
            for (size_t i = 1; i < m_fadePoints.size(); ++i)
                p.drawLine(m_fadePoints[i-1], m_fadePoints[i]);
        }
    }

private slots:
    void onSubmit() {
        if (m_input.points().empty()) return;
        sc::ModelRunner runner;
        if (!runner.loadModel("models/symbolcast-v1.onnx")) {
            sc::log(sc::LogLevel::Error, "Failed to load model");
            return;
        }
        auto sym = runner.run(m_input.points());
        sc::log(sc::LogLevel::Info, std::string("Detected symbol: ")+sym);
        m_input.clear();
        m_points.clear();
        update();
    }
    void onFrame() {
        for (auto& r : m_ripples) {
            r.radius += 2.0f;
            r.opacity -= 0.05f;
        }
        m_ripples.erase(std::remove_if(m_ripples.begin(), m_ripples.end(), [](const Ripple& r){return r.opacity <= 0.f;}), m_ripples.end());
        if (m_fading) {
            m_fadeOpacity -= 0.02f;
            if (m_fadeOpacity <= 0.f) {
                m_fadeOpacity = 0.f;
                m_fading = false;
                m_fadePoints.clear();
            }
        }
        update();
    }

private:
    sc::InputManager m_input;
    QPushButton* m_button;
    std::vector<QPointF> m_points;
    std::vector<QPointF> m_fadePoints;
    float m_fadeOpacity;
    bool m_fading;
    std::vector<Ripple> m_ripples;
    QTimer* m_timer;
    QShortcut* m_exitEsc;
    QShortcut* m_exitCtrlC;
};

#endif // CANVASWINDOW_HPP
