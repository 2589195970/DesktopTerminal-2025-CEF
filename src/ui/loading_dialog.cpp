#include "loading_dialog.h"
#include "../logging/logger.h"
#include "../core/system_checker.h"

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
#include <QProgressBar>
#include <QTextEdit>
#include <QScrollArea>
#include <QSplitter>
#include <QGroupBox>
#include <QFrame>

LoadingDialog::LoadingDialog(QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_titleLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_retryButton(nullptr)
    , m_cancelButton(nullptr)
    , m_progressBar(nullptr)
    , m_progressLabel(nullptr)
    , m_errorDetailsText(nullptr)
    , m_detailsButton(nullptr)
    , m_autoFixButton(nullptr)
    , m_systemChecker(nullptr)
    , m_animationTimer(nullptr)
    , m_rotationAnimation(nullptr)
    , m_rotation(0.0)
    , m_isError(false)
    , m_systemCheckInProgress(false)
    , m_showingDetails(false)
    , m_progressValue(0)
    , m_progressMax(100)
{
    setupUI();
    applyStyles();
    createRotationAnimation();
    
    // 初始化SystemChecker
    initializeSystemChecker();
    
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
    
    // 进度条
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(100);
    m_progressBar->setObjectName("systemProgressBar");
    mainLayout->addWidget(m_progressBar);
    
    // 进度标签
    m_progressLabel = new QLabel("", this);
    m_progressLabel->setAlignment(Qt::AlignCenter);
    m_progressLabel->setObjectName("progressLabel");
    m_progressLabel->setVisible(false);
    mainLayout->addWidget(m_progressLabel);
    
    // 错误详情显示区域
    m_errorDetailsText = new QTextEdit(this);
    m_errorDetailsText->setObjectName("errorDetails");
    m_errorDetailsText->setReadOnly(true);
    m_errorDetailsText->setVisible(false);
    m_errorDetailsText->setMaximumHeight(150);
    mainLayout->addWidget(m_errorDetailsText);
    
    mainLayout->addStretch();
    
    // 按钮布局
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    
    // 重试按钮（初始隐藏）
    m_retryButton = new QPushButton("重试", this);
    m_retryButton->setObjectName("retryButton");
    m_retryButton->setVisible(false);
    m_retryButton->setMinimumWidth(100);
    m_retryButton->setMinimumHeight(40);
    connect(m_retryButton, &QPushButton::clicked, this, &LoadingDialog::retryClicked);
    buttonLayout->addWidget(m_retryButton);
    
    // 取消按钮（初始隐藏）
    m_cancelButton = new QPushButton("取消", this);
    m_cancelButton->setObjectName("cancelButton");
    m_cancelButton->setVisible(false);
    m_cancelButton->setMinimumWidth(100);
    m_cancelButton->setMinimumHeight(40);
    m_cancelButton->setFont(buttonFont);
    connect(m_cancelButton, &QPushButton::clicked, this, &LoadingDialog::cancelClicked);
    buttonLayout->addWidget(m_cancelButton);
    
    // 详情按钮（初始隐藏）
    m_detailsButton = new QPushButton("显示详情", this);
    m_detailsButton->setObjectName("detailsButton");
    m_detailsButton->setVisible(false);
    m_detailsButton->setMinimumWidth(100);
    m_detailsButton->setMinimumHeight(40);
    m_detailsButton->setFont(buttonFont);
    connect(m_detailsButton, &QPushButton::clicked, this, &LoadingDialog::onShowErrorDetails);
    buttonLayout->addWidget(m_detailsButton);
    
    // 自动修复按钮（初始隐藏）
    m_autoFixButton = new QPushButton("自动修复", this);
    m_autoFixButton->setObjectName("autoFixButton");
    m_autoFixButton->setVisible(false);
    m_autoFixButton->setMinimumWidth(100);
    m_autoFixButton->setMinimumHeight(40);
    m_autoFixButton->setFont(buttonFont);
    buttonLayout->addWidget(m_autoFixButton);
    
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
}

void LoadingDialog::applyStyles()
{
    // 从文件加载样式表
    QFile file(":/resources/loading_animation.css");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(file.readAll());
        file.close();
    } else {
        Logger::instance().errorEvent("无法加载loading_animation.css");
    }
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
    // 背景绘制现在由样式表处理
    QDialog::paintEvent(event);
    
    // 如果系统检测正在进行，绘制加载动画
    if (m_systemCheckInProgress) {
        QPainter painter(this);
        drawLoadingIcon(painter);
    }
}

void LoadingDialog::drawLoadingIcon(QPainter& painter)
{
    // 设置渲染提示
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 计算图标绘制位置 (在状态文本上方)
    int iconSize = 32;
    int x = (width() - iconSize) / 2;
    int y = height() / 2 - 60; // 在中心上方一点
    
    // 保存当前变换状态
    painter.save();
    
    // 设置旋转中心并旋转
    painter.translate(x + iconSize / 2, y + iconSize / 2);
    painter.rotate(m_rotation);
    
    // 绘制简单的旋转加载图标
    painter.setPen(QPen(QColor(64, 128, 255), 3)); // 蓝色笔触
    painter.setBrush(Qt::NoBrush);
    
    // 绘制圆弧表示加载
    QRect iconRect(-iconSize / 2, -iconSize / 2, iconSize, iconSize);
    painter.drawArc(iconRect, 0, 270 * 16); // 绘制3/4圆弧
    
    // 恢复变换状态
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

// SystemChecker集成实现

void LoadingDialog::initializeSystemChecker()
{
    // 创建SystemChecker实例
    m_systemChecker = new SystemChecker(this);
    
    // 连接SystemChecker信号到LoadingDialog槽函数
    connect(m_systemChecker, &SystemChecker::checkProgress,
            this, &LoadingDialog::onCheckProgress);
    connect(m_systemChecker, &SystemChecker::checkItemCompleted,
            this, &LoadingDialog::onCheckItemCompleted);
    connect(m_systemChecker, &SystemChecker::checkCompleted,
            this, &LoadingDialog::onCheckCompleted);
    connect(m_systemChecker, &SystemChecker::autoFixCompleted,
            this, &LoadingDialog::onAutoFixCompleted);
    
    // 连接自动修复按钮
    connect(m_autoFixButton, &QPushButton::clicked, [this]() {
        if (m_systemChecker) {
            m_systemChecker->attemptAutoFix();
        }
    });
    
    // 连接重试按钮到系统检测
    connect(m_retryButton, &QPushButton::clicked, this, &LoadingDialog::onRetrySystemCheck);
    
    Logger::instance().appEvent("LoadingDialog SystemChecker初始化完成");
}

void LoadingDialog::onCheckProgress(int current, int total, const QString& message)
{
    if (!m_progressBar || !m_progressLabel || !m_statusLabel) {
        return;
    }
    
    // 显示进度条和进度标签
    m_progressBar->setVisible(true);
    m_progressLabel->setVisible(true);
    
    // 更新进度
    int progressPercent = (current * 100) / total;
    m_progressBar->setValue(progressPercent);
    
    // 更新状态标签
    m_statusLabel->setText(message);
    
    // 更新进度标签
    m_progressLabel->setText(QString("正在执行: %1/%2").arg(current).arg(total));
    
    Logger::instance().appEvent(QString("检测进度: %1/%2 - %3").arg(current).arg(total).arg(message));
}

void LoadingDialog::onCheckItemCompleted(const SystemChecker::CheckResult& result)
{
    // 记录检测结果
    m_checkResults.append(result);
    
    // 根据检测结果级别更新界面
    QString levelText;
    switch (result.level) {
        case SystemChecker::LEVEL_OK:
            levelText = "✓ 正常";
            break;
        case SystemChecker::LEVEL_WARNING:
            levelText = "⚠ 警告";
            break;
        case SystemChecker::LEVEL_ERROR:
            levelText = "✗ 错误";
            break;
        case SystemChecker::LEVEL_FATAL:
            levelText = "✗ 致命错误";
            break;
    }
    
    QString statusText = QString("%1: %2 - %3").arg(result.title).arg(levelText).arg(result.message);
    
    if (m_statusLabel) {
        m_statusLabel->setText(statusText);
    }
    
    // 如果是错误或警告，准备显示详情
    if (result.level >= SystemChecker::LEVEL_WARNING) {
        updateErrorDisplay();
    }
    
    Logger::instance().appEvent(QString("检测项目完成: %1 - 级别: %2").arg(result.title).arg(levelText));
}

void LoadingDialog::onCheckCompleted(bool success, const QList<SystemChecker::CheckResult>& results)
{
    m_systemCheckInProgress = false;
    m_checkResults = results;
    
    // 停止动画
    stopAnimation();
    
    if (success) {
        // 所有检测通过，准备启动应用程序
        if (m_statusLabel) {
            m_statusLabel->setText("系统检测完成，正在启动应用程序...");
        }
        
        if (m_progressBar) {
            m_progressBar->setValue(100);
        }
        
        // 隐藏错误相关按钮
        if (m_retryButton) m_retryButton->setVisible(false);
        if (m_cancelButton) m_cancelButton->setVisible(false);
        if (m_detailsButton) m_detailsButton->setVisible(false);
        if (m_autoFixButton) m_autoFixButton->setVisible(false);
        
        Logger::instance().appEvent("系统检测成功完成，准备启动应用程序");
        
        // 发射信号通知系统检测完成
        emit systemCheckCompleted(true);
        emit readyToStartApplication();
        
    } else {
        // 检测失败，显示错误信息
        m_isError = true;
        
        // 计算错误统计
        int fatalCount = 0, errorCount = 0, warningCount = 0;
        for (const auto& result : results) {
            switch (result.level) {
                case SystemChecker::LEVEL_FATAL: fatalCount++; break;
                case SystemChecker::LEVEL_ERROR: errorCount++; break;
                case SystemChecker::LEVEL_WARNING: warningCount++; break;
                default: break;
            }
        }
        
        QString errorSummary = QString("系统检测失败: 致命错误%1个，错误%2个，警告%3个")
                              .arg(fatalCount).arg(errorCount).arg(warningCount);
        
        if (m_statusLabel) {
            m_statusLabel->setText(errorSummary);
            m_statusLabel->setProperty("error", true);
            m_statusLabel->style()->polish(m_statusLabel);
        }
        
        // 显示操作按钮
        if (m_retryButton) m_retryButton->setVisible(true);
        if (m_cancelButton) m_cancelButton->setVisible(true);
        if (m_detailsButton) m_detailsButton->setVisible(true);
        
        // 检查是否有可自动修复的问题
        bool hasAutoFixable = false;
        for (const auto& result : results) {
            if (result.autoFixable && result.level != SystemChecker::LEVEL_OK) {
                hasAutoFixable = true;
                break;
            }
        }
        
        if (hasAutoFixable && m_autoFixButton) {
            m_autoFixButton->setVisible(true);
        }
        
        // 更新错误详情显示
        updateErrorDisplay();
        
        Logger::instance().errorEvent(errorSummary);
        
        // 发射信号通知系统检测失败
        emit systemCheckCompleted(false);
    }
}

void LoadingDialog::onAutoFixCompleted(int fixed)
{
    QString message = QString("自动修复完成，已修复%1个问题").arg(fixed);
    
    if (m_statusLabel) {
        m_statusLabel->setText(message);
    }
    
    Logger::instance().appEvent(message);
    
    // 如果有问题被修复，可以重新开始检测
    if (fixed > 0) {
        // 短暂延迟后重新检测
        QTimer::singleShot(1000, this, &LoadingDialog::onRetrySystemCheck);
    }
}

void LoadingDialog::onRetrySystemCheck()
{
    if (m_systemCheckInProgress) {
        return;
    }
    
    // 重置界面状态
    m_isError = false;
    m_checkResults.clear();
    
    // 隐藏错误相关UI
    if (m_errorDetailsText) {
        m_errorDetailsText->setVisible(false);
    }
    
    if (m_retryButton) m_retryButton->setVisible(false);
    if (m_cancelButton) m_cancelButton->setVisible(false);
    if (m_detailsButton) m_detailsButton->setVisible(false);
    if (m_autoFixButton) m_autoFixButton->setVisible(false);
    
    // 重置状态标签样式
    if (m_statusLabel) {
        m_statusLabel->setProperty("error", false);
        m_statusLabel->style()->polish(m_statusLabel);
        m_statusLabel->setText("重新开始系统检测...");
    }
    
    // 重置进度条
    if (m_progressBar) {
        m_progressBar->setValue(0);
        m_progressBar->setVisible(true);
    }
    
    if (m_progressLabel) {
        m_progressLabel->setText("");
        m_progressLabel->setVisible(true);
    }
    
    // 重新启动动画
    startAnimation();
    
    // 开始系统检测
    startSystemCheck();
    
    Logger::instance().appEvent("用户触发重试系统检测");
}

void LoadingDialog::onShowErrorDetails()
{
    if (!m_errorDetailsText || !m_detailsButton) {
        return;
    }
    
    m_showingDetails = !m_showingDetails;
    
    if (m_showingDetails) {
        // 显示详细错误信息
        m_errorDetailsText->setVisible(true);
        m_detailsButton->setText("隐藏详情");
        
        // 填充错误详情内容
        displayCheckResults();
        
        Logger::instance().appEvent("用户查看详细错误信息");
    } else {
        // 隐藏详细错误信息
        m_errorDetailsText->setVisible(false);
        m_detailsButton->setText("显示详情");
        
        Logger::instance().appEvent("用户隐藏详细错误信息");
    }
}

// 公共方法实现

void LoadingDialog::startSystemCheck()
{
    if (m_systemCheckInProgress || !m_systemChecker) {
        return;
    }
    
    m_systemCheckInProgress = true;
    m_isError = false;
    m_checkResults.clear();
    
    // 更新界面
    if (m_titleLabel) {
        m_titleLabel->setText("系统检测中");
    }
    
    if (m_statusLabel) {
        m_statusLabel->setText("正在开始系统检测...");
        m_statusLabel->setProperty("error", false);
        m_statusLabel->style()->polish(m_statusLabel);
    }
    
    // 显示进度控件
    if (m_progressBar) {
        m_progressBar->setVisible(true);
        m_progressBar->setValue(0);
    }
    
    if (m_progressLabel) {
        m_progressLabel->setVisible(true);
        m_progressLabel->setText("准备检测...");
    }
    
    // 隐藏按钮
    if (m_retryButton) m_retryButton->setVisible(false);
    if (m_cancelButton) m_cancelButton->setVisible(false);
    if (m_detailsButton) m_detailsButton->setVisible(false);
    if (m_autoFixButton) m_autoFixButton->setVisible(false);
    
    // 启动动画
    startAnimation();
    
    // 开始检测
    m_systemChecker->startSystemCheck();
    
    Logger::instance().appEvent("开始系统检测流程");
}

void LoadingDialog::startApplicationLoad()
{
    if (m_titleLabel) {
        m_titleLabel->setText("启动应用程序");
    }
    
    if (m_statusLabel) {
        m_statusLabel->setText("正在加载应用程序组件...");
    }
    
    // 隐藏检测相关UI
    if (m_progressBar) m_progressBar->setVisible(false);
    if (m_progressLabel) m_progressLabel->setVisible(false);
    if (m_errorDetailsText) m_errorDetailsText->setVisible(false);
    
    // 启动动画
    startAnimation();
    
    Logger::instance().appEvent("开始应用程序加载流程");
}

// 辅助方法实现

void LoadingDialog::updateErrorDisplay()
{
    if (!m_errorDetailsText) {
        return;
    }
    
    // 检查是否有错误或警告
    bool hasIssues = false;
    for (const auto& result : m_checkResults) {
        if (result.level >= SystemChecker::LEVEL_WARNING) {
            hasIssues = true;
            break;
        }
    }
    
    if (hasIssues && m_detailsButton) {
        m_detailsButton->setVisible(true);
    }
}

void LoadingDialog::displayCheckResults()
{
    if (!m_errorDetailsText) {
        return;
    }
    
    QString detailsText;
    
    for (const auto& result : m_checkResults) {
        detailsText += formatCheckResult(result) + "\n\n";
    }
    
    m_errorDetailsText->setPlainText(detailsText);
}

QString LoadingDialog::formatCheckResult(const SystemChecker::CheckResult& result)
{
    QString formatted;
    
    // 标题和级别
    QString levelText;
    switch (result.level) {
        case SystemChecker::LEVEL_OK:
            levelText = "✓ 正常";
            break;
        case SystemChecker::LEVEL_WARNING:
            levelText = "⚠ 警告";
            break;
        case SystemChecker::LEVEL_ERROR:
            levelText = "✗ 错误";
            break;
        case SystemChecker::LEVEL_FATAL:
            levelText = "✗ 致命错误";
            break;
    }
    
    formatted += QString("[%1] %2\n").arg(levelText).arg(result.title);
    formatted += QString("状态: %1\n").arg(result.message);
    
    // 详细信息
    if (!result.details.isEmpty()) {
        formatted += "详细信息:\n";
        for (const QString& detail : result.details) {
            formatted += QString("  • %1\n").arg(detail);
        }
    }
    
    // 解决方案
    if (!result.solution.isEmpty()) {
        formatted += QString("建议解决方案: %1\n").arg(result.solution);
    }
    
    // 操作提示
    if (result.canRetry) {
        formatted += "可重试检测此项目\n";
    }
    
    if (result.autoFixable) {
        formatted += "可尝试自动修复\n";
    }
    
    return formatted;
}