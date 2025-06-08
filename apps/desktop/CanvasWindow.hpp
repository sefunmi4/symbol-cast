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
#include <QLabel>
#include <QResizeEvent>
#include <QShortcut>
#include <QTimer>
#include <QWidget>
#include <algorithm>
#include <vector>

class CanvasWindow : public QWidget {
  Q_OBJECT
public:
  explicit CanvasWindow(QWidget *parent = nullptr)
      : QWidget(parent), m_dragging(false), m_resizing(false),
        m_resizeEdges(0), m_borderWidth(2) {
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
    // Instruction label shown when idle
    m_label = new QLabel(
        tr("Double-tap to start drawing. Draw a symbol, then double-tap again to submit."),
        this);
    m_label->setStyleSheet("color:#CCCCCC;font-size:12px;");
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_label->setGeometry(rect());
    m_label->show();

    // Mac-style window controls
    m_closeBtn = new QPushButton(this);
    m_minBtn = new QPushButton(this);
    m_maxBtn = new QPushButton(this);
    const QString common =
        "border-radius:6px;border:1px solid #00000030;";
    auto styleBtn = [&](QPushButton *b, const QString &color) {
      b->setFixedSize(12, 12);
      b->setStyleSheet(QString("background:%1;%2").arg(color, common));
    };
    styleBtn(m_closeBtn, "#ff5f57");
    styleBtn(m_minBtn, "#ffbd2e");
    styleBtn(m_maxBtn, "#28c840");
    m_closeBtn->setCursor(Qt::ArrowCursor);
    m_minBtn->setCursor(Qt::ArrowCursor);
    m_maxBtn->setCursor(Qt::ArrowCursor);
    m_closeBtn->move(8, 8);
    m_minBtn->move(26, 8);
    m_maxBtn->move(44, 8);
    connect(m_closeBtn, &QPushButton::clicked, this, &QWidget::close);
    connect(m_minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(m_maxBtn, &QPushButton::clicked, this, [this] {
      isMaximized() ? showNormal() : showMaximized();
    });

    m_idleTimer = new QTimer(this);
    connect(m_idleTimer, &QTimer::timeout, this, [this] { m_label->show(); });
    m_idleTimer->setInterval(5000);
    m_idleTimer->start();

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
        sc::log(sc::LogLevel::Info, "Resize start");
        m_resizing = true;
        m_resizeEdges = edges;
        m_originPos = event->globalPos();
        m_origRect = geometry();
        return;
      }
      if (!m_input.capturing()) {
        sc::log(sc::LogLevel::Info, "Drag start");
        m_dragging = true;
        m_dragPos = event->globalPos() - frameGeometry().topLeft();
        return;
      }
    }

    const uint64_t ts =
        static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch());
    resetIdleTimer();
    bool dbl = m_input.onTap(ts);
    m_ripples.push_back({event->pos(), 0.f, 1.f});
    if (dbl) {
      if (m_input.capturing()) {
        sc::log(sc::LogLevel::Info, "Capture started");
        m_strokes.clear();
        m_strokes.push_back({{event->pos()}, 1.f});
        m_input.addPoint(event->pos().x(), event->pos().y());
        sc::log(sc::LogLevel::Info, "Point " + std::to_string(event->pos().x()) + "," + std::to_string(event->pos().y()));
        m_label->hide();
      } else {
        sc::log(sc::LogLevel::Info, "Capture ended");
        onSubmit();
      }
    } else if (m_input.capturing()) {
      if (m_strokes.empty())
        m_strokes.push_back({});
      m_strokes.back().points.push_back(event->pos());
      m_input.addPoint(event->pos().x(), event->pos().y());
      sc::log(sc::LogLevel::Info, "Point " + std::to_string(event->pos().x()) + "," + std::to_string(event->pos().y()));
      m_label->hide();
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

    resetIdleTimer();
    int edges = edgesForPos(event->pos());
    if (!m_input.capturing() && !m_dragging && !m_resizing) {
      QWidget* c = childAt(event->pos());
      if (c == m_closeBtn || c == m_minBtn || c == m_maxBtn) {
        // leave button cursors untouched
      } else if (edges == (EdgeLeft | EdgeTop) || edges == (EdgeRight | EdgeBottom))
        setCursor(Qt::SizeFDiagCursor);
      else if (edges == (EdgeRight | EdgeTop) || edges == (EdgeLeft | EdgeBottom))
        setCursor(Qt::SizeBDiagCursor);
      else if (edges == EdgeLeft || edges == EdgeRight)
        setCursor(Qt::SizeHorCursor);
      else if (edges == EdgeTop || edges == EdgeBottom)
        setCursor(Qt::SizeVerCursor);
      else
        setCursor(Qt::BlankCursor);
    }

    m_ripples.push_back({event->pos(), 0.f, 1.f});
    if (m_input.capturing()) {
      if (m_strokes.empty())
        m_strokes.push_back({});
      m_strokes.back().points.push_back(event->pos());
      m_input.addPoint(event->pos().x(), event->pos().y());
      sc::log(sc::LogLevel::Info, "Point " + std::to_string(event->pos().x()) + "," + std::to_string(event->pos().y()));
      m_label->hide();
    }
    update();
  }
  void mouseReleaseEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      if (m_dragging)
        sc::log(sc::LogLevel::Info, "Drag end");
      if (m_resizing)
        sc::log(sc::LogLevel::Info, "Resize end");
      m_dragging = false;
      m_resizing = false;
      resetIdleTimer();
    }
  }
  void resizeEvent(QResizeEvent *event) override {
    QWidget::resizeEvent(event);
    m_label->setGeometry(rect());
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
    // strokes
    for (const auto &s : m_strokes) {
      if (s.points.empty())
        continue;
      QColor col(255, 255, 255, static_cast<int>(255 * s.opacity));
      QPen pen(col, 3);
      p.setPen(pen);
      if (s.points.size() == 1) {
        p.drawEllipse(s.points[0], 2, 2);
      } else {
        for (size_t i = 1; i < s.points.size(); ++i)
          p.drawLine(s.points[i - 1], s.points[i]);
      }
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
    auto cmd = runner.commandForSymbol(sym);
    sc::log(sc::LogLevel::Info, std::string("Detected symbol: ") + sym +
                                     ", command: " + cmd);
    m_input.clear();
    m_idleTimer->start();
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
    for (auto &s : m_strokes)
      s.opacity -= m_fadeRate;
    m_strokes.erase(std::remove_if(m_strokes.begin(), m_strokes.end(),
                                   [&](const Stroke &s) {
                                     return s.opacity <= 0.f;
                                   }),
                    m_strokes.end());
    update();
  }

private:
  void resetIdleTimer() {
    m_idleTimer->stop();
    m_idleTimer->start();
    m_label->hide();
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
  QLabel *m_label;
  QPushButton *m_closeBtn;
  QPushButton *m_minBtn;
  QPushButton *m_maxBtn;
  struct Stroke {
    std::vector<QPointF> points;
    float opacity{1.f};
  };
  std::vector<Stroke> m_strokes;
  float m_fadeRate{0.005f};
  std::vector<Ripple> m_ripples;
  QTimer *m_timer;
  QTimer *m_idleTimer;
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
