#ifndef CANVASWINDOW_HPP
#define CANVASWINDOW_HPP

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <vector>
#include "core/input/InputManager.hpp"
#include "core/recognition/ModelRunner.hpp"
#include "utils/Logger.hpp"

class CanvasWindow : public QWidget {
    Q_OBJECT
public:
    explicit CanvasWindow(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        resize(400, 400);
        m_button = new QPushButton("Submit", this);
        m_button->setFixedHeight(30);
        connect(m_button, &QPushButton::clicked, this, &CanvasWindow::onSubmit);
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->addStretch(1);
        layout->addWidget(m_button);
        layout->setContentsMargins(0, 0, 0, 0);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        m_input.startCapture();
        m_points.clear();
        m_points.push_back(event->pos());
        update();
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if (m_input.capturing()) {
            m_points.push_back(event->pos());
            m_input.addPoint(event->pos().x(), event->pos().y());
            update();
        }
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        Q_UNUSED(event);
        m_input.stopCapture();
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(0,0,0,30));
        if (m_points.size() < 2) return;
        QPen pen(Qt::red, 2);
        p.setPen(pen);
        for (size_t i = 1; i < m_points.size(); ++i) {
            p.drawLine(m_points[i-1], m_points[i]);
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

private:
    sc::InputManager m_input;
    QPushButton* m_button;
    std::vector<QPointF> m_points;
};

#endif // CANVASWINDOW_HPP
