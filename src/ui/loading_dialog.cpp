#include "loading_dialog.h"
#include "../logging/logger.h"

#include <QApplication>
#include <QScreen>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QMovie>
#include <QPixmap>
#include <QPainter>
#include <QStyleOption>
#include <QDesktopWidget>

LoadingDialog::LoadingDialog(QWidget *parent)
    : QDialog(parent)
    , m_mainLayout(nullptr)
    , m_buttonLayout(nullptr)
    , m_iconLabel(nullptr)
    , m_titleLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_progressBar(nullptr)
    , m_detailsText(nullptr)
    , m_retryButton(nullptr)
    , m_cancelButton(nullptr)
    , m_detailsButton(nullptr)
    , m_loadingMovie(nullptr)
    , m_animationTimer(nullptr)
    , m_animationFrame(0)
    , m_currentState(Initializing)
    , m_cancellable(false)
    , m_detailsVisible(false)
{
    setupUI();
    setupLoadingAnimation();
    updateStateText();
    
    // 设置窗口属性
    setWindowTitle("智多分机考桌面端 - 正在启动");
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
    setModal(true);
    setFixedSize(400, 300);
    
    // 居中显示
    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }
    
    // 应用样式
    setStyleSheet(R"(
        QDialog {
            background-color: #f0f0f0;
            border: 2px solid #cccccc;
            border-radius: 8px;
        }
        
        QLabel#titleLabel {
            color: #333333;
            font-size: 16px;
            font-weight: bold;
            margin: 10px 0px;
        }
        
        QLabel#statusLabel {
            color: #666666;
            font-size: 12px;
            margin: 5px 0px;
        }
        
        QProgressBar {
            border: 2px solid #cccccc;
            border-radius: 5px;
            text-align: center;
            background-color: #ffffff;
        }
        
        QProgressBar::chunk {
            background-color: #4CAF50;
            border-radius: 3px;
        }
        
        QPushButton {
            background-color: #ffffff;
            border: 1px solid #cccccc;
            border-radius: 4px;
            padding: 8px 16px;
            font-size: 12px;
            min-width: 70px;
        }
        
        QPushButton:hover {
            background-color: #f5f5f5;
            border-color: #999999;
        }
        
        QPushButton:pressed {
            background-color: #e0e0e0;
        }
        
        QTextEdit {
            border: 1px solid #cccccc;
            border-radius: 4px;
            background-color: #ffffff;
            font-family: 'Courier New', monospace;
            font-size: 10px;
        }
    )");
}

LoadingDialog::~LoadingDialog()
{
    if (m_animationTimer) {
        m_animationTimer->stop();
        delete m_animationTimer;
    }
    
    if (m_loadingMovie) {
        delete m_loadingMovie;
    }
}

void LoadingDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(10);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // 图标标签
    m_iconLabel = new QLabel();
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setFixedSize(48, 48);
    m_mainLayout->addWidget(m_iconLabel);
    
    // 标题标签
    m_titleLabel = new QLabel("正在启动机考桌面端");
    m_titleLabel->setObjectName("titleLabel");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(m_titleLabel);
    
    // 状态标签
    m_statusLabel = new QLabel("正在初始化...");
    m_statusLabel->setObjectName("statusLabel");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    m_mainLayout->addWidget(m_statusLabel);
    
    // 进度条
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 0); // 无限进度条
    m_progressBar->setTextVisible(false);
    m_mainLayout->addWidget(m_progressBar);
    
    // 详细信息文本（初始隐藏）
    m_detailsText = new QTextEdit();
    m_detailsText->setVisible(false);
    m_detailsText->setReadOnly(true);
    m_detailsText->setMaximumHeight(100);
    m_mainLayout->addWidget(m_detailsText);
    
    // 按钮布局
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->addStretch();
    
    m_retryButton = new QPushButton("重试");
    m_retryButton->setVisible(false);
    connect(m_retryButton, &QPushButton::clicked, this, &LoadingDialog::onRetryClicked);
    m_buttonLayout->addWidget(m_retryButton);
    
    m_detailsButton = new QPushButton("详细信息");
    m_detailsButton->setVisible(false);
    connect(m_detailsButton, &QPushButton::clicked, this, &LoadingDialog::onDetailsToggled);
    m_buttonLayout->addWidget(m_detailsButton);
    
    m_cancelButton = new QPushButton("取消");
    m_cancelButton->setVisible(false);
    connect(m_cancelButton, &QPushButton::clicked, this, &LoadingDialog::onCancelClicked);
    m_buttonLayout->addWidget(m_cancelButton);
    
    m_mainLayout->addLayout(m_buttonLayout);
}

void LoadingDialog::setupLoadingAnimation()
{
    // 创建简单的旋转加载动画
    m_animationTimer = new QTimer(this);
    connect(m_animationTimer, &QTimer::timeout, this, &LoadingDialog::updateAnimation);
    m_animationTimer->start(100); // 100ms间隔
}

void LoadingDialog::updateAnimation()
{
    // 创建旋转的加载图标
    QPixmap pixmap(48, 48);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制旋转的圆圈
    painter.translate(24, 24);
    painter.rotate(m_animationFrame * 36); // 每次旋转36度
    
    QPen pen(QColor(76, 175, 80), 4); // 绿色圆环
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    
    // 绘制部分圆弧
    painter.drawArc(-20, -20, 40, 40, 0, 240 * 16); // 240度的弧
    
    m_iconLabel->setPixmap(pixmap);
    
    m_animationFrame = (m_animationFrame + 1) % 10;
}

void LoadingDialog::updateLoadingState(LoadingState state, const QString& message)
{
    m_currentState = state;
    m_currentMessage = message;
    
    updateStateText();
    
    if (state == Completed) {
        m_animationTimer->stop();
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(100);
        
        // 完成后短暂显示然后关闭
        QTimer::singleShot(500, this, &QDialog::accept);
    } else if (state == Failed) {
        m_animationTimer->stop();
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(0);
        showButtons(true, true, !m_errorDetails.isEmpty());
    }
}

void LoadingDialog::showError(const QString& error, const QString& details, bool showRetry)
{
    m_currentState = Failed;
    m_currentMessage = error;
    m_errorDetails = details;
    
    m_titleLabel->setText("启动失败");
    m_statusLabel->setText(error);
    m_statusLabel->setStyleSheet("color: #f44336; font-weight: bold;"); // 红色错误文本
    
    if (!details.isEmpty()) {
        m_detailsText->setPlainText(details);
    }
    
    m_animationTimer->stop();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    
    showButtons(showRetry, true, !details.isEmpty());
    
    // 记录错误日志
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
    
    // 根据状态设置进度条样式
    switch (m_currentState) {
        case Initializing:
        case CheckingNetwork:
        case VerifyingCEF:
        case LoadingCEF:
        case CreatingBrowser:
            m_progressBar->setRange(0, 0); // 无限进度条
            break;
        case Completed:
            m_progressBar->setRange(0, 100);
            m_progressBar->setValue(100);
            break;
        case Failed:
            m_progressBar->setRange(0, 100);
            m_progressBar->setValue(0);
            break;
    }
}

void LoadingDialog::showButtons(bool retry, bool cancel, bool details)
{
    m_retryButton->setVisible(retry);
    m_cancelButton->setVisible(cancel);
    m_detailsButton->setVisible(details);
    
    // 调整对话框大小
    if (details && m_detailsVisible) {
        setFixedSize(400, 450);
    } else {
        setFixedSize(400, 300);
    }
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

void LoadingDialog::onRetryClicked()
{
    // 重置状态
    m_currentState = Initializing;
    m_errorDetails.clear();
    m_detailsVisible = false;
    
    m_titleLabel->setText("智多分机考桌面端 - 正在启动");
    m_statusLabel->setText("正在重新初始化...");
    m_statusLabel->setStyleSheet("color: #666666; font-weight: normal;");
    
    m_detailsText->setVisible(false);
    showButtons(false, m_cancellable, false);
    
    // 重新开始动画
    m_animationTimer->start(100);
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
    m_detailsText->setVisible(m_detailsVisible);
    
    if (m_detailsVisible) {
        m_detailsButton->setText("隐藏详细信息");
        setFixedSize(400, 450);
    } else {
        m_detailsButton->setText("详细信息");
        setFixedSize(400, 300);
    }
    
    emit detailsRequested();
}