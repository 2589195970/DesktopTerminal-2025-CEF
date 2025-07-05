#ifndef LOADING_DIALOG_H
#define LOADING_DIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QMovie>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QParallelAnimationGroup>

/**
 * @brief CEF加载进度对话框
 * 
 * 在CEF初始化过程中显示加载动画和进度信息，
 * 支持错误状态显示和用户交互
 */
class LoadingDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 加载状态枚举
     */
    enum LoadingState {
        Initializing,      // 正在初始化
        CheckingNetwork,   // 检查网络连接
        VerifyingCEF,      // 验证CEF安装
        LoadingCEF,        // 加载CEF组件
        CreatingBrowser,   // 创建浏览器实例
        Completed,         // 完成
        Failed             // 失败
    };

    explicit LoadingDialog(QWidget *parent = nullptr);
    ~LoadingDialog();

    void updateLoadingState(LoadingState state, const QString& message = QString());
    void showError(const QString& error, const QString& details = QString(), bool showRetry = true);
    void showNetworkError(const QString& networkError);
    void setCancellable(bool cancellable);
    LoadingState getCurrentState() const { return m_currentState; }

signals:
    void retryRequested();
    void cancelRequested();
    void detailsRequested();

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onRetryClicked();
    void onCancelClicked();
    void onDetailsToggled();
    void updateLoadingAnimation();
    void updateProgressAnimation();

private:
    void setupUI();
    void setupAnimations();
    void setupShadowEffects();
    void setupProgressSteps();
    void updateStateText();
    void updateStateIcon();
    void updateProgressSteps();
    void showButtons(bool retry, bool cancel, bool details);
    void animateStateTransition();
    void startLoadingAnimation();
    void stopLoadingAnimation();
    QString getStateMessage(LoadingState state) const;
    QString getModernStyleSheet() const;
    QPixmap createStateIcon(LoadingState state) const;
    QPixmap createLoadingRing(int frame) const;
    QColor getStateColor(LoadingState state) const;
    int getStateProgress(LoadingState state) const;
    
    // DPI感知工具函数
    double getDpiScale() const;
    int scaledSize(int baseSize) const;
    int scaledFont(int baseFontSize) const;
    QSize scaledWindowSize(int baseWidth, int baseHeight) const;
    QSize calculateOptimalWindowSize() const;
    
    // 图标绘制辅助函数
    void drawGearIcon(QPainter *painter, int size) const;
    void drawWifiIcon(QPainter *painter, int size) const;
    void drawModernWifiIcon(QPainter *painter, int size) const;
    void drawShieldIcon(QPainter *painter, int size) const;
    void drawGlobeIcon(QPainter *painter, int size) const;
    void drawMonitorIcon(QPainter *painter, int size) const;
    void drawCheckIcon(QPainter *painter, int size) const;
    void drawExclamationIcon(QPainter *painter, int size) const;
    
    void applyModernStyles();

private:
    // 布局组件
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_buttonLayout;
    QVBoxLayout *m_headerLayout;
    QHBoxLayout *m_iconLayout;
    QHBoxLayout *m_stepsLayout;
    
    // UI组件
    QLabel *m_iconLabel;
    QLabel *m_loadingRingLabel;
    QLabel *m_titleLabel;
    QLabel *m_statusLabel;
    QLabel *m_subtitleLabel;
    QProgressBar *m_progressBar;
    QLabel *m_progressLabel;
    QTextEdit *m_detailsText;
    QPushButton *m_retryButton;
    QPushButton *m_cancelButton;
    QPushButton *m_detailsButton;
    
    // 进度步骤指示器
    QList<QLabel*> m_stepLabels;
    
    // 动画相关
    QTimer *m_loadingAnimationTimer;
    QTimer *m_progressAnimationTimer;
    QPropertyAnimation *m_iconAnimation;
    QPropertyAnimation *m_progressAnimation;
    QSequentialAnimationGroup *m_stateTransitionGroup;
    
    // 状态变量
    int m_loadingFrame;
    int m_progressFrame;
    LoadingState m_currentState;
    QString m_currentMessage;
    QString m_errorDetails;
    bool m_cancellable;
    bool m_detailsVisible;
    
    // 视觉效果
    QGraphicsDropShadowEffect *m_shadowEffect;
    
    // 窗口大小管理
    QSize m_targetSize;
};

#endif // LOADING_DIALOG_H