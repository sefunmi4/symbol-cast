#ifndef CANVASWINDOW_HPP
#define CANVASWINDOW_HPP
#include "core/input/InputManager.hpp"
#include "core/recognition/ModelRunner.hpp"
#include "core/recognition/RecognizerRouter.hpp"
#include "core/recognition/GestureRecognizer.hpp"
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
#include <QSize>
#include <QShortcut>
#include <QTimer>
#include <QInputDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QAction>
#include <QHBoxLayout>
#include <QMenu>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>
#include <QString>
#include <QWidget>
#include <algorithm>
#include <vector>
#include <random>
#include <deque>
#include <unordered_map>

struct CanvasWindowOptions {
  float rippleGrowthRate{2.f};
  float rippleMaxRadius{80.f};
  QColor rippleColor{255, 251, 224, 150};
  int strokeWidth{3};
  QColor strokeColor{255, 251, 224};
  float fadeRate{0.005f};
  QColor backgroundTint{34, 34, 34, 120};
  QColor detectionColor{255, 255, 255, 102};
  bool fullscreen{false};
  bool cursorAnimation{true};
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
    m_trainShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+T")), this);
    connect(m_trainShortcut, &QShortcut::activated, this,
            &CanvasWindow::onTrainGesture);
    m_undoShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Z")), this);
    connect(m_undoShortcut, &QShortcut::activated, this, [this] {
      if (m_recognizer.undo())
        m_recognizer.saveProfile("data/user_gestures.json");
    });
    m_redoShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Y")), this);
    connect(m_redoShortcut, &QShortcut::activated, this, [this] {
      if (m_recognizer.redo())
        m_recognizer.saveProfile("data/user_gestures.json");
    });
    m_recognizer.loadProfile("data/user_gestures.json");
    int w = qEnvironmentVariableIntValue("SC_TRACKPAD_WIDTH");
    int h = qEnvironmentVariableIntValue("SC_TRACKPAD_HEIGHT");
    if (w <= 0)
      w = 400;
    if (h <= 0)
      h = 300;
    resize(w, h);
    // Instruction label shown when idle
    m_label = new QLabel(
        tr("Double-tap to start. Single-tap ends symbol. Double-tap submits."),
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
    if (m_options.fullscreen) {
      m_closeBtn->hide();
      m_minBtn->hide();
      m_maxBtn->hide();
    }

    m_idleTimer = new QTimer(this);
    connect(m_idleTimer, &QTimer::timeout, this, [this] { m_label->show(); });
    m_idleTimer->setInterval(5000);
    m_idleTimer->start();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this,
            QOverload<>::of(&CanvasWindow::onFrame));
    // higher refresh rate for smoother drawing
    m_timer->start(10);

    m_hoverLabel = new QLabel(this);
    m_hoverLabel->setStyleSheet("color:#FFFFFF;background:rgba(0,0,0,80);"
                                "font-size:10px;border-radius:4px;padding:2px;");
    m_hoverLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_hoverLabel->hide();
    m_hoverTimer = new QTimer(this);
    m_hoverTimer->setSingleShot(true);
    connect(m_hoverTimer, &QTimer::timeout, m_hoverLabel, &QWidget::hide);

    setupMacroControls();
    setupSettingsMenu();
    updateMacroPanelGeometry();
    updateSettingsButtonGeometry();
  }

  struct Ripple {
    QPointF pos;
    float radius{0.f};
    float opacity{1.f};
  };

  struct TracePoint {
    QPointF pos;
    float life{1.f};
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
    if (isMacroRegion(event->pos()) || isSettingsRegion(event->pos())) {
      resetIdleTimer();
      QWidget::mousePressEvent(event);
      return;
    }
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
    sc::TapAction act = m_input.onTapSequence(ts);
    bool nowCapturing = m_input.capturing();
    if (m_options.cursorAnimation) {
      m_ripples.push_back({event->pos(), 0.f, 1.f});
      appendCursorTrace(event->pos());
    }
    if (act == sc::TapAction::StartSequence) {
      m_pressPending = false;
      m_dragging = false;
      m_strokes.clear();
      m_strokes.push_back({});
      m_strokes.back().addPoint(event->pos());
      m_input.addPoint(event->pos().x(), event->pos().y());
      m_label->hide();
      SC_LOG(sc::LogLevel::Info, "Sequence start");
      updatePrediction();
    } else if (act == sc::TapAction::EndSymbol) {
      onSubmit();
      m_input.clear();
      m_strokes.clear();
    } else if (act == sc::TapAction::EndSequence) {
      onSubmit();
      m_input.clear();
      m_strokes.clear();
      m_label->show();
      SC_LOG(sc::LogLevel::Info, "Sequence end");
    } else if (act == sc::TapAction::LabelSymbol) {
      onTrainGesture();
    } else if (act == sc::TapAction::RecordStream) {
      SC_LOG(sc::LogLevel::Info, "Record stream" );
    } else if (nowCapturing) {
      if (m_strokes.empty())
        m_strokes.push_back({});
      m_strokes.back().addPoint(event->pos());
      m_input.addPoint(event->pos().x(), event->pos().y());
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
    if (isMacroRegion(event->pos()) || isSettingsRegion(event->pos())) {
      resetIdleTimer();
      setCursor(Qt::ArrowCursor);
      QWidget::mouseMoveEvent(event);
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
    if (m_options.cursorAnimation) {
      m_ripples.push_back({event->pos(), 0.f, 1.f});
      appendCursorTrace(event->pos());
    }
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
    if (isMacroRegion(event->pos()) || isSettingsRegion(event->pos())) {
      QWidget::mouseReleaseEvent(event);
      resetIdleTimer();
      return;
    }
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
    updateMacroPanelGeometry();
    updateSettingsButtonGeometry();
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
    // cursor trace
    if (m_options.cursorAnimation && m_cursorTrace.size() >= 1) {
      p.save();
      for (size_t i = 1; i < m_cursorTrace.size(); ++i) {
        float life = (m_cursorTrace[i - 1].life + m_cursorTrace[i].life) / 2.f;
        if (life <= 0.f)
          continue;
        QColor col = m_options.strokeColor;
        col.setAlphaF(std::clamp(life * 0.6f, 0.f, 1.f));
        QPen tracePen(col, std::max(1.f, m_options.strokeWidth * 0.5f + life));
        tracePen.setCapStyle(Qt::RoundCap);
        tracePen.setJoinStyle(Qt::RoundJoin);
        p.setPen(tracePen);
        p.drawLine(m_cursorTrace[i - 1].pos, m_cursorTrace[i].pos);
      }
      const auto &head = m_cursorTrace.back();
      if (head.life > 0.f) {
        QColor headColor = m_options.strokeColor;
        headColor.setAlphaF(std::clamp(0.4f + head.life * 0.6f, 0.f, 1.f));
        p.setPen(Qt::NoPen);
        p.setBrush(headColor);
        p.drawEllipse(head.pos, 3.0, 3.0);
      }
      p.restore();
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
    std::string cmd = m_recognizer.commandForGesture(m_input.points());
    if (cmd.empty()) {
      std::string sym = m_router.recognize(m_input.points());
      if (!sym.empty()) {
        std::string macroCmd;
        if (triggerMacro(sym, macroCmd)) {
          cmd = macroCmd;
        } else {
          cmd = m_router.commandForSymbol(sym);
          if (!cmd.empty())
            showHoverFeedback(QString::fromStdString(cmd));
          else
            showHoverFeedback(QString::fromStdString(sym));
        }
      }
    } else {
      showHoverFeedback(QString::fromStdString(cmd));
    }
    SC_LOG(sc::LogLevel::Info, std::string("Detected command: ") + cmd);
    m_input.clear();
    m_detectionRect = QRectF();
    m_idleTimer->start();
    update();
  }
  void onTrainGesture() {
    if (m_input.points().empty())
      return;
    bool ok = false;
    QString label = QInputDialog::getText(this, tr("Label Gesture"), tr("Label:"),
                                         QLineEdit::Normal, QString(), &ok);
    if (!ok || label.isEmpty())
      return;
    QString cmd = QInputDialog::getText(this, tr("Command"), tr("Command:"),
                                        QLineEdit::Normal, QString(), &ok);
    if (!ok)
      return;

    int augment = QInputDialog::getInt(
        this, tr("Augment Samples"), tr("Extra random copies:"), 0, 0, 100, 1, &ok);
    if (!ok)
      augment = 0;

    m_recognizer.addSample(label.toStdString(), m_input.points(),
                           cmd.toStdString());

    if (augment > 0) {
      std::default_random_engine eng(std::random_device{}());
      std::normal_distribution<float> dist(0.f, 0.02f);
      for (int i = 0; i < augment; ++i) {
        std::vector<sc::Point> jittered;
        jittered.reserve(m_input.points().size());
        for (const auto &pt : m_input.points()) {
          jittered.push_back({pt.x + dist(eng), pt.y + dist(eng)});
        }
        m_recognizer.addSample(label.toStdString(), jittered, cmd.toStdString());
      }
    }

    m_recognizer.saveProfile("data/user_gestures.json");
    m_input.clear();
    m_strokes.clear();
    m_predictionPath = QPainterPath();
    m_detectionRect = QRectF();
    update();
  }
  void onFrame() {
    if (m_options.cursorAnimation) {
      for (auto &r : m_ripples) {
        r.radius += m_options.rippleGrowthRate;
        r.opacity -= 0.05f;
      }
      m_ripples.erase(
          std::remove_if(m_ripples.begin(), m_ripples.end(),
                         [&](const Ripple &r) {
                           return r.opacity <= 0.f ||
                                  r.radius >= m_options.rippleMaxRadius;
                         }),
          m_ripples.end());
      for (auto &tp : m_cursorTrace)
        tp.life -= 0.07f;
      while (!m_cursorTrace.empty() && m_cursorTrace.front().life <= 0.f)
        m_cursorTrace.pop_front();
    }
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
  struct MacroBinding {
    QString id;
    QString commandDisplay;
    QString commandAction;
    QChar character{QChar()};
  };

  struct MacroUIRow {
    QComboBox *combo{nullptr};
    QLabel *charLabel{nullptr};
  };

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
    std::string sym = m_router.recognize(m_input.points());
    if (sym.empty()) {
      m_predictionPath = QPainterPath();
      return;
    }
    auto bindingIt = m_macroBindings.find(sym);
    if (bindingIt != m_macroBindings.end() &&
        (!bindingIt->second.commandDisplay.isEmpty() ||
         !bindingIt->second.character.isNull())) {
      QString label = bindingIt->second.commandDisplay.isEmpty()
                           ? QString::fromStdString(sym)
                           : bindingIt->second.commandDisplay;
      if (!bindingIt->second.character.isNull())
        label += QStringLiteral(" (%1)").arg(bindingIt->second.character);
      showHoverFeedback(label);
    } else {
      showHoverFeedback(QString::fromStdString(sym));
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
    } else if (sym == "circle" || sym == "dot") {
      pred.addEllipse(box.center(), box.width() / 4.0, box.height() / 4.0);
    }
    m_predictionPath = pred;
    m_predictionOpacity = 1.f;
  }

  void showHoverFeedback(const QString &text) {
    m_hoverLabel->setText(text);
    m_hoverLabel->adjustSize();
    m_hoverLabel->move(width() - m_hoverLabel->width() - 10, 10);
    m_hoverLabel->show();
    m_hoverTimer->start(2000);
  }

  void appendCursorTrace(const QPointF &pos) {
    if (!m_cursorTrace.empty() &&
        (pos - m_cursorTrace.back().pos).manhattanLength() < 1.0)
      return;
    m_cursorTrace.push_back({pos, 1.f});
    constexpr size_t kMaxTracePoints = 80;
    if (m_cursorTrace.size() > kMaxTracePoints)
      m_cursorTrace.pop_front();
  }

  void setupMacroControls() {
    m_macroPanel = new QWidget(this);
    m_macroPanel->setAttribute(Qt::WA_StyledBackground, true);
    m_macroPanel->setStyleSheet(
        "background:rgba(0,0,0,120);border-radius:8px;padding:4px;");
    m_macroPanel->setCursor(Qt::ArrowCursor);

    auto *layout = new QVBoxLayout(m_macroPanel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    addMacroRow("triangle", tr("Triangle"), layout);
    addMacroRow("circle", tr("Circle"), layout);
    addMacroRow("square", tr("Square"), layout);

    setComboToId("triangle", QStringLiteral("copy"), true);
    setMacroBinding("triangle", presetBinding(QStringLiteral("copy")));

    setComboToId("circle", QStringLiteral("paste"), true);
    setMacroBinding("circle", presetBinding(QStringLiteral("paste")));

    setComboToId("square", QStringLiteral("custom"), true);
    MacroBinding customBinding;
    customBinding.id = QStringLiteral("custom");
    customBinding.commandDisplay = tr("Custom");
    customBinding.character = QChar('?');
    setMacroBinding("square", customBinding);

    for (auto &entry : m_macroRows) {
      if (!entry.second.combo)
        continue;
      connect(entry.second.combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
              this, [this, symbol = entry.first](int index) {
                onMacroSelection(symbol, index);
              });
    }

    m_macroPanel->hide();
  }

  void setupSettingsMenu() {
    m_settingsButton = new QToolButton(this);
    m_settingsButton->setText(tr("Settings"));
    m_settingsButton->setPopupMode(QToolButton::InstantPopup);
    m_settingsButton->setCursor(Qt::ArrowCursor);
    m_settingsButton->setStyleSheet(
        "QToolButton { color:#DDDDDD; background:rgba(0,0,0,40); border:1px solid "
        "rgba(255,255,255,30); border-radius:6px; padding:2px 10px; }"
        "QToolButton:hover { background:rgba(255,255,255,45); }");
    m_settingsButton->raise();

    m_settingsMenu = new QMenu(m_settingsButton);
    m_macroPanelAction =
        m_settingsMenu->addAction(tr("Show Shape Command Mapping"));
    connect(m_macroPanelAction, &QAction::triggered, this,
            &CanvasWindow::toggleMacroPanelVisibility);
    m_settingsButton->setMenu(m_settingsMenu);

    updateMacroPanelVisibilityAction();
  }

  void updateSettingsButtonGeometry() {
    if (!m_settingsButton)
      return;
    QSize size = m_settingsButton->sizeHint();
    int x = std::max(10, width() - size.width() - 20);
    int y = 6;
    m_settingsButton->setGeometry(x, y, size.width(), size.height());
    m_settingsButton->raise();
  }

  void toggleMacroPanelVisibility() {
    if (!m_macroPanel)
      return;
    if (m_macroPanel->isVisible()) {
      m_macroPanel->hide();
    } else {
      updateMacroPanelGeometry();
      m_macroPanel->show();
      m_macroPanel->raise();
    }
    updateMacroPanelVisibilityAction();
  }

  void updateMacroPanelVisibilityAction() {
    if (!m_macroPanelAction)
      return;
    bool visible = m_macroPanel && m_macroPanel->isVisible();
    if (visible)
      m_macroPanelAction->setText(tr("Hide Shape Command Mapping"));
    else
      m_macroPanelAction->setText(tr("Show Shape Command Mapping"));
  }

  void addMacroRow(const std::string &symbol, const QString &label,
                   QVBoxLayout *layout) {
    QWidget *row = new QWidget(m_macroPanel);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(6);

    QLabel *title = new QLabel(label, row);
    title->setStyleSheet("color:#FFFFFF;font-size:10px;");
    rowLayout->addWidget(title);

    QComboBox *combo = new QComboBox(row);
    combo->addItem(tr("Copy (Ctrl+C)"), QStringLiteral("copy"));
    combo->addItem(tr("Paste (Ctrl+V)"), QStringLiteral("paste"));
    combo->addItem(tr("Custom..."), QStringLiteral("custom"));
    combo->setCursor(Qt::ArrowCursor);
    rowLayout->addWidget(combo, 1);

    QLabel *charLabel = new QLabel(tr("--"), row);
    charLabel->setStyleSheet("color:#FFFFFF;font-size:10px;");
    charLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    charLabel->setFixedWidth(28);
    rowLayout->addWidget(charLabel);

    layout->addWidget(row);
    m_macroRows[symbol] = {combo, charLabel};
  }

  void updateMacroPanelGeometry() {
    if (!m_macroPanel)
      return;
    int panelWidth = 220;
    int x = std::max(10, width() - panelWidth - 20);
    int y = 40;
    int panelHeight = m_macroPanel->sizeHint().height();
    m_macroPanel->setGeometry(x, y, panelWidth, panelHeight);
    m_macroPanel->raise();
  }

  void setComboToId(const std::string &symbol, const QString &id,
                    bool blockSignals = false) {
    auto it = m_macroRows.find(symbol);
    if (it == m_macroRows.end() || !it->second.combo)
      return;
    QComboBox *combo = it->second.combo;
    int idx = combo->findData(id);
    if (idx < 0)
      return;
    if (blockSignals) {
      QSignalBlocker blocker(combo);
      combo->setCurrentIndex(idx);
    } else {
      combo->setCurrentIndex(idx);
    }
  }

  void setMacroBinding(const std::string &symbol, const MacroBinding &binding) {
    m_macroBindings[symbol] = binding;
    auto it = m_macroRows.find(symbol);
    if (it != m_macroRows.end() && it->second.charLabel) {
      QString text = binding.character.isNull()
                         ? QStringLiteral("--")
                         : QString(binding.character);
      it->second.charLabel->setText(text);
    }
  }

  MacroBinding currentMacroBinding(const std::string &symbol) const {
    auto it = m_macroBindings.find(symbol);
    if (it != m_macroBindings.end())
      return it->second;
    return MacroBinding();
  }

  MacroBinding presetBinding(const QString &id) const {
    MacroBinding binding;
    if (id == QStringLiteral("copy")) {
      binding.id = id;
      binding.commandDisplay = tr("Copy");
      binding.commandAction = QStringLiteral("copy");
      binding.character = QChar('C');
    } else if (id == QStringLiteral("paste")) {
      binding.id = id;
      binding.commandDisplay = tr("Paste");
      binding.commandAction = QStringLiteral("paste");
      binding.character = QChar('V');
    }
    return binding;
  }

  void restorePreviousSelection(const std::string &symbol, const QString &id) {
    if (id.isEmpty())
      return;
    setComboToId(symbol, id, true);
  }

  void onMacroSelection(const std::string &symbol, int index) {
    auto rowIt = m_macroRows.find(symbol);
    if (rowIt == m_macroRows.end() || !rowIt->second.combo)
      return;
    QComboBox *combo = rowIt->second.combo;
    QString selectedId = combo->itemData(index).toString();
    MacroBinding previous = currentMacroBinding(symbol);

    if (selectedId == QStringLiteral("custom")) {
      bool ok = false;
      QString defaultCommand = previous.id == QStringLiteral("custom")
                                   ? previous.commandAction
                                   : QString();
      QString commandInput = QInputDialog::getText(
          this, tr("Custom Command"), tr("Command label:"),
          QLineEdit::Normal, defaultCommand, &ok);
      if (!ok) {
        restorePreviousSelection(symbol, previous.id);
        return;
      }
      commandInput = commandInput.trimmed();

      QString charDefault = (!previous.character.isNull())
                                ? QString(previous.character)
                                : QString();
      QString charInput = QInputDialog::getText(
          this, tr("Character Mapping"),
          tr("Character to emit (optional):"), QLineEdit::Normal,
          charDefault, &ok);
      if (!ok) {
        restorePreviousSelection(symbol, previous.id);
        return;
      }

      MacroBinding binding;
      binding.id = QStringLiteral("custom");
      binding.commandAction = commandInput;
      binding.commandDisplay = commandInput.isEmpty()
                                   ? tr("Custom")
                                   : commandInput;
      if (!charInput.isEmpty())
        binding.character = charInput.front();
      else if (!charDefault.isEmpty())
        binding.character = charDefault.front();
      setMacroBinding(symbol, binding);

      if (!binding.commandAction.isEmpty())
        combo->setItemText(index,
                           tr("Custom: %1").arg(binding.commandDisplay));
      else
        combo->setItemText(index, tr("Custom..."));
    } else {
      MacroBinding binding = presetBinding(selectedId);
      if (binding.id.isEmpty()) {
        restorePreviousSelection(symbol, previous.id);
        return;
      }
      setMacroBinding(symbol, binding);
      int customIndex = combo->findData(QStringLiteral("custom"));
      if (customIndex >= 0)
        combo->setItemText(customIndex, tr("Custom..."));
    }
  }

  bool triggerMacro(const std::string &symbol, std::string &executedCommand) {
    auto it = m_macroBindings.find(symbol);
    if (it == m_macroBindings.end())
      return false;
    const MacroBinding &binding = it->second;
    if (binding.id.isEmpty() && binding.commandDisplay.isEmpty() &&
        binding.commandAction.isEmpty())
      return false;

    QString charText = binding.character.isNull()
                           ? QString()
                           : QString(binding.character);
    QString message;

    if (binding.id == QStringLiteral("copy")) {
      QString buffer = charText;
      if (buffer.isEmpty())
        buffer = binding.commandAction;
      if (buffer.isEmpty())
        buffer = QString::fromStdString(symbol);
      m_macroBuffer = buffer;
      message = tr("Copied %1").arg(buffer);
    } else if (binding.id == QStringLiteral("paste")) {
      if (m_macroBuffer.isEmpty())
        message = tr("Paste buffer empty");
      else
        message = tr("Pasted %1").arg(m_macroBuffer);
    } else {
      if (!binding.commandAction.isEmpty())
        m_macroBuffer = binding.commandAction;
      QString displayName = binding.commandDisplay.isEmpty()
                                ? tr("Custom")
                                : binding.commandDisplay;
      message =
          tr("%1 → %2").arg(QString::fromStdString(symbol), displayName);
      if (!charText.isEmpty())
        message += QStringLiteral(" (%1)").arg(charText);
    }

    executedCommand = binding.commandAction.isEmpty()
                          ? binding.id.toStdString()
                          : binding.commandAction.toStdString();

    showHoverFeedback(message);
    return true;
  }

  bool isMacroRegion(const QPoint &pos) const {
    return m_macroPanel && m_macroPanel->isVisible() &&
           m_macroPanel->geometry().contains(pos);
  }

  bool isSettingsRegion(const QPoint &pos) const {
    return m_settingsButton && m_settingsButton->isVisible() &&
           m_settingsButton->geometry().contains(pos);
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
  std::deque<TracePoint> m_cursorTrace;
  QTimer *m_timer;
  QTimer *m_idleTimer;
  QShortcut *m_exitEsc;
  QShortcut *m_exitCtrlC;
  QShortcut *m_togglePrediction;
  QShortcut *m_trainShortcut;
  QShortcut *m_undoShortcut;
  QShortcut *m_redoShortcut;
  sc::GestureRecognizer m_recognizer;
  sc::RecognizerRouter m_router;
  QWidget *m_macroPanel{nullptr};
  QToolButton *m_settingsButton{nullptr};
  QMenu *m_settingsMenu{nullptr};
  QAction *m_macroPanelAction{nullptr};
  std::unordered_map<std::string, MacroUIRow> m_macroRows;
  std::unordered_map<std::string, MacroBinding> m_macroBindings;
  QString m_macroBuffer;
  QLabel *m_hoverLabel;
  QTimer *m_hoverTimer;
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
