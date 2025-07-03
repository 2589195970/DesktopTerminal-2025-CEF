#include "loading_dialog.h"
#include "../logging/logger.h"

#include <QApplication>
#include <QScreen>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QStyleOption>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QFont>
#include <QFontMetrics>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QtMath>

LoadingDialog::LoadingDialog(QWidget *parent)
    : QDialog(parent)
    , m_mainLayout(nullptr)
    , m_buttonLayout(nullptr)
    , m_headerLayout(nullptr)
    , m_iconLayout(nullptr)
    , m_stepsLayout(nullptr)
    , m_iconLabel(nullptr)
    , m_loadingRingLabel(nullptr)
    , m_titleLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_subtitleLabel(nullptr)
    , m_progressBar(nullptr)
    , m_progressLabel(nullptr)
    , m_detailsText(nullptr)
    , m_retryButton(nullptr)
    , m_cancelButton(nullptr)
    , m_detailsButton(nullptr)
    , m_loadingAnimationTimer(nullptr)
    , m_progressAnimationTimer(nullptr)
    , m_iconAnimation(nullptr)
    , m_progressAnimation(nullptr)
    , m_stateTransitionGroup(nullptr)
    , m_loadingFrame(0)
    , m_progressFrame(0)
    , m_currentState(Initializing)
    , m_cancellable(false)
    , m_detailsVisible(false)
    , m_shadowEffect(nullptr)
{
    setupUI();
    setupAnimations();
    setupShadowEffects();
    updateStateText();
    updateStateIcon();
    
    // 设置窗口属性
    setWindowTitle("智多分机考桌面端 - 正在启动");
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);
    setModal(true);
    
    // 参照现代UI设计，设置宽敞的尺寸比例 - 至少712x614
    QSize windowSize = scaledWindowSize(720, 600);
    resize(windowSize);
    setMinimumSize(scaledWindowSize(650, 500));
    
    // 居中显示
    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }
    
    // 应用现代化样式将在setupShadowEffects中处理
    
    // 启动加载动画
    startLoadingAnimation();
}

LoadingDialog::~LoadingDialog()
{
    stopLoadingAnimation();
    
    if (m_stateTransitionGroup) {
        delete m_stateTransitionGroup;
    }
}

void LoadingDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(scaledSize(0));
    m_mainLayout->setContentsMargins(scaledSize(80), scaledSize(100), scaledSize(80), scaledSize(80));
    
    // 顶部大量留白空间
    m_mainLayout->addStretch(3);
    
    // 图标区域 - 更大尺寸适应大窗口
    m_iconLabel = new QLabel();
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setFixedSize(scaledSize(140), scaledSize(140));
    m_iconLabel->setObjectName("iconLabel");
    
    // Loading环标签
    m_loadingRingLabel = new QLabel();
    m_loadingRingLabel->setAlignment(Qt::AlignCenter);
    m_loadingRingLabel->setFixedSize(scaledSize(140), scaledSize(140));
    m_loadingRingLabel->setObjectName("loadingRingLabel");
    m_loadingRingLabel->setVisible(false);
    
    m_mainLayout->addWidget(m_iconLabel);
    m_mainLayout->addSpacing(scaledSize(60));
    
    // 状态标签 - 更大字体适应大窗口
    m_statusLabel = new QLabel("正在启动");
    m_statusLabel->setObjectName("statusLabel");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    QFont statusFont = m_statusLabel->font();
    statusFont.setPointSize(scaledFont(32));
    statusFont.setWeight(QFont::Medium);
    m_statusLabel->setFont(statusFont);
    m_mainLayout->addWidget(m_statusLabel);
    
    m_mainLayout->addSpacing(scaledSize(25));
    
    // 副标题 - 适当增大字体
    m_subtitleLabel = new QLabel("正在初始化应用程序...");
    m_subtitleLabel->setObjectName("subtitleLabel");
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    m_subtitleLabel->setWordWrap(true);
    QFont subtitleFont = m_subtitleLabel->font();
    subtitleFont.setPointSize(scaledFont(18));
    m_subtitleLabel->setFont(subtitleFont);
    m_mainLayout->addWidget(m_subtitleLabel);
    
    // 隐藏不必要的标题（为了简洁设计）
    m_titleLabel = new QLabel();
    m_titleLabel->setVisible(false);
    
    // 中间大量留白空间
    m_mainLayout->addStretch(2);
    
    // 进度条 - 增加高度使其在大窗口中更明显
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(10);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(scaledSize(8));
    m_progressBar->setObjectName("modernProgressBar");
    m_mainLayout->addWidget(m_progressBar);
    
    // 底部留白空间
    m_mainLayout->addStretch(1);
    
    // 隐藏不必要的组件
    m_progressLabel = new QLabel();
    m_progressLabel->setVisible(false);
    
    // 简化步骤指示器为不可见
    m_stepsLayout = new QHBoxLayout();
    setupProgressSteps(); // 创建但隐藏
    
    // 详细信息文本（初始隐藏）- 适应大窗口
    m_detailsText = new QTextEdit();
    m_detailsText->setVisible(false);
    m_detailsText->setReadOnly(true);
    m_detailsText->setMaximumHeight(scaledSize(180));
    m_detailsText->setObjectName("detailsText");
    QFont detailsFont("Consolas", scaledFont(11));
    m_detailsText->setFont(detailsFont);
    
    // 按钮区域 - 居中布局，适应大窗口
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setSpacing(scaledSize(25));
    m_buttonLayout->setAlignment(Qt::AlignCenter);
    
    m_retryButton = new QPushButton("重试");
    m_retryButton->setVisible(false);
    m_retryButton->setObjectName("primaryButton");
    m_retryButton->setFixedSize(scaledSize(140), scaledSize(48));
    connect(m_retryButton, &QPushButton::clicked, this, &LoadingDialog::onRetryClicked);
    
    m_detailsButton = new QPushButton("详细信息");
    m_detailsButton->setVisible(false);
    m_detailsButton->setObjectName("secondaryButton");
    m_detailsButton->setFixedSize(scaledSize(140), scaledSize(48));
    connect(m_detailsButton, &QPushButton::clicked, this, &LoadingDialog::onDetailsToggled);
    
    m_cancelButton = new QPushButton("取消");
    m_cancelButton->setVisible(false);
    m_cancelButton->setObjectName("secondaryButton");
    m_cancelButton->setFixedSize(scaledSize(140), scaledSize(48));
    connect(m_cancelButton, &QPushButton::clicked, this, &LoadingDialog::onCancelClicked);
    
    // 在错误状态时才添加按钮和详细信息到布局
    // 平时保持简洁布局
}

void LoadingDialog::setupProgressSteps()
{
    // 简洁设计 - 不显示步骤指示器
    // 创建空的步骤标签以保持兼容性
    for (int i = 0; i < 6; ++i) {
        QLabel *stepDot = new QLabel();
        stepDot->setVisible(false); // 隐藏所有步骤指示器
        m_stepLabels.append(stepDot);
    }
}

void LoadingDialog::setupAnimations()
{
    // Loading环旋转动画
    m_loadingAnimationTimer = new QTimer(this);
    connect(m_loadingAnimationTimer, &QTimer::timeout, this, &LoadingDialog::updateLoadingAnimation);
    
    // 进度条流光动画
    m_progressAnimationTimer = new QTimer(this);
    connect(m_progressAnimationTimer, &QTimer::timeout, this, &LoadingDialog::updateProgressAnimation);
    
    // 状态切换动画组
    m_stateTransitionGroup = new QSequentialAnimationGroup(this);
}

void LoadingDialog::setupShadowEffects()
{
    // Windows 7兼容性：禁用QGraphicsDropShadowEffect
    // 该特效在Windows 7上会导致严重性能问题（FPS从300降到30）
    // 改用CSS样式实现视觉效果
    m_shadowEffect = nullptr;
    
    // 通过CSS边框和背景渐变替代阴影效果
    QString shadowStyle = QString(R"(
        QDialog {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #f8fafc, stop:0.5 #f1f5f9, stop:1 #e2e8f0);
            border: 2px solid #cbd5e1;
            border-radius: %1px;
        }
    )").arg(scaledSize(12));
    
    // 合并阴影样式和其他组件样式
    QString fullStyle = shadowStyle + getModernStyleSheet().replace(
        QRegExp("QDialog\\s*\\{[^}]*\\}"), ""); // 移除QDialog样式块避免冲突
    setStyleSheet(fullStyle);
}

void LoadingDialog::startLoadingAnimation()
{
    if (m_currentState != Completed && m_currentState != Failed) {
        m_loadingAnimationTimer->start(50); // 50ms间隔，更流畅
        m_progressAnimationTimer->start(100);
    }
}

void LoadingDialog::stopLoadingAnimation()
{
    if (m_loadingAnimationTimer) {
        m_loadingAnimationTimer->stop();
    }
    if (m_progressAnimationTimer) {
        m_progressAnimationTimer->stop();
    }
}

void LoadingDialog::updateLoadingAnimation()
{
    // 创建旋转的loading环
    QPixmap loadingRing = createLoadingRing(m_loadingFrame);
    m_loadingRingLabel->setPixmap(loadingRing);
    
    m_loadingFrame = (m_loadingFrame + 1) % 72; // 72帧完整旋转
}

void LoadingDialog::updateProgressAnimation()
{
    // 进度条流光效果通过样式表实现
    m_progressFrame = (m_progressFrame + 1) % 100;
}

QPixmap LoadingDialog::createLoadingRing(int frame) const
{
    int size = scaledSize(140);
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    painter.translate(size/2, size/2);
    
    // 简洁的旋转环动画 - 参照现代UI风格
    painter.rotate(frame * 6); // 适中的旋转速度
    
    QPen pen(getStateColor(m_currentState), scaledSize(3));
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    
    // 绘制简洁的圆弧 - 适应更大的尺寸
    painter.drawArc(-scaledSize(45), -scaledSize(45), scaledSize(90), scaledSize(90), 0, 270 * 16);
    
    return pixmap;
}

// DPI感知工具函数实现
double LoadingDialog::getDpiScale() const
{
    QScreen* screen = QApplication::primaryScreen();
    if (!screen) return 1.0;
    
    double dpiScale = screen->devicePixelRatio();
    
    // 在高DPI情况下适度缩放
    if (dpiScale >= 2.0) {
        return qMax(1.5, dpiScale * 0.75);
    }
    
    return qMax(1.0, dpiScale);
}

int LoadingDialog::scaledSize(int baseSize) const
{
    return static_cast<int>(baseSize * getDpiScale());
}

int LoadingDialog::scaledFont(int baseFontSize) const
{
    double scale = getDpiScale();
    if (scale > 1.5) {
        scale = 1.0 + (scale - 1.0) * 0.8;
    }
    return static_cast<int>(baseFontSize * scale);
}

QSize LoadingDialog::scaledWindowSize(int baseWidth, int baseHeight) const
{
    return QSize(scaledSize(baseWidth), scaledSize(baseHeight));
}

QColor LoadingDialog::getStateColor(LoadingState state) const
{
    switch (state) {
        case Initializing: return QColor(59, 130, 246); // blue-500
        case CheckingNetwork: return QColor(147, 51, 234); // purple-500
        case VerifyingCEF: return QColor(249, 115, 22); // orange-500
        case LoadingCEF: return QColor(99, 102, 241); // indigo-500
        case CreatingBrowser: return QColor(6, 182, 212); // cyan-500
        case Completed: return QColor(34, 197, 94); // green-500
        case Failed: return QColor(239, 68, 68); // red-500
        default: return QColor(107, 114, 128); // gray-500
    }
}

QString LoadingDialog::getModernStyleSheet() const
{
    return QString(R"(
        QDialog {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #f8fafc, stop:0.5 #f1f5f9, stop:1 #e2e8f0);
            border: 1px solid #cbd5e1;
            border-radius: %1px;
        }
        
        #titleLabel {
            color: #1e293b;
            font-weight: bold;
        }
        
        #statusLabel {
            color: #3b82f6;
            font-weight: 500;
        }
        
        #subtitleLabel {
            color: #64748b;
        }
        
        #progressTitle, #progressLabel {
            color: #374151;
            font-weight: 500;
        }
        
        #modernProgressBar {
            border: none;
            background-color: #f1f5f9;
            border-radius: %2px;
        }
        
        #modernProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #3b82f6, stop:0.5 #8b5cf6, stop:1 #6366f1);
            border-radius: %2px;
        }
        
        #stepLabel {
            color: #9ca3af;
            padding: %3px %4px;
            border-radius: %5px;
            background-color: transparent;
        }
        
        #stepLabel[active="true"] {
            color: #3b82f6;
            background-color: #dbeafe;
            font-weight: 500;
        }
        
        #primaryButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #3b82f6, stop:1 #2563eb);
            color: white;
            border: none;
            border-radius: %6px;
            font-weight: 500;
            font-size: %7px;
        }
        
        #primaryButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #2563eb, stop:1 #1d4ed8);
        }
        
        #primaryButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #1d4ed8, stop:1 #1e40af);
        }
        
        #secondaryButton {
            background-color: white;
            color: #374151;
            border: 1px solid #d1d5db;
            border-radius: %6px;
            font-weight: 500;
            font-size: %7px;
        }
        
        #secondaryButton:hover {
            background-color: #f9fafb;
            border-color: #9ca3af;
        }
        
        #secondaryButton:pressed {
            background-color: #f3f4f6;
        }
        
        #detailsText {
            border: 1px solid #d1d5db;
            border-radius: %8px;
            background-color: #f9fafb;
            color: #374151;
            selection-background-color: #dbeafe;
        }
        
        #footerLabel {
            color: #9ca3af;
            border-top: 1px solid #e5e7eb;
            padding-top: %9px;
            margin-top: %10px;
        }
    )")
    .arg(scaledSize(12))  // dialog border-radius
    .arg(scaledSize(6))   // progress bar border-radius
    .arg(scaledSize(4))   // step label padding vertical
    .arg(scaledSize(8))   // step label padding horizontal
    .arg(scaledSize(4))   // step label border-radius
    .arg(scaledSize(8))   // button border-radius
    .arg(scaledFont(12))  // button font-size
    .arg(scaledSize(6))   // details text border-radius
    .arg(scaledSize(15))  // footer padding-top
    .arg(scaledSize(15)); // footer margin-top
}

// 继续实现其他方法...
void LoadingDialog::updateLoadingState(LoadingState state, const QString& message)
{
    LoadingState oldState = m_currentState;
    m_currentState = state;
    m_currentMessage = message;
    
    updateStateText();
    updateStateIcon();
    updateProgressSteps();
    
    if (state != oldState) {
        animateStateTransition();
    }
    
    if (state == Completed) {
        stopLoadingAnimation();
        m_progressBar->setValue(100);
        m_progressLabel->setText("100%");
        
        // 完成状态，不自动关闭，由外部控制关闭时机
    } else if (state == Failed) {
        stopLoadingAnimation();
        m_progressBar->setValue(0);
        m_progressLabel->setText("0%");
        showButtons(true, true, !m_errorDetails.isEmpty());
    } else {
        // 更新进度
        int progress = getStateProgress(state);
        m_progressBar->setValue(progress);
        m_progressLabel->setText(QString("%1%").arg(progress));
    }
}

void LoadingDialog::updateStateIcon()
{
    QPixmap iconPixmap = createStateIcon(m_currentState);
    m_iconLabel->setPixmap(iconPixmap);
    
    // 如果是加载状态，显示loading环
    if (m_currentState != Completed && m_currentState != Failed) {
        m_loadingRingLabel->setVisible(true);
        if (!m_loadingAnimationTimer->isActive()) {
            startLoadingAnimation();
        }
    } else {
        m_loadingRingLabel->setVisible(false);
    }
}

void LoadingDialog::updateProgressSteps()
{
    for (int i = 0; i < m_stepLabels.size(); ++i) {
        QLabel *dot = m_stepLabels[i];
        bool isActive = (i == static_cast<int>(m_currentState));
        bool isCompleted = (i < static_cast<int>(m_currentState));
        
        if (isActive) {
            // 当前活动步骤：蓝色
            dot->setStyleSheet("QLabel { background-color: #3b82f6; border-radius: 6px; }");
        } else if (isCompleted) {
            // 已完成步骤：绿色
            dot->setStyleSheet("QLabel { background-color: #10b981; border-radius: 6px; }");
        } else {
            // 未开始步骤：灰色
            dot->setStyleSheet("QLabel { background-color: #e5e7eb; border-radius: 6px; }");
        }
    }
}

int LoadingDialog::getStateProgress(LoadingState state) const
{
    switch (state) {
        case Initializing: return 10;
        case CheckingNetwork: return 25;
        case VerifyingCEF: return 50;
        case LoadingCEF: return 75;
        case CreatingBrowser: return 90;
        case Completed: return 100;
        case Failed: return 0;
        default: return 0;
    }
}

void LoadingDialog::animateStateTransition()
{
    // 状态图标的缩放动画
    if (m_iconAnimation) {
        m_iconAnimation->stop();
        delete m_iconAnimation;
    }
    
    m_iconAnimation = new QPropertyAnimation(m_iconLabel, "geometry");
    m_iconAnimation->setDuration(300);
    m_iconAnimation->setEasingCurve(QEasingCurve::OutBack);
    
    QRect currentGeometry = m_iconLabel->geometry();
    QRect scaledGeometry = currentGeometry;
    scaledGeometry.setWidth(currentGeometry.width() * 1.1);
    scaledGeometry.setHeight(currentGeometry.height() * 1.1);
    scaledGeometry.moveCenter(currentGeometry.center());
    
    m_iconAnimation->setStartValue(currentGeometry);
    m_iconAnimation->setKeyValueAt(0.5, scaledGeometry);
    m_iconAnimation->setEndValue(currentGeometry);
    m_iconAnimation->start();
}

QPixmap LoadingDialog::createStateIcon(LoadingState state) const
{
    int size = scaledSize(140);
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QColor color = getStateColor(state);
    QColor bgColor = color;
    bgColor.setAlpha(15); // 更淡的背景色，符合现代UI和大尺寸
    
    // 绘制背景圆
    painter.setBrush(QBrush(bgColor));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(0, 0, size, size);
    
    // 绘制状态图标 - 适应更大的尺寸
    painter.setPen(QPen(color, scaledSize(3)));
    painter.translate(size/2, size/2);
    
    switch (state) {
        case Initializing:
            drawGearIcon(&painter, scaledSize(35));
            break;
        case CheckingNetwork:
            drawWifiIcon(&painter, scaledSize(35));
            break;
        case VerifyingCEF:
            drawShieldIcon(&painter, scaledSize(35));
            break;
        case LoadingCEF:
            drawGlobeIcon(&painter, scaledSize(35));
            break;
        case CreatingBrowser:
            drawMonitorIcon(&painter, scaledSize(35));
            break;
        case Completed:
            drawCheckIcon(&painter, scaledSize(35));
            break;
        case Failed:
            drawExclamationIcon(&painter, scaledSize(35));
            break;
    }
    
    return pixmap;
}

// 图标绘制辅助函数
void LoadingDialog::drawGearIcon(QPainter *painter, int size) const
{
    // 简化的齿轮图标
    painter->drawEllipse(-size/3, -size/3, size*2/3, size*2/3);
    for (int i = 0; i < 8; ++i) {
        painter->save();
        painter->rotate(i * 45);
        painter->drawLine(0, -size/2, 0, -size/3);
        painter->restore();
    }
}

void LoadingDialog::drawWifiIcon(QPainter *painter, int size) const
{
    // WiFi信号图标
    for (int i = 1; i <= 3; ++i) {
        int radius = size * i / 6;
        painter->drawArc(-radius, -radius/2, radius*2, radius*2, 0, 180*16);
    }
    painter->drawEllipse(-size/12, size/4, size/6, size/6);
}

void LoadingDialog::drawShieldIcon(QPainter *painter, int size) const
{
    // 盾牌图标
    QPainterPath path;
    path.moveTo(0, -size/2);
    path.lineTo(-size/3, -size/4);
    path.lineTo(-size/3, size/4);
    path.lineTo(0, size/2);
    path.lineTo(size/3, size/4);
    path.lineTo(size/3, -size/4);
    path.closeSubpath();
    painter->drawPath(path);
    
    // 对勾
    painter->drawLine(-size/6, 0, -size/12, size/8);
    painter->drawLine(-size/12, size/8, size/4, -size/6);
}

void LoadingDialog::drawGlobeIcon(QPainter *painter, int size) const
{
    // 地球图标
    painter->drawEllipse(-size/2, -size/2, size, size);
    painter->drawEllipse(-size/2, -size/4, size, size/2);
    painter->drawLine(0, -size/2, 0, size/2);
    painter->drawLine(-size/2, 0, size/2, 0);
}

void LoadingDialog::drawMonitorIcon(QPainter *painter, int size) const
{
    // 显示器图标
    painter->drawRect(-size/2, -size/3, size, size*2/3);
    painter->drawLine(-size/6, size/3, size/6, size/3);
    painter->drawLine(0, size/3, 0, size/2);
    painter->drawLine(-size/4, size/2, size/4, size/2);
}

void LoadingDialog::drawCheckIcon(QPainter *painter, int size) const
{
    // 对勾图标
    QPen pen = painter->pen();
    pen.setWidth(scaledSize(4));
    pen.setCapStyle(Qt::RoundCap);
    painter->setPen(pen);
    
    painter->drawLine(-size/3, 0, -size/6, size/4);
    painter->drawLine(-size/6, size/4, size/2, -size/3);
}

void LoadingDialog::drawExclamationIcon(QPainter *painter, int size) const
{
    // 感叹号图标
    QPen pen = painter->pen();
    pen.setWidth(scaledSize(4));
    pen.setCapStyle(Qt::RoundCap);
    painter->setPen(pen);
    
    painter->drawLine(0, -size/2, 0, size/6);
    painter->drawEllipse(-size/12, size/3, size/6, size/6);
}

// 现有方法的简化实现
void LoadingDialog::showError(const QString& error, const QString& details, bool showRetry)
{
    m_currentState = Failed;
    m_currentMessage = error;
    m_errorDetails = details;
    
    // 更新为简洁的错误显示
    m_statusLabel->setText("启动失败");
    m_statusLabel->setStyleSheet("color: #ef4444; font-weight: 500;");
    m_subtitleLabel->setText(error);
    m_subtitleLabel->setVisible(true);
    
    if (!details.isEmpty()) {
        m_detailsText->setPlainText(details);
    }
    
    stopLoadingAnimation();
    updateStateIcon();
    m_progressBar->setValue(0);
    
    // 动态添加按钮到布局（错误状态下）
    m_buttonLayout->addWidget(m_retryButton);
    if (!details.isEmpty()) {
        m_buttonLayout->addWidget(m_detailsButton);
    }
    m_buttonLayout->addWidget(m_cancelButton);
    m_mainLayout->addLayout(m_buttonLayout);
    
    showButtons(showRetry, true, !details.isEmpty());
    
    // 调整窗口大小以容纳按钮 - 保持大窗口设计
    resize(scaledWindowSize(720, 680));
    
    Logger::instance().errorEvent(QString("LoadingDialog: %1").arg(error));
    if (!details.isEmpty()) {
        Logger::instance().errorEvent(QString("LoadingDialog Details: %1").arg(details));
    }
}

void LoadingDialog::showNetworkError(const QString& networkError)
{
    QString errorMsg = QString("网络连接失败：%1").arg(networkError);
    QString details = "请检查以下项目：\n"
                     "• 网络连接是否正常\n"
                     "• 防火墙设置是否阻止程序访问网络\n"
                     "• 代理设置是否正确\n"
                     "• DNS设置是否正确\n\n"
                     "您也可以选择离线模式继续使用。";
    
    showError(errorMsg, details, true);
}

void LoadingDialog::setCancellable(bool cancellable)
{
    m_cancellable = cancellable;
    if (m_currentState != Failed && m_currentState != Completed) {
        m_cancelButton->setVisible(cancellable);
    }
}

void LoadingDialog::updateStateText()
{
    QString stateMsg = m_currentMessage.isEmpty() ? getStateMessage(m_currentState) : m_currentMessage;
    m_statusLabel->setText(stateMsg);
    
    // 更新状态颜色
    QColor stateColor = getStateColor(m_currentState);
    m_statusLabel->setStyleSheet(QString("color: %1; font-weight: 500;").arg(stateColor.name()));
}

void LoadingDialog::showButtons(bool retry, bool cancel, bool details)
{
    m_retryButton->setVisible(retry);
    m_cancelButton->setVisible(cancel);
    m_detailsButton->setVisible(details);
}

QString LoadingDialog::getStateMessage(LoadingState state) const
{
    switch (state) {
        case Initializing:
            return "正在初始化应用程序...";
        case CheckingNetwork:
            return "正在检查网络连接...";
        case VerifyingCEF:
            return "正在验证CEF组件...";
        case LoadingCEF:
            return "正在加载CEF浏览器引擎...";
        case CreatingBrowser:
            return "正在创建浏览器实例...";
        case Completed:
            return "启动完成！";
        case Failed:
            return "启动失败";
        default:
            return "正在处理...";
    }
}

void LoadingDialog::closeEvent(QCloseEvent *event)
{
    if (m_currentState == Failed || m_cancellable) {
        emit cancelRequested();
        event->accept();
    } else {
        event->ignore();
    }
}

void LoadingDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        if (m_currentState == Failed || m_cancellable) {
            emit cancelRequested();
            close();
        }
    } else {
        QDialog::keyPressEvent(event);
    }
}

void LoadingDialog::paintEvent(QPaintEvent *event)
{
    QDialog::paintEvent(event);
    
    // 绘制顶部渐变条
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QLinearGradient gradient(0, 0, width(), 0);
    gradient.setColorAt(0, QColor(59, 130, 246));
    gradient.setColorAt(0.5, QColor(147, 51, 234));
    gradient.setColorAt(1, QColor(99, 102, 241));
    
    painter.fillRect(0, 0, width(), scaledSize(4), gradient);
}

void LoadingDialog::onRetryClicked()
{
    m_currentState = Initializing;
    m_errorDetails.clear();
    m_detailsVisible = false;
    
    // 重置为简洁的初始状态
    m_statusLabel->setText("正在启动");
    m_statusLabel->setStyleSheet("color: #3b82f6; font-weight: 500;");
    m_subtitleLabel->setText("正在初始化应用程序...");
    m_subtitleLabel->setVisible(true);
    
    // 移除按钮和详细信息
    m_detailsText->setVisible(false);
    showButtons(false, m_cancellable, false);
    
    // 从布局中移除按钮
    m_mainLayout->removeItem(m_buttonLayout);
    
    // 恢复原始大窗口尺寸
    resize(scaledWindowSize(720, 600));
    
    updateStateIcon();
    startLoadingAnimation();
    updateStateText();
    
    emit retryRequested();
}

void LoadingDialog::onCancelClicked()
{
    emit cancelRequested();
    reject();
}

void LoadingDialog::onDetailsToggled()
{
    m_detailsVisible = !m_detailsVisible;
    
    if (m_detailsVisible) {
        m_detailsButton->setText("隐藏详细信息");
        // 添加详细信息到布局
        if (m_detailsText->parent() == nullptr) {
            m_mainLayout->insertWidget(m_mainLayout->count() - 1, m_detailsText);
        }
        m_detailsText->setVisible(true);
        // 增加高度以容纳详细信息，保持大窗口设计
        resize(scaledWindowSize(720, 800));
    } else {
        m_detailsButton->setText("详细信息");
        m_detailsText->setVisible(false);
        // 恢复错误状态的窗口尺寸
        resize(scaledWindowSize(720, 680));
    }
    
    emit detailsRequested();
}