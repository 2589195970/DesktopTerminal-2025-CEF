#ifndef LOADING_DIALOG_H
#define LOADING_DIALOG_H

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMovie>
#include <QTimer>
#include <QTextEdit>

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

    /**
     * @brief 更新加载状态
     * @param state 当前状态
     * @param message 状态描述
     */
    void updateLoadingState(LoadingState state, const QString& message = QString());

    /**
     * @brief 显示错误信息
     * @param error 错误描述
     * @param details 详细错误信息
     * @param showRetry 是否显示重试按钮
     */
    void showError(const QString& error, const QString& details = QString(), bool showRetry = true);

    /**
     * @brief 显示网络错误
     * @param networkError 网络错误描述
     */
    void showNetworkError(const QString& networkError);

    /**
     * @brief 设置是否可以取消
     * @param cancellable 是否可取消
     */
    void setCancellable(bool cancellable);

    /**
     * @brief 获取当前状态
     */
    LoadingState getCurrentState() const { return m_currentState; }

signals:
    /**
     * @brief 用户请求重试
     */
    void retryRequested();

    /**
     * @brief 用户请求取消
     */
    void cancelRequested();

    /**
     * @brief 用户请求查看详细信息
     */
    void detailsRequested();

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onRetryClicked();
    void onCancelClicked();
    void onDetailsToggled();
    void updateAnimation();

private:
    void setupUI();
    void setupLoadingAnimation();
    void updateStateText();
    void showButtons(bool retry, bool cancel, bool details);
    QString getStateMessage(LoadingState state) const;

private:
    // UI组件
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_buttonLayout;
    
    QLabel* m_iconLabel;
    QLabel* m_titleLabel;
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QTextEdit* m_detailsText;
    
    QPushButton* m_retryButton;
    QPushButton* m_cancelButton;
    QPushButton* m_detailsButton;

    // 加载动画
    QMovie* m_loadingMovie;
    QTimer* m_animationTimer;
    int m_animationFrame;

    // 状态管理
    LoadingState m_currentState;
    QString m_currentMessage;
    QString m_errorDetails;
    bool m_cancellable;
    bool m_detailsVisible;
};

#endif // LOADING_DIALOG_H