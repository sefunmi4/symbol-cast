#ifndef CANVASWINDOW_HPP
#define CANVASWINDOW_HPP
#include "core/input/InputManager.hpp"
#include "core/recognition/ModelRunner.hpp"
#include "core/recognition/RecognizerRouter.hpp"
#include "core/recognition/GestureRecognizer.hpp"
#include "core/recognition/TrocrDecoder.hpp"
#include "utils/Logger.hpp"
#include <QCoreApplication>
#include <QClipboard>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPainterPath>
#include <QPushButton>
#include <QLabel>
#include <QColor>
#include <QCloseEvent>
#include <QHideEvent>
#include <QResizeEvent>
#include <QShowEvent>
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
#include <QTransform>
#include <QWidget>
#include <algorithm>
#include <vector>
#include <random>
#include <deque>
#include <unordered_map>
#include <memory>
#include <cstdint>

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
      : QWidget(parent), m_options(opts), m_hideOnClose(false),
        m_dragging(false), m_resizing(false), m_pressPending(false),
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
        tr("Double-tap to start. Double-tap again to submit."), this);
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
    loadPaletteConfig();
#ifdef SC_ENABLE_TROCR
    initializeTrocrDecoder();
#endif
    loadMacroBindingsFromConfig();
    setupSettingsMenu();
    updateMacroPanelGeometry();
    updateSettingsButtonGeometry();
  }

  void setHideOnClose(bool hide) { m_hideOnClose = hide; }

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

signals:
  void visibilityChanged(bool visible);

protected:
  void closeEvent(QCloseEvent *event) override {
    if (m_hideOnClose) {
      event->ignore();
      hide();
    } else {
      QWidget::closeEvent(event);
    }
  }

  void hideEvent(QHideEvent *event) override {
    QWidget::hideEvent(event);
    emit visibilityChanged(false);
  }

  void showEvent(QShowEvent *event) override {
    QWidget::showEvent(event);
    emit visibilityChanged(true);
  }

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
      finishActiveStrokes();
      m_strokes.push_back({});
      m_strokes.back().active = true;
      m_strokes.back().opacity = 1.f;
      m_strokes.back().addPoint(event->pos());
      m_input.addPoint(event->pos().x(), event->pos().y());
      m_label->hide();
      SC_LOG(sc::LogLevel::Info, "Sequence start");
      updatePrediction();
    } else if (act == sc::TapAction::EndSequence) {
      finishActiveStrokes();
      onSubmit();
      m_label->show();
      SC_LOG(sc::LogLevel::Info, "Sequence end");
    } else if (act == sc::TapAction::ResetDrawing) {
      finishActiveStrokes();
      resetRecognitionState();
      m_label->show();
      updatePrediction();
    } else if (act == sc::TapAction::LabelSymbol) {
      onTrainGesture();
    } else if (act == sc::TapAction::RecordStream) {
      SC_LOG(sc::LogLevel::Info, "Record stream" );
    } else if (nowCapturing) {
      if (m_strokes.empty())
        m_strokes.push_back({});
      if (!m_strokes.back().active) {
        finishActiveStrokes();
        m_strokes.back().active = true;
        m_strokes.back().opacity = 1.f;
      }
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
      if (!m_strokes.back().active) {
        finishActiveStrokes();
        m_strokes.back().active = true;
        m_strokes.back().opacity = 1.f;
      }
      m_strokes.back().addPoint(event->pos());
      m_input.addPoint(event->pos().x(), event->pos().y());
      if (static_cast<int>(sc::globalLogLevel()) <=
          static_cast<int>(sc::LogLevel::Debug)) {
        SC_LOG(sc::LogLevel::Debug,
               "Point " + std::to_string(event->pos().x()) + "," +
                   std::to_string(event->pos().y()));
      }
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
      float strokeOpacity = s.active ? 1.f : s.opacity;
      col.setAlphaF(col.alphaF() * strokeOpacity);
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
    std::string recognizedSymbol;
    std::string executedCommand;
    QString emittedGlyph;
    QString trocrGlyph;

#ifdef SC_ENABLE_TROCR
    if (m_trocrDecoder && m_trocrDecoder->available()) {
      QImage glyphImage = renderGlyphForTrocr();
      if (!glyphImage.isNull()) {
        std::u32string decoded = m_trocrDecoder->decode(glyphImage);
        for (char32_t cp : decoded) {
          if (cp == U'\0')
            continue;
          QChar qc = QChar::fromUcs4(cp);
          if (!qc.isNull() && qc.isSpace() && decoded.size() > 1)
            continue;
          trocrGlyph = mapCodepointToPalette(cp);
          if (!trocrGlyph.isEmpty())
            break;
        }
        if (trocrGlyph.isEmpty() && !decoded.empty())
          trocrGlyph = mapCodepointToPalette(decoded.front());
      }
    }
#endif

    std::string cmd = m_recognizer.commandForGesture(m_input.points());
    if (cmd.empty()) {
      std::string sym = m_router.recognize(m_input.points());
      if (!sym.empty()) {
        recognizedSymbol = sym;
        std::string macroCmd;
        if (triggerMacro(sym, trocrGlyph, macroCmd, emittedGlyph)) {
          executedCommand = macroCmd;
        } else {
          std::string routed = m_router.commandForSymbol(sym);
          if (!routed.empty()) {
            executedCommand = routed;
            showHoverFeedback(QString::fromStdString(routed));
          } else {
            if (!trocrGlyph.isEmpty())
              showHoverFeedback(trocrGlyph);
            else
              showHoverFeedback(QString::fromStdString(sym));
          }
        }
      }
    } else {
      executedCommand = cmd;
      auto prediction = m_recognizer.predictWithDistance(m_input.points());
      if (!prediction.first.empty())
        recognizedSymbol = prediction.first;
      if (!trocrGlyph.isEmpty())
        showHoverFeedback(trocrGlyph);
      else
        showHoverFeedback(QString::fromStdString(cmd));
    }

    if (emittedGlyph.isEmpty() && !trocrGlyph.isEmpty())
      emittedGlyph = trocrGlyph;

    if (!recognizedSymbol.empty() || !executedCommand.empty()) {
      std::string logMessage = "Detected symbol: " +
                               (recognizedSymbol.empty() ? std::string("<none>")
                                                         : recognizedSymbol);
      logMessage += ", action: " +
                    (executedCommand.empty() ? std::string("<none>")
                                             : executedCommand);
      if (!emittedGlyph.isEmpty())
        logMessage += ", glyph: " + emittedGlyph.toStdString();
      SC_LOG(sc::LogLevel::Info, logMessage);
    } else {
      SC_LOG(sc::LogLevel::Info,
             std::string("Detected symbol: <none>, action: <none>"));
    }
    resetRecognitionState();
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
    finishActiveStrokes();
    resetRecognitionState();
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
    for (auto &s : m_strokes) {
      if (!s.active)
        s.opacity -= m_options.fadeRate;
    }
    m_strokes.erase(std::remove_if(m_strokes.begin(), m_strokes.end(),
                                   [&](const Stroke &s) {
                                     return !s.active && s.opacity <= 0.f;
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
    QString output;
  };

  struct MacroUIRow {
    QComboBox *combo{nullptr};
    QLabel *glyphLabel{nullptr};
  };

  void resetIdleTimer() {
    m_idleTimer->stop();
    m_idleTimer->start();
    m_label->hide();
  }

  void finishActiveStrokes() {
    for (auto &stroke : m_strokes) {
      if (stroke.active)
        stroke.active = false;
    }
  }

  void resetRecognitionState() {
    m_input.clear();
    m_predictionPath = QPainterPath();
    m_predictionOpacity = 0.f;
    m_detectionRect = QRectF();
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
         !bindingIt->second.output.isEmpty())) {
      QString label = bindingIt->second.commandDisplay.isEmpty()
                           ? QString::fromStdString(sym)
                           : bindingIt->second.commandDisplay;
      if (!bindingIt->second.output.isEmpty())
        label += QStringLiteral(" (%1)").arg(bindingIt->second.output);
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

    QLabel *glyphLabel = new QLabel(tr("--"), row);
    glyphLabel->setStyleSheet("color:#FFFFFF;font-size:10px;");
    glyphLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    glyphLabel->setFixedWidth(48);
    rowLayout->addWidget(glyphLabel);

    layout->addWidget(row);
    m_macroRows[symbol] = {combo, glyphLabel};
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

  void setMacroBinding(const std::string &symbol, const MacroBinding &binding,
                       bool persist = true) {
    m_macroBindings[symbol] = binding;
    auto it = m_macroRows.find(symbol);
    if (it != m_macroRows.end() && it->second.glyphLabel) {
      QString text = binding.output.isEmpty() ? QStringLiteral("--")
                                              : binding.output;
      it->second.glyphLabel->setText(text);
    }
    if (persist)
      saveMacroBindings();
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
    } else if (id == QStringLiteral("paste")) {
      binding.id = id;
      binding.commandDisplay = tr("Paste");
      binding.commandAction = QStringLiteral("paste");
    }
    return binding;
  }

  MacroBinding defaultBindingForSymbol(const std::string &symbol) const {
    if (symbol == "triangle")
      return presetBinding(QStringLiteral("copy"));
    if (symbol == "circle" || symbol == "dot")
      return presetBinding(QStringLiteral("paste"));
    if (symbol == "square") {
      MacroBinding binding;
      binding.id = QStringLiteral("custom");
      binding.commandDisplay = tr("Custom");
      binding.output = QStringLiteral("?");
      return binding;
    }
    return MacroBinding();
  }

  QJsonObject readJsonObject(const QString &path) const {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
      return {};
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
      return {};
    return doc.object();
  }

  MacroBinding bindingFromJson(const QJsonObject &obj) const {
    MacroBinding binding;
    binding.id = obj.value(QStringLiteral("command")).toString();
    QString explicitId = obj.value(QStringLiteral("id")).toString();
    if (!explicitId.isEmpty())
      binding.id = explicitId;
    binding.commandDisplay = obj.value(QStringLiteral("display")).toString();
    binding.commandAction =
        obj.value(QStringLiteral("action")).toString(binding.id);
    binding.output = obj.value(QStringLiteral("output")).toString();
    if (binding.id == QStringLiteral("custom") &&
        binding.commandDisplay.isEmpty() && !binding.commandAction.isEmpty())
      binding.commandDisplay = binding.commandAction;
    return binding;
  }

  void loadMacroBindingsFromConfig() {
    QJsonObject defaults = readJsonObject(QStringLiteral("config/commands.json"));
    QJsonObject overrides = readJsonObject(QStringLiteral("data/macro_bindings.json"));
    QJsonObject merged = defaults;
    for (auto it = overrides.begin(); it != overrides.end(); ++it)
      merged.insert(it.key(), it.value());

    for (auto &entry : m_macroRows) {
      QString symbolKey = QString::fromStdString(entry.first);
      MacroBinding binding;
      QJsonValue value = merged.value(symbolKey);
      if (value.isObject()) {
        binding = bindingFromJson(value.toObject());
      } else if (value.isString()) {
        QString command = value.toString();
        binding = presetBinding(command);
        if (binding.id.isEmpty()) {
          binding.id = QStringLiteral("custom");
          binding.commandDisplay = command;
          binding.commandAction = command;
        }
      }
      if (binding.id.isEmpty())
        binding = defaultBindingForSymbol(entry.first);
      if (binding.id.isEmpty())
        binding.id = QStringLiteral("custom");
      setMacroBinding(entry.first, binding, false);
      setComboToId(entry.first, binding.id, true);
    }
  }

  void saveMacroBindings() const {
    if (m_macroBindings.empty())
      return;
    QJsonObject root;
    for (const auto &entry : m_macroBindings) {
      const MacroBinding &binding = entry.second;
      QJsonObject obj;
      if (!binding.id.isEmpty())
        obj.insert(QStringLiteral("command"), binding.id);
      if (!binding.commandDisplay.isEmpty())
        obj.insert(QStringLiteral("display"), binding.commandDisplay);
      if (!binding.commandAction.isEmpty())
        obj.insert(QStringLiteral("action"), binding.commandAction);
      if (!binding.output.isEmpty())
        obj.insert(QStringLiteral("output"), binding.output);
      root.insert(QString::fromStdString(entry.first), obj);
    }
    if (root.isEmpty())
      return;
    QDir().mkpath(QStringLiteral("data"));
    QFile file(QStringLiteral("data/macro_bindings.json"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
      return;
    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
  }

  void loadPaletteConfig() {
    m_paletteEntries.assign(256, QString());
    m_paletteDirect.clear();
    m_paletteOverrides.clear();
    for (int i = 0; i < 256; ++i) {
      char32_t cp = static_cast<char32_t>(i);
      QString ch = QString::fromUcs4(&cp, 1);
      m_paletteEntries[static_cast<size_t>(i)] = ch;
      m_paletteDirect[static_cast<uint32_t>(cp)] = ch;
    }

    QFile file(QStringLiteral("config/palette.json"));
    if (!file.open(QIODevice::ReadOnly))
      return;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull())
      return;
    if (doc.isArray()) {
      applyPaletteArray(doc.array());
      return;
    }
    if (doc.isObject()) {
      QJsonObject obj = doc.object();
      if (obj.value(QStringLiteral("palette")).isArray())
        applyPaletteArray(obj.value(QStringLiteral("palette")).toArray());
      if (obj.value(QStringLiteral("map")).isObject())
        applyPaletteOverrides(obj.value(QStringLiteral("map")).toObject());
    }
  }

  void applyPaletteArray(const QJsonArray &arr) {
    const int limit = std::min(256, arr.size());
    for (int i = 0; i < limit; ++i) {
      QString entry = arr.at(i).toString();
      if (entry.isEmpty()) {
        char32_t cp = static_cast<char32_t>(i);
        entry = QString::fromUcs4(&cp, 1);
      }
      m_paletteEntries[static_cast<size_t>(i)] = entry;
      auto codepoints = entry.toUcs4();
      if (!codepoints.empty())
        m_paletteDirect[static_cast<uint32_t>(codepoints.front())] = entry;
    }
  }

  void applyPaletteOverrides(const QJsonObject &obj) {
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      QString key = it.key();
      bool ok = false;
      uint32_t cp = 0;
      if (key.startsWith(QStringLiteral("0x")))
        cp = key.mid(2).toUInt(&ok, 16);
      else
        cp = key.toUInt(&ok, 10);
      if (!ok)
        continue;
      QString value = it.value().toString();
      if (!value.isEmpty())
        m_paletteOverrides[cp] = value;
    }
  }

  QString mapCodepointToPalette(char32_t codepoint) const {
    auto overrideIt = m_paletteOverrides.find(static_cast<uint32_t>(codepoint));
    if (overrideIt != m_paletteOverrides.end())
      return overrideIt->second;
    auto directIt = m_paletteDirect.find(static_cast<uint32_t>(codepoint));
    if (directIt != m_paletteDirect.end())
      return directIt->second;
    if (!m_paletteEntries.empty()) {
      size_t idx = static_cast<size_t>(codepoint) % m_paletteEntries.size();
      return m_paletteEntries[idx];
    }
    return QString::fromUcs4(&codepoint, 1);
  }

#ifdef SC_ENABLE_TROCR
  void initializeTrocrDecoder() {
    m_trocrInputSize = 384;
    QJsonObject cfg = readJsonObject(QStringLiteral("config/trocr.json"));
    QString modulePath = cfg.value(QStringLiteral("module")).toString();
    QString tokenizerPath = cfg.value(QStringLiteral("tokenizer")).toString();
    int configuredSize = cfg.value(QStringLiteral("input_size")).toInt();
    if (configuredSize > 0)
      m_trocrInputSize = configuredSize;
    if (!modulePath.isEmpty() && !tokenizerPath.isEmpty()) {
      m_trocrDecoder = std::make_unique<sc::TrocrDecoder>(
          modulePath.toStdString(), tokenizerPath.toStdString(), m_trocrInputSize);
    } else {
      m_trocrDecoder.reset();
    }
  }

  QImage renderGlyphForTrocr() const {
    if (!m_trocrDecoder || m_strokes.empty())
      return QImage();
    const int size = std::max(32, m_trocrInputSize);
    QImage image(size, size, QImage::Format_RGBA8888);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRectF bounds;
    bool hasBounds = false;
    for (const auto &stroke : m_strokes) {
      if (stroke.path.isEmpty())
        continue;
      if (!hasBounds) {
        bounds = stroke.path.boundingRect();
        hasBounds = true;
      } else {
        bounds = bounds.united(stroke.path.boundingRect());
      }
    }
    if (!hasBounds || bounds.width() <= 0.0 || bounds.height() <= 0.0)
      return image;

    const qreal marginRatio = 0.15;
    QRectF targetRect(marginRatio * size, marginRatio * size,
                      size * (1 - 2 * marginRatio),
                      size * (1 - 2 * marginRatio));
    QTransform transform;
    qreal scale = std::min(targetRect.width() / bounds.width(),
                           targetRect.height() / bounds.height());
    transform.translate(targetRect.center().x(), targetRect.center().y());
    transform.scale(scale, scale);
    transform.translate(-bounds.center().x(), -bounds.center().y());
    painter.setTransform(transform);

    QPen pen(Qt::black, std::max(1.5, m_options.strokeWidth * 0.8));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    for (const auto &stroke : m_strokes) {
      if (!stroke.path.isEmpty())
        painter.drawPath(stroke.path);
    }
    painter.end();
    return image;
  }
#else
  void initializeTrocrDecoder() {}
  QImage renderGlyphForTrocr() const { return QImage(); }
#endif

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

      QString glyphDefault = previous.output;
      QString glyphInput = QInputDialog::getText(
          this, tr("Character Mapping"),
          tr("Character to emit (optional):"), QLineEdit::Normal,
          glyphDefault, &ok);
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
      binding.output = glyphInput.isEmpty() ? glyphDefault : glyphInput;
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
      binding.output = previous.output;
      setMacroBinding(symbol, binding);
      int customIndex = combo->findData(QStringLiteral("custom"));
      if (customIndex >= 0)
        combo->setItemText(customIndex, tr("Custom..."));
    }
  }

  bool triggerMacro(const std::string &symbol, const QString &glyphOverride,
                    std::string &executedCommand, QString &emittedGlyph) {
    auto it = m_macroBindings.find(symbol);
    if (it == m_macroBindings.end())
      return false;
    const MacroBinding &binding = it->second;
    if (binding.id.isEmpty() && binding.commandDisplay.isEmpty() &&
        binding.commandAction.isEmpty() && binding.output.isEmpty())
      return false;

    QString outputText = glyphOverride.isEmpty() ? binding.output : glyphOverride;
    QString message;

    if (!outputText.isEmpty()) {
      emittedGlyph = outputText;
      if (QClipboard *clipboard = QGuiApplication::clipboard())
        clipboard->setText(outputText);
      m_macroBuffer = outputText;
    }

    if (binding.id == QStringLiteral("copy")) {
      if (outputText.isEmpty()) {
        QString fallback = binding.commandAction.isEmpty()
                                 ? QString::fromStdString(symbol)
                                 : binding.commandAction;
        if (!fallback.isEmpty()) {
          if (QClipboard *clipboard = QGuiApplication::clipboard())
            clipboard->setText(fallback);
          emittedGlyph = fallback;
          m_macroBuffer = fallback;
          outputText = fallback;
        }
      }
      message = outputText.isEmpty() ? tr("Copy") : tr("Copied %1").arg(outputText);
    } else if (binding.id == QStringLiteral("paste")) {
      if (outputText.isEmpty()) {
        if (m_macroBuffer.isEmpty()) {
          message = tr("Paste buffer empty");
        } else {
          if (QClipboard *clipboard = QGuiApplication::clipboard())
            clipboard->setText(m_macroBuffer);
          emittedGlyph = m_macroBuffer;
          outputText = m_macroBuffer;
          message = tr("Pasted %1").arg(m_macroBuffer);
        }
      } else {
        if (QClipboard *clipboard = QGuiApplication::clipboard())
          clipboard->setText(outputText);
        emittedGlyph = outputText;
        message = tr("Pasted %1").arg(outputText);
      }
    } else {
      QString displayName = binding.commandDisplay.isEmpty()
                                ? tr("Custom")
                                : binding.commandDisplay;
      message =
          tr("%1 → %2").arg(QString::fromStdString(symbol), displayName);
      if (!outputText.isEmpty())
        message += QStringLiteral(" (%1)").arg(outputText);
    }

    executedCommand = binding.commandAction.isEmpty()
                          ? binding.id.toStdString()
                          : binding.commandAction.toStdString();

    if (!message.isEmpty())
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
    bool active{false};

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
#ifdef SC_ENABLE_TROCR
  std::unique_ptr<sc::TrocrDecoder> m_trocrDecoder;
#endif
  int m_trocrInputSize{384};
  std::vector<QString> m_paletteEntries;
  std::unordered_map<uint32_t, QString> m_paletteDirect;
  std::unordered_map<uint32_t, QString> m_paletteOverrides;
  QString m_macroBuffer;
  QLabel *m_hoverLabel;
  QTimer *m_hoverTimer;
  bool m_showPrediction{true};
  QPainterPath m_predictionPath;
  float m_predictionOpacity{0.f};
  QRectF m_detectionRect;
  bool m_hideOnClose;
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
