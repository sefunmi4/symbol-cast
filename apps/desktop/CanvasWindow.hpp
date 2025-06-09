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
#include <QColor>
#include <QResizeEvent>
#include <QShortcut>
#include <QTimer>
#include <QWidget>
#include <algorithm>
#include <vector>

struct CanvasWindowOptions {
  float rippleGrowthRate{2.f};
  float rippleMaxRadius{80.f};
  QColor rippleColor{255, 251, 224, 150};
  int strokeWidth{3};
  QColor strokeColor{255, 251, 224};
  float fadeRate{0.005f};
  QColor backgroundTint{34, 34, 34, 120};
  QColor detectionColor{255, 255, 255, 102};
};

class CanvasWindow : public QWidget {
  Q_OBJECT
public:

  explicit CanvasWindow(const CanvasWindowOptions &opts = CanvasWindowOptions(), QWidget *parent = nullptr)
      : QWidget(parent), m_options(opts), m_dragging(false), m_resizing(false),
        m_pressPending(false), m_resizeEdges(0), m_borderWidth(2) {
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
    m_togglePrediction = new QShortcut(QKeySequence(Qt::Key_P), this);
    connect(m_togglePrediction, &QShortcut::activated, this, [this] {
      m_showPrediction = !m_showPrediction;
      if (!m_showPrediction)
        m_predictionPath = QPainterPath();
    });
    int w = qEnvironmentVariableIntValue("SC_TRACKPAD_WIDTH");
    int h = qEnvironmentVariableIntValue("SC_TRACKPAD_HEIGHT");
    if (w <= 0)
      w = 400;
    if (h <= 0)
      h = 300;
    resize(w, h);
    // Instruction label shown when idle
    m_label = new QLabel(
        tr("Double-tap to start drawing. Draw a symbol, then tap once to submit."),
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
    // higher refresh rate for smoother drawing
    m_timer->start(10);
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
        SC_LOG(sc::LogLevel::Info, "Resize start");
        m_resizing = true;
        m_resizeEdges = edges;
        m_originPos = event->globalPos();
        m_origRect = geometry();
        return;
      }
      if (!m_input.capturing()) {
        m_pressPending = true;
        m_pressPos = event->globalPos();
        m_dragPos = event->globalPos() - frameGeometry().topLeft();
      }
    }

    const uint64_t ts =
        static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch());
    resetIdleTimer();
    bool wasCapturing = m_input.capturing();
    bool dbl = m_input.onTap(ts);
    bool nowCapturing = m_input.capturing();
    m_ripples.push_back({event->pos(), 0.f, 1.f});
    if (!wasCapturing && nowCapturing) {
      m_pressPending = false;
      m_dragging = false;
      SC_LOG(sc::LogLevel::Info, "Capture started");
      m_strokes.clear();
      m_strokes.push_back({});
      m_strokes.back().addPoint(event->pos());
      m_input.addPoint(event->pos().x(), event->pos().y());
      SC_LOG(sc::LogLevel::Info, "Point " + std::to_string(event->pos().x()) + "," + std::to_string(event->pos().y()));
      m_label->hide();
      updatePrediction();
    } else if (wasCapturing && !nowCapturing) {
      SC_LOG(sc::LogLevel::Info, "Capture ended");
      onSubmit();
      m_predictionPath = QPainterPath();
    } else if (nowCapturing) {
      if (m_strokes.empty())
        m_strokes.push_back({});
      m_strokes.back().addPoint(event->pos());
      m_input.addPoint(event->pos().x(), event->pos().y());
      SC_LOG(sc::LogLevel::Info, "Point " + std::to_string(event->pos().x()) + "," + std::to_string(event->pos().y()));
      m_label->hide();
      updatePrediction();
    }
    update();
  }
  void mouseMoveEvent(QMouseEvent *event) override {
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
    if (m_dragging) {
      move(event->globalPos() - m_dragPos);
      return;
    }
    if (m_pressPending && (event->buttons() & Qt::LeftButton)) {
      if ((event->globalPos() - m_pressPos).manhattanLength() > 3) {
        SC_LOG(sc::LogLevel::Info, "Drag start");
        m_dragging = true;
        m_pressPending = false;
      }
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
      m_strokes.back().addPoint(event->pos());
      m_input.addPoint(event->pos().x(), event->pos().y());
      SC_LOG(sc::LogLevel::Info, "Point " + std::to_string(event->pos().x()) + "," + std::to_string(event->pos().y()));
      m_label->hide();
      updatePrediction();
    }
    update();
  }
  void mouseReleaseEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      if (m_dragging)
        SC_LOG(sc::LogLevel::Info, "Drag end");
      if (m_resizing)
        SC_LOG(sc::LogLevel::Info, "Resize end");
      m_dragging = false;
      m_resizing = false;
      m_pressPending = false;
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
    p.fillPath(path, m_options.backgroundTint);
    QPen borderPen(QColor(170, 170, 170), m_borderWidth);
    p.setPen(borderPen);
    p.drawPath(path);
    p.setClipPath(path);
    // ripples
    for (auto &r : m_ripples) {
      QColor c = m_options.rippleColor;
      c.setAlphaF(c.alphaF() * r.opacity);
      p.setPen(Qt::NoPen);
      p.setBrush(c);
      p.drawEllipse(r.pos, r.radius, r.radius);
    }
    // strokes
    for (const auto &s : m_strokes) {
      if (s.points.empty())
        continue;
      QColor col = m_options.strokeColor;
      col.setAlphaF(col.alphaF() * s.opacity);
      QPen pen(col, m_options.strokeWidth);
      p.setPen(pen);
      if (s.path.isEmpty()) {
        p.drawEllipse(s.points.front(), 2, 2);
      } else {
        p.drawPath(s.path);
      }
    }
    if (!m_detectionRect.isNull()) {
      QPen boxPen(m_options.detectionColor, 1, Qt::DashLine);
      boxPen.setCapStyle(Qt::RoundCap);
      boxPen.setJoinStyle(Qt::RoundJoin);
      p.setPen(boxPen);
      p.setBrush(Qt::NoBrush);
      p.drawRect(m_detectionRect);
    }
    if (!m_predictionPath.isEmpty() && m_predictionOpacity > 0.f) {
      QColor predColor(255, 255, 255);
      predColor.setAlphaF(m_predictionOpacity);
      QPen dashPen(predColor, 1, Qt::DashLine);
      p.setPen(dashPen);
      p.drawPath(m_predictionPath);
    }
  }

private slots:
  void onSubmit() {
    if (m_input.points().empty())
      return;
    sc::ModelRunner runner;
    if (!runner.loadModel("models/symbolcast-v1.onnx")) {
      SC_LOG(sc::LogLevel::Error, "Failed to load model");
      return;
    }
    auto sym = runner.run(m_input.points());
    auto cmd = runner.commandForSymbol(sym);
    SC_LOG(sc::LogLevel::Info, std::string("Detected symbol: ") + sym +
                                     ", command: " + cmd);
    m_input.clear();
    m_detectionRect = QRectF();
    m_idleTimer->start();
    update();
  }
  void onFrame() {
    for (auto &r : m_ripples) {
      r.radius += m_options.rippleGrowthRate;
      r.opacity -= 0.05f;
    }
    m_ripples.erase(
        std::remove_if(m_ripples.begin(), m_ripples.end(), [&](const Ripple &r) {
          return r.opacity <= 0.f || r.radius >= m_options.rippleMaxRadius;
        }),
        m_ripples.end());
    for (auto &s : m_strokes)
      s.opacity -= m_options.fadeRate;
    m_strokes.erase(std::remove_if(m_strokes.begin(), m_strokes.end(),
                                   [&](const Stroke &s) {
                                     return s.opacity <= 0.f;
                                   }),
                    m_strokes.end());
    if (m_predictionOpacity > 0.f)
      m_predictionOpacity -= 0.05f;
    if (m_predictionOpacity < 0.f)
      m_predictionOpacity = 0.f;
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

  void updatePrediction() {
    if (!m_showPrediction || m_input.points().empty()) {
      m_predictionPath = QPainterPath();
      m_detectionRect = QRectF();
      return;
    }
    sc::ModelRunner runner;
    if (!runner.loadModel("models/symbolcast-v1.onnx")) {
      m_predictionPath = QPainterPath();
      return;
    }
    std::string sym = runner.run(m_input.points());
    if (sym.empty()) {
      m_predictionPath = QPainterPath();
      return;
    }
    float minX = m_input.points()[0].x;
    float minY = m_input.points()[0].y;
    float maxX = minX;
    float maxY = minY;
    for (const auto &pt : m_input.points()) {
      minX = std::min(minX, pt.x);
      minY = std::min(minY, pt.y);
      maxX = std::max(maxX, pt.x);
      maxY = std::max(maxY, pt.y);
    }
    QRectF box(QPointF(minX, minY), QPointF(maxX, maxY));
    m_detectionRect = box;
    box.adjust(-10, -10, 10, 10);
    QPainterPath pred;
    if (sym == "triangle") {
      pred.moveTo(box.center().x(), box.top());
      pred.lineTo(box.bottomRight());
      pred.lineTo(box.bottomLeft());
      pred.closeSubpath();
    } else if (sym == "square") {
      pred.addRect(box);
    } else if (sym == "dot") {
      pred.addEllipse(box.center(), box.width() / 4.0, box.height() / 4.0);
    }
    m_predictionPath = pred;
    m_predictionOpacity = 1.f;
  }

  sc::InputManager m_input;
  QLabel *m_label;
  QPushButton *m_closeBtn;
  QPushButton *m_minBtn;
  QPushButton *m_maxBtn;
  struct Stroke {
    std::vector<QPointF> points;
    QPainterPath path;
    float opacity{1.f};

    void addPoint(const QPointF &p) {
      points.push_back(p);
      rebuildPath();
    }

    void rebuildPath() {
      path = buildPath(points);
    }

    static QPainterPath buildPath(const std::vector<QPointF> &pts) {
      QPainterPath p;
      if (pts.empty())
        return p;

      std::vector<QPointF> smoothed;
      smoothed.reserve(pts.size());
      for (size_t i = 0; i < pts.size(); ++i) {
        if (i < 2) {
          smoothed.push_back(pts[i]);
        } else {
          QPointF avg = (pts[i] + pts[i - 1] + pts[i - 2]) / 3.0;
          smoothed.push_back(avg);
        }
      }

      p.moveTo(smoothed[0]);
      if (smoothed.size() == 1)
        return p;

      for (size_t i = 1; i < smoothed.size() - 1; ++i) {
        QPointF mid = (smoothed[i] + smoothed[i + 1]) / 2.0;
        p.quadTo(smoothed[i], mid);
      }
      p.lineTo(smoothed.back());
      return p;
    }
  };
  std::vector<Stroke> m_strokes;
  CanvasWindowOptions m_options;
  std::vector<Ripple> m_ripples;
  QTimer *m_timer;
  QTimer *m_idleTimer;
  QShortcut *m_exitEsc;
  QShortcut *m_exitCtrlC;
  QShortcut *m_togglePrediction;
  bool m_showPrediction{true};
  QPainterPath m_predictionPath;
  float m_predictionOpacity{0.f};
  QRectF m_detectionRect;
  bool m_dragging;
  bool m_resizing;
  bool m_pressPending;
  int m_resizeEdges;
  QPoint m_dragPos;
  QPoint m_pressPos;
  QPoint m_originPos;
  QRect m_origRect;
  const int m_borderWidth;
};

#endif // CANVASWINDOW_HPP
