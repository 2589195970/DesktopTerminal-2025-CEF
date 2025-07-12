#include "loading_dialog.h"
#include "../logging/logger.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QPropertyAnimation>
#include <QApplication>
#include <QScreen>
#include <QStyle>
#include <QtMath>

LoadingDialog::LoadingDialog(QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_titleLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_retryButton(nullptr)
    , m_cancelButton(nullptr)
    , m_animationTimer(nullptr)
    , m_rotationAnimation(nullptr)
    , m_rotation(0.0)
    , m_isError(false)
    , m_progressValue(0)
    , m_progressMax(100)
{
    setupUI();
    applyStyles();
    createRotationAnimation();
    
    // 设置窗口属性
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    
    // 居中显示
    if (QScreen *screen = QApplication::primaryScreen()) {
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - DIALOG_WIDTH) / 2;
        int y = (screenGeometry.height() - DIALOG_HEIGHT) / 2;
        setGeometry(x, y, DIALOG_WIDTH, DIALOG_HEIGHT);
    }
    
    Logger::instance().appEvent("LoadingDialog创建完成");
}

LoadingDialog::~LoadingDialog()
{
    stopAnimation();
    Logger::instance().appEvent("LoadingDialog销毁");
}

void LoadingDialog::setupUI()
{
    // 主布局
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);
    
    // 留出空间给自定义绘制的加载图标
    mainLayout->addSpacing(ICON_SIZE + 20);
    
    // 标题标签
    m_titleLabel = new QLabel("正在启动应用程序", this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setObjectName("loadingTitle");
    mainLayout->addWidget(m_titleLabel);
    
    // 状态标签
    m_statusLabel = new QLabel("正在初始化...", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setObjectName("loadingStatus");
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);
    
    mainLayout->addStretch();
    
    // 按钮布局
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    
    // 重试按钮（初始隐藏）
    m_retryButton = new QPushButton("重试", this);
    m_retryButton->setObjectName("retryButton");
    m_retryButton->setVisible(false);
    m_retryButton->setMinimumWidth(80);
    connect(m_retryButton, &QPushButton::clicked, this, &LoadingDialog::retryClicked);
    buttonLayout->addWidget(m_retryButton);
    
    // 取消按钮（初始隐藏）
    m_cancelButton = new QPushButton("取消", this);
    m_cancelButton->setObjectName("cancelButton");
    m_cancelButton->setVisible(false);
    m_cancelButton->setMinimumWidth(80);
    connect(m_cancelButton, &QPushButton::clicked, this, &LoadingDialog::cancelClicked);
    buttonLayout->addWidget(m_cancelButton);
    
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
}

void LoadingDialog::applyStyles()
{
    // 应用样式表
    QString styleSheet = R"(
        LoadingDialog {
            background-color: rgba(240, 240, 240, 250);
            border: 2px solid #cccccc;
            border-radius: 8px;
        }
        
        #loadingTitle {
            color: #333333;
            font-size: 24px;
            font-weight: bold;
        }
        
        #loadingStatus {
            color: #666666;
            font-size: 18px;
        }
        
        #loadingStatus[error="true"] {
            color: #f44336;
            font-weight: bold;
        }
        
        QPushButton {
            background-color: #ffffff;
            border: 1px solid #cccccc;
            border-radius: 4px;
            padding: 6px 12px;
            font-size: 12px;
        }
        
        QPushButton:hover {
            background-color: #f5f5f5;
            border-color: #999999;
        }
        
        QPushButton:pressed {
            background-color: #e0e0e0;
        }
    )";
    
    setStyleSheet(styleSheet);
}

void LoadingDialog::createRotationAnimation()
{
    // 创建旋转动画
    m_rotationAnimation = new QPropertyAnimation(this, "rotation");
    m_rotationAnimation->setDuration(ANIMATION_DURATION);
    m_rotationAnimation->setStartValue(0.0);
    m_rotationAnimation->setEndValue(360.0);
    m_rotationAnimation->setLoopCount(-1); // 无限循环
    m_rotationAnimation->setEasingCurve(QEasingCurve::Linear);
    
    // 创建定时器触发重绘
    m_animationTimer = new QTimer(this);
    m_animationTimer->setInterval(16); // 约60 FPS
    connect(m_animationTimer, &QTimer::timeout, this, QOverload<>::of(&LoadingDialog::update));
}

void LoadingDialog::setStatus(const QString& status)
{
    m_isError = false;
    m_statusLabel->setText(status);
    m_statusLabel->setProperty("error", false);
    m_statusLabel->style()->polish(m_statusLabel);
    
    // 正常状态下隐藏按钮
    m_retryButton->setVisible(false);
    m_cancelButton->setVisible(false);
    
    Logger::instance().appEvent(QString("LoadingDialog状态: %1").arg(status));
}

void LoadingDialog::setError(const QString& error)
{
    m_isError = true;
    m_statusLabel->setText(error);
    m_statusLabel->setProperty("error", true);
    m_statusLabel->style()->polish(m_statusLabel);
    
    // 错误状态下显示按钮
    m_retryButton->setVisible(true);
    m_cancelButton->setVisible(true);
    
    // 停止动画
    stopAnimation();
    
    Logger::instance().errorEvent(QString("LoadingDialog错误: %1").arg(error));
}

void LoadingDialog::setProgress(int value, int maximum)
{
    m_progressValue = value;
    m_progressMax = maximum;
    update(); // 触发重绘
}

void LoadingDialog::startAnimation()
{
    if (m_rotationAnimation && m_animationTimer) {
        m_rotationAnimation->start();
        m_animationTimer->start();
        Logger::instance().appEvent("LoadingDialog动画启动");
    }
}

void LoadingDialog::stopAnimation()
{
    if (m_rotationAnimation && m_animationTimer) {
        m_rotationAnimation->stop();
        m_animationTimer->stop();
        m_rotation = 0.0;
        update();
        Logger::instance().appEvent("LoadingDialog动画停止");
    }
}

void LoadingDialog::setRotation(qreal rotation)
{
    m_rotation = rotation;
    update(); // 触发重绘
}

void LoadingDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制背景
    painter.setBrush(QColor(240, 240, 240, 250));
    painter.setPen(QPen(QColor(204, 204, 204), 2));
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);
    
    // 如果不是错误状态，绘制加载图标
    if (!m_isError) {
        drawLoadingIcon(painter);
    }
    
    // 绘制进度条（如果有进度值）
    if (m_progressMax > 0 && m_progressValue >= 0) {
        int barWidth = width() - 40;
        int barHeight = 4;
        int barX = 20;
        int barY = height() - 50;
        
        // 背景
        painter.setBrush(QColor(220, 220, 220));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(barX, barY, barWidth, barHeight, 2, 2);
        
        // 进度
        if (m_progressValue > 0) {
            int progressWidth = (barWidth * m_progressValue) / m_progressMax;
            painter.setBrush(QColor(76, 175, 80));
            painter.drawRoundedRect(barX, barY, progressWidth, barHeight, 2, 2);
        }
    }
}

void LoadingDialog::drawLoadingIcon(QPainter& painter)
{
    // 保存画家状态
    painter.save();
    
    // 移动到图标中心位置
    int centerX = width() / 2;
    int centerY = 40;
    painter.translate(centerX, centerY);
    
    // 应用旋转
    painter.rotate(m_rotation);
    
    // 绘制旋转的圆形加载图标
    int radius = ICON_SIZE / 2;
    int arcLength = 270; // 弧长（度）
    
    // 绘制圆弧
    QPen pen(QColor(76, 175, 80), 4);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    
    QRectF rect(-radius, -radius, ICON_SIZE, ICON_SIZE);
    painter.drawArc(rect, 0, arcLength * 16); // Qt使用1/16度为单位
    
    // 恢复画家状态
    painter.restore();
}

void LoadingDialog::keyPressEvent(QKeyEvent *event)
{
    // 拦截ESC键，防止意外关闭
    if (event->key() == Qt::Key_Escape) {
        if (m_isError) {
            // 错误状态下ESC等同于取消
            emit cancelClicked();
        }
        event->accept();
        return;
    }
    
    QDialog::keyPressEvent(event);
}