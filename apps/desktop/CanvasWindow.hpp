#ifndef CANVASWINDOW_HPP
#define CANVASWINDOW_HPP
#include "core/input/InputManager.hpp"
#include "core/recognition/ModelRunner.hpp"
#include "utils/Logger.hpp"
#include <QCoreApplication>
#include <QDateTime>
#include <QKeySequence>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <vector>

class CanvasWindow : public QWidget {
  Q_OBJECT
public:
  explicit CanvasWindow(QWidget *parent = nullptr)
      : QWidget(parent), m_fadeOpacity(0.0), m_fading(false), m_dragging(false),
        m_resizing(false), m_resizeEdges(0), m_borderWidth(2) {
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint);
    setMouseTracking(true);
    setCursor(Qt::BlankCursor);
    setStyleSheet(QString("border:%1px solid #AAAAAA; border-radius:12px;")
                      .arg(m_borderWidth));
    // allow quick exit with Esc or Ctrl+C
    m_exitEsc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(m_exitEsc, &QShortcut::activated, qApp, &QCoreApplication::quit);
    m_exitCtrlC = new QShortcut(QKeySequence(QStringLiteral("Ctrl+C")), this);
    connect(m_exitCtrlC, &QShortcut::activated, qApp, &QCoreApplication::quit);
    int w = qEnvironmentVariableIntValue("SC_TRACKPAD_WIDTH");
    int h = qEnvironmentVariableIntValue("SC_TRACKPAD_HEIGHT");
    if (w <= 0)
      w = 400;
    if (h <= 0)
      h = 300;
    resize(w, h);
    m_button = new QPushButton("Submit", this);
    m_button->setFixedHeight(30);
    connect(m_button, &QPushButton::clicked, this, &CanvasWindow::onSubmit);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addStretch(1);
    layout->addWidget(m_button);
    layout->setContentsMargins(0, 0, 0, 0);
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this,
            QOverload<>::of(&CanvasWindow::onFrame));
    m_timer->start(16);
  }

  struct Ripple {
    QPointF pos;
    float radius{0.f};
    float opacity{1.f};
  };

  enum EdgeFlag {
    EdgeNone = 0x0,
    EdgeLeft = 0x1,
    EdgeTop = 0x2,
    EdgeRight = 0x4,
    EdgeBottom = 0x8
  };

protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      int edges = edgesForPos(event->pos());
      if (edges != EdgeNone) {
        m_resizing = true;
        m_resizeEdges = edges;
        m_originPos = event->globalPos();
        m_origRect = geometry();
        return;
      }
      if (inDragZone(event->pos())) {
        m_dragging = true;
        m_dragPos = event->globalPos() - frameGeometry().topLeft();
        return;
      }
    }

    const uint64_t ts =
        static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch());
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
      if (m_points.empty())
        m_points.push_back(event->pos());
    }
    update();
  }
  void mouseMoveEvent(QMouseEvent *event) override {
    if (m_dragging) {
      move(event->globalPos() - m_dragPos);
      return;
    }
    if (m_resizing) {
      QPoint delta = event->globalPos() - m_originPos;
      QRect r = m_origRect;
      if (m_resizeEdges & EdgeLeft)
        r.setLeft(r.left() + delta.x());
      if (m_resizeEdges & EdgeRight)
        r.setRight(r.right() + delta.x());
      if (m_resizeEdges & EdgeTop)
        r.setTop(r.top() + delta.y());
      if (m_resizeEdges & EdgeBottom)
        r.setBottom(r.bottom() + delta.y());
      setGeometry(r);
      return;
    }
    m_ripples.push_back({event->pos(), 0.f, 1.f});
    if (m_input.capturing()) {
      m_points.push_back(event->pos());
      m_input.addPoint(event->pos().x(), event->pos().y());
    }
    update();
  }
  void mouseReleaseEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      m_dragging = false;
      m_resizing = false;
    }
  }
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QRectF r = rect().adjusted(m_borderWidth / 2.0, m_borderWidth / 2.0,
                               -m_borderWidth / 2.0, -m_borderWidth / 2.0);
    QPainterPath path;
    path.addRoundedRect(r, 12, 12);
    p.fillPath(path, QColor(0, 0, 0, 30));
    QPen borderPen(QColor(170, 170, 170), m_borderWidth);
    p.setPen(borderPen);
    p.drawPath(path);
    p.setClipPath(path);
    // ripples
    for (auto &r : m_ripples) {
      QColor c(255, 255, 255, static_cast<int>(150 * r.opacity));
      p.setPen(Qt::NoPen);
      p.setBrush(c);
      p.drawEllipse(r.pos, r.radius, r.radius);
    }
    // drawing path
    if (m_points.size() >= 2) {
      QPen pen(QColor(0, 255, 0, 180), 2);
      p.setPen(pen);
      for (size_t i = 1; i < m_points.size(); ++i)
        p.drawLine(m_points[i - 1], m_points[i]);
    }
    if (!m_fadePoints.empty()) {
      QPen pen(QColor(0, 255, 0, static_cast<int>(180 * m_fadeOpacity)), 2);
      p.setPen(pen);
      for (size_t i = 1; i < m_fadePoints.size(); ++i)
        p.drawLine(m_fadePoints[i - 1], m_fadePoints[i]);
    }
  }

private slots:
  void onSubmit() {
    if (m_input.points().empty())
      return;
    sc::ModelRunner runner;
    if (!runner.loadModel("models/symbolcast-v1.onnx")) {
      sc::log(sc::LogLevel::Error, "Failed to load model");
      return;
    }
    auto sym = runner.run(m_input.points());
    sc::log(sc::LogLevel::Info, std::string("Detected symbol: ") + sym);
    m_input.clear();
    m_points.clear();
    update();
  }
  void onFrame() {
    for (auto &r : m_ripples) {
      r.radius += 2.0f;
      r.opacity -= 0.05f;
    }
    m_ripples.erase(
        std::remove_if(m_ripples.begin(), m_ripples.end(),
                       [](const Ripple &r) { return r.opacity <= 0.f; }),
        m_ripples.end());
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
  bool inDragZone(const QPoint &p) const {
    const int margin = 8;
    return p.x() < margin || p.x() > width() - margin || p.y() < margin ||
           p.y() > height() - margin;
  }

  int edgesForPos(const QPoint &p) const {
    const int margin = 6;
    int e = EdgeNone;
    if (p.x() <= margin)
      e |= EdgeLeft;
    if (p.x() >= width() - margin)
      e |= EdgeRight;
    if (p.y() <= margin)
      e |= EdgeTop;
    if (p.y() >= height() - margin)
      e |= EdgeBottom;
    return e;
  }

  sc::InputManager m_input;
  QPushButton *m_button;
  std::vector<QPointF> m_points;
  std::vector<QPointF> m_fadePoints;
  float m_fadeOpacity;
  bool m_fading;
  std::vector<Ripple> m_ripples;
  QTimer *m_timer;
  QShortcut *m_exitEsc;
  QShortcut *m_exitCtrlC;
  bool m_dragging;
  bool m_resizing;
  int m_resizeEdges;
  QPoint m_dragPos;
  QPoint m_originPos;
  QRect m_origRect;
  const int m_borderWidth;
};

#endif // CANVASWINDOW_HPP
