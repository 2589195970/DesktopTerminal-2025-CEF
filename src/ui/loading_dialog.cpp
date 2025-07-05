#include "loading_dialog.h"
#include "../logging/logger.h"

#include <QApplication>
#include <QScreen>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QShowEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QFont>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QStyle>
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
    
    // 设置窗口属性 - 1200x800基础尺寸，支持DPI感知
    setWindowTitle("智多分机考桌面端 - 正在启动");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    
    // 参照现代UI设计，设置简洁大方的尺寸 - 修复大小设置无效问题
    m_targetSize = scaledWindowSize(1024, 683);
    QSize minSize = scaledWindowSize(960, 640);
    
    // 先重置所有大小约束 - 这是修复setFixedSize无效的关键步骤
    setMinimumSize(0, 0);
    setMaximumSize(QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
    
    // 在构造阶段不设置大小，等到showEvent中设置
    // 这样可以避免与parent窗口状态和布局系统的冲突
    
    // 居中显示
    if (QWidget *parentWin = parentWidget()) {
        QRect parentGeometry = parentWin->geometry();
        move(parentGeometry.center() - rect().center());
    } else {
        if (QScreen *screen = QApplication::primaryScreen()) {
            QRect screenGeometry = screen->geometry();
            move(screenGeometry.center() - rect().center());
        }
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
    m_mainLayout->setSpacing(0);
    m_mainLayout->setContentsMargins(scaledSize(100), scaledSize(100), scaledSize(100), scaledSize(100));
    
    // 顶部弹性空间
    m_mainLayout->addStretch(2);
    
    // WiFi图标区域 - 大窗口适配的图标尺寸
    m_iconLabel = new QLabel();
    m_iconLabel->setAlignment(Qt::AlignCenter);
    int iconSize = 300;  // 固定300px适应1600x1000窗口
    m_iconLabel->setFixedSize(iconSize, iconSize);
    m_iconLabel->setObjectName("iconLabel");
    m_mainLayout->addWidget(m_iconLabel);
    m_mainLayout->addSpacing(scaledSize(50));
    
    // 主标题 - 智多分机考桌面端
    m_titleLabel = new QLabel("智多分机考桌面端");
    m_titleLabel->setObjectName("mainTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(48);  // 固定48px适应大窗口
    titleFont.setWeight(QFont::Bold);
    m_titleLabel->setFont(titleFont);
    m_mainLayout->addWidget(m_titleLabel);
    m_mainLayout->addSpacing(scaledSize(20));
    
    // 状态标签 - 正在检查网络连接...
    m_statusLabel = new QLabel("正在检查网络连接...");
    m_statusLabel->setObjectName("statusLabel");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    QFont statusFont = m_statusLabel->font();
    statusFont.setPointSize(28);  // 固定28px适应大窗口
    statusFont.setWeight(QFont::Medium);
    m_statusLabel->setFont(statusFont);
    m_mainLayout->addWidget(m_statusLabel);
    m_mainLayout->addSpacing(scaledSize(15));
    
    // 副标题 - 请稍候，正在为您准备最佳的考试环境...
    m_subtitleLabel = new QLabel("请稍候，正在为您准备最佳的考试环境...");
    m_subtitleLabel->setObjectName("subtitleLabel");
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    QFont subtitleFont = m_subtitleLabel->font();
    subtitleFont.setPointSize(scaledFont(18));
    m_subtitleLabel->setFont(subtitleFont);
    m_mainLayout->addWidget(m_subtitleLabel);
    
    // 中间弹性空间
    m_mainLayout->addStretch(1);
    
    // 进度信息区域
    QHBoxLayout *progressInfoLayout = new QHBoxLayout();
    progressInfoLayout->setContentsMargins(scaledSize(200), 0, scaledSize(200), 0);
    
    QLabel *progressTitle = new QLabel("启动进度");
    progressTitle->setObjectName("progressTitle");
    QFont progressTitleFont = progressTitle->font();
    progressTitleFont.setPointSize(scaledFont(16));
    progressTitle->setFont(progressTitleFont);
    
    m_progressLabel = new QLabel("25%");
    m_progressLabel->setObjectName("progressPercent");
    QFont progressPercentFont = m_progressLabel->font();
    progressPercentFont.setPointSize(scaledFont(16));
    progressPercentFont.setWeight(QFont::Bold);
    m_progressLabel->setFont(progressPercentFont);
    
    progressInfoLayout->addWidget(progressTitle);
    progressInfoLayout->addStretch();
    progressInfoLayout->addWidget(m_progressLabel);
    
    m_mainLayout->addLayout(progressInfoLayout);
    m_mainLayout->addSpacing(scaledSize(15));
    
    // 进度条
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(25);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(scaledSize(8));
    m_progressBar->setObjectName("modernProgressBar");
    m_progressBar->setContentsMargins(scaledSize(200), 0, scaledSize(200), 0);
    m_mainLayout->addWidget(m_progressBar);
    m_mainLayout->addSpacing(scaledSize(30));
    
    // 步骤指示器
    m_stepsLayout = new QHBoxLayout();
    m_stepsLayout->setSpacing(scaledSize(40));
    m_stepsLayout->setAlignment(Qt::AlignCenter);
    setupProgressSteps();
    m_mainLayout->addLayout(m_stepsLayout);
    
    // 底部弹性空间
    m_mainLayout->addStretch(1);
    
    // 底部区域
    QVBoxLayout *bottomLayout = new QVBoxLayout();
    bottomLayout->setSpacing(scaledSize(20));
    bottomLayout->setAlignment(Qt::AlignCenter);
    
    // 模拟错误按钮（调试用）
    QPushButton *simulateErrorButton = new QPushButton("模拟错误");
    simulateErrorButton->setObjectName("debugButton");
    simulateErrorButton->setFixedSize(scaledSize(120), scaledSize(36));
    QFont debugFont = simulateErrorButton->font();
    debugFont.setPointSize(scaledFont(14));
    simulateErrorButton->setFont(debugFont);
    simulateErrorButton->setVisible(false); // 默认隐藏
    bottomLayout->addWidget(simulateErrorButton);
    
    // 版本信息
    QLabel *versionLabel = new QLabel("智多分机考系统 v2.0 | 为您提供安全可靠的考试环境");
    versionLabel->setObjectName("versionLabel");
    versionLabel->setAlignment(Qt::AlignCenter);
    QFont versionFont = versionLabel->font();
    versionFont.setPointSize(scaledFont(14));
    versionLabel->setFont(versionFont);
    bottomLayout->addWidget(versionLabel);
    
    m_mainLayout->addLayout(bottomLayout);
    m_mainLayout->addSpacing(scaledSize(50));
    
    // 其他隐藏组件
    m_loadingRingLabel = new QLabel();
    m_loadingRingLabel->setVisible(false);
    
    m_detailsText = new QTextEdit();
    m_detailsText->setVisible(false);
    m_detailsText->setReadOnly(true);
    m_detailsText->setMaximumHeight(scaledSize(200));
    m_detailsText->setObjectName("detailsText");
    
    // 错误状态按钮
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
}

void LoadingDialog::setupProgressSteps()
{
    // 6个步骤的指示器
    QStringList stepNames = {"初始化", "网络检测", "组件验证", "引擎加载", "创建实例", "完成"};
    
    for (int i = 0; i < stepNames.size(); ++i) {
        QLabel *stepLabel = new QLabel(stepNames[i]);
        stepLabel->setObjectName("stepLabel");
        stepLabel->setAlignment(Qt::AlignCenter);
        stepLabel->setProperty("stepIndex", i);
        stepLabel->setProperty("active", i == 1); // 默认"网络检测"为当前步骤
        
        QFont stepFont = stepLabel->font();
        stepFont.setPointSize(scaledFont(14));
        stepLabel->setFont(stepFont);
        
        // 设置固定宽度以确保对齐
        stepLabel->setMinimumWidth(scaledSize(80));
        
        m_stepLabels.append(stepLabel);
        m_stepsLayout->addWidget(stepLabel);
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
    
    // 现代化的渐变背景 - 仿照React设计 (slate-50 -> blue-50 -> indigo-100)
    QString backgroundStyle = QString(R"(
        QDialog {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #f8fafc, stop:0.5 #eff6ff, stop:1 #e0e7ff);
            border: none;
        }
    )");
    
    // 合并背景样式和其他组件样式
    QString fullStyle = backgroundStyle + getModernStyleSheet().replace(
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

QSize LoadingDialog::calculateOptimalWindowSize() const
{
    // 基础尺寸：1200x800
    int baseWidth = 1200;
    int baseHeight = 800;
    
    QScreen* screen = QApplication::primaryScreen();
    if (!screen) {
        return QSize(baseWidth, baseHeight);
    }
    
    QSize screenSize = screen->availableSize();
    double dpiScale = getDpiScale();
    
    // 根据DPI计算初始尺寸
    int scaledWidth = static_cast<int>(baseWidth * dpiScale);
    int scaledHeight = static_cast<int>(baseHeight * dpiScale);
    
    // 确保窗口不超过屏幕可用区域的80%
    int maxWidth = static_cast<int>(screenSize.width() * 0.8);
    int maxHeight = static_cast<int>(screenSize.height() * 0.8);
    
    scaledWidth = qMin(scaledWidth, maxWidth);
    scaledHeight = qMin(scaledHeight, maxHeight);
    
    // 保持宽高比例
    double aspectRatio = static_cast<double>(baseWidth) / baseHeight;
    if (scaledWidth / aspectRatio > scaledHeight) {
        scaledWidth = static_cast<int>(scaledHeight * aspectRatio);
    } else {
        scaledHeight = static_cast<int>(scaledWidth / aspectRatio);
    }
    
    // 最小尺寸限制：确保窗口不会太小
    scaledWidth = qMax(scaledWidth, 900);
    scaledHeight = qMax(scaledHeight, 600);
    
    return QSize(scaledWidth, scaledHeight);
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
                stop:0 #f8fafc, stop:0.5 #eff6ff, stop:1 #e0e7ff);
            border: none;
        }
        
        #mainTitle {
            color: #1a202c;
            font-weight: bold;
        }
        
        #statusLabel {
            color: #8b5cf6;
            font-weight: 500;
        }
        
        #subtitleLabel {
            color: #6b7280;
        }
        
        #progressTitle {
            color: #374151;
            font-weight: 500;
        }
        
        #progressPercent {
            color: #1f2937;
            font-weight: bold;
        }
        
        #modernProgressBar {
            border: none;
            background-color: #f3f4f6;
            border-radius: 6px;
            margin-left: 150px;
            margin-right: 150px;
        }
        
        #modernProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #3b82f6, stop:0.5 #8b5cf6, stop:1 #6366f1);
            border-radius: 6px;
        }
        
        #stepLabel {
            color: #9ca3af;
            padding: 4px 8px;
            border-radius: 4px;
            background-color: transparent;
        }
        
        #stepLabel[active="true"] {
            color: #8b5cf6;
            font-weight: 600;
        }
        
        #debugButton {
            background-color: transparent;
            color: #6b7280;
            border: 1px solid #d1d5db;
            border-radius: 6px;
            font-weight: 400;
        }
        
        #debugButton:hover {
            background-color: #f9fafb;
            border-color: #9ca3af;
        }
        
        #versionLabel {
            color: #9ca3af;
        }
        
        #primaryButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #3b82f6, stop:1 #1d4ed8);
            color: white;
            border: none;
            border-radius: 6px;
            font-weight: 500;
        }
        
        #primaryButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #2563eb, stop:1 #1e40af);
        }
        
        #secondaryButton {
            background-color: white;
            color: #374151;
            border: 1px solid #d1d5db;
            border-radius: 6px;
            font-weight: 500;
        }
        
        #secondaryButton:hover {
            background-color: #f9fafb;
            border-color: #9ca3af;
        }
        
        #detailsText {
            border: 1px solid #d1d5db;
            border-radius: 8px;
            background-color: #f9fafb;
            color: #374151;
            selection-background-color: #dbeafe;
        }
    )");
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
        QLabel *stepLabel = m_stepLabels[i];
        bool isActive = (i == static_cast<int>(m_currentState));
        
        stepLabel->setProperty("active", isActive);
        
        // 刷新样式表以应用新的active状态
        stepLabel->style()->unpolish(stepLabel);
        stepLabel->style()->polish(stepLabel);
        stepLabel->update();
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
    int size = 300;  // 固定300px适应大窗口
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 创建渐变背景
    QRadialGradient gradient(size/2, size/2, size/2);
    gradient.setColorAt(0, QColor(240, 240, 255, 50));  // 中心淡紫色
    gradient.setColorAt(0.7, QColor(220, 220, 240, 30)); // 边缘更淡
    gradient.setColorAt(1, QColor(200, 200, 230, 10));   // 外边缘几乎透明
    
    // 绘制背景圆
    painter.setBrush(QBrush(gradient));
    painter.setPen(QPen(QColor(200, 200, 230, 80), 1));
    painter.drawEllipse(10, 10, size - 20, size - 20);
    
    // 绘制WiFi图标 - 统一使用WiFi符号，体现网络检测主题
    QColor iconColor = getStateColor(state);
    painter.setPen(QPen(iconColor, 4));
    painter.translate(size/2, size/2);
    
    // 绘制大尺寸WiFi图标，适应300px画布
    drawModernWifiIcon(&painter, 80);
    
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

void LoadingDialog::drawModernWifiIcon(QPainter *painter, int size) const
{
    QPen pen = painter->pen();
    pen.setCapStyle(Qt::RoundCap);
    painter->setPen(pen);
    
    // 绘制4层WiFi弧形，从内到外
    for (int i = 1; i <= 4; ++i) {
        int radius = size * i / 8;
        int arcWidth = radius * 2;
        int arcHeight = radius * 2;
        
        // 弧形的起始角度和跨度角度（在Qt中，角度以1/16度为单位）
        int startAngle = 45 * 16;  // 45度
        int spanAngle = 90 * 16;   // 90度弧形
        
        painter->drawArc(-radius, -radius, arcWidth, arcHeight, startAngle, spanAngle);
    }
    
    // 绘制中心圆点
    int dotSize = size / 8;
    painter->setBrush(painter->pen().color());
    painter->drawEllipse(-dotSize/2, size/3, dotSize, dotSize);
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
    
    // 全屏模式无需调整窗口大小
    // resize(scaledWindowSize(720, 680)); // 已移除，保持全屏
    
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
    
    // 全屏模式无需调整窗口大小
    // resize(scaledWindowSize(720, 600)); // 已移除，保持全屏
    
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
        // 全屏模式无需调整窗口大小
    } else {
        m_detailsButton->setText("详细信息");
        m_detailsText->setVisible(false);
        // 全屏模式无需调整窗口大小
    }
    
    emit detailsRequested();
}

void LoadingDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    
    // 在显示时设置窗口大小 - 这是修复setFixedSize无效的关键方法
    if (!m_targetSize.isEmpty()) {
        // 先调用showNormal()确保窗口处于正常状态
        showNormal();
        
        // 设置固定大小
        setFixedSize(m_targetSize);
        
        // 强制刷新窗口属性
        adjustSize();
        
        // 重新居中显示
        if (QWidget *parentWin = parentWidget()) {
            QRect parentGeometry = parentWin->geometry();
            move(parentGeometry.center() - rect().center());
        } else {
            if (QScreen *screen = QApplication::primaryScreen()) {
                QRect screenGeometry = screen->geometry();
                move(screenGeometry.center() - rect().center());
            }
        }
    }
}