#ifndef LOADING_DIALOG_H
#define LOADING_DIALOG_H

#include <QDialog>
#include <QTimer>
#include <QString>
#include <QProgressBar>
#include <QTextEdit>
#include <QScrollArea>
#include <QVBoxLayout>
#include "../core/system_checker.h"

class QLabel;
class QPushButton;
class QPropertyAnimation;

/**
 * @brief 智能加载对话框类
 * 
 * 集成系统检测和预加载功能的智能启动界面：
 * - 系统兼容性检测（Qt版本、OpenGL、操作系统）
 * - 网络连接检测（连接状态、质量、目标可达性）
 * - CEF依赖完整性检查（文件存在、版本兼容、完整性）
 * - 配置和权限验证（文件权限、管理员权限、磁盘空间）
 * - 组件预加载和错误处理
 */
class LoadingDialog : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(qreal rotation READ rotation WRITE setRotation)

public:
    explicit LoadingDialog(QWidget *parent = nullptr);
    ~LoadingDialog();

    // 主要功能
    void startSystemCheck();
    void startApplicationLoad();
    
    // 状态管理
    void setStatus(const QString& status);
    void setError(const QString& error);
    void setProgress(int value, int maximum = 100);
    void setDetailedStatus(const QString& title, const QString& message, const QString& level = "info");
    
    // 控制方法
    void startAnimation();
    void stopAnimation();
    void showErrorDetails(bool show = true);
    
    // 属性访问
    qreal rotation() const { return m_rotation; }
    void setRotation(qreal rotation);
    
signals:
    // 用户交互信号
    void retryClicked();
    void cancelClicked();
    void systemCheckCompleted(bool success);
    void readyToStartApplication();
    
protected:
    // 事件处理
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    
private slots:
    // 系统检测相关槽函数
    void onCheckProgress(int current, int total, const QString& message);
    void onCheckItemCompleted(const SystemChecker::CheckResult& result);
    void onCheckCompleted(bool success, const QList<SystemChecker::CheckResult>& results);
    void onAutoFixCompleted(int fixed);
    void onRetrySystemCheck();
    void onShowErrorDetails();
    
private:
    // UI初始化
    void setupUI();
    void setupEnhancedUI();
    void applyStyles();
    void updateErrorDisplay();
    
    // 动画相关
    void createRotationAnimation();
    void drawLoadingIcon(QPainter& painter);
    
    // 系统检测相关
    void initializeSystemChecker();
    void displayCheckResults();
    QString formatCheckResult(const SystemChecker::CheckResult& result);
    
private:
    // 基础UI组件
    QLabel* m_titleLabel;
    QLabel* m_statusLabel;
    QPushButton* m_retryButton;
    QPushButton* m_cancelButton;
    
    // 增强UI组件
    QProgressBar* m_progressBar;
    QLabel* m_progressLabel;
    QTextEdit* m_errorDetailsText;
    QScrollArea* m_detailsScrollArea;
    QPushButton* m_detailsButton;
    QPushButton* m_autoFixButton;
    QVBoxLayout* m_mainLayout;
    
    // 系统检测
    SystemChecker* m_systemChecker;
    QList<SystemChecker::CheckResult> m_checkResults;
    
    // 动画相关
    QTimer* m_animationTimer;
    QPropertyAnimation* m_rotationAnimation;
    qreal m_rotation;
    
    // 状态
    bool m_isError;
    bool m_systemCheckInProgress;
    bool m_showingDetails;
    int m_progressValue;
    int m_progressMax;
    
    // 样式常量
    static const int DIALOG_WIDTH = 800;
    static const int DIALOG_HEIGHT = 600;
    static const int ICON_SIZE = 48;
    static const int ANIMATION_DURATION = 1000;
};

#endif // LOADING_DIALOG_H