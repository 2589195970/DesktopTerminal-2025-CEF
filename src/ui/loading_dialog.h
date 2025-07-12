#ifndef LOADING_DIALOG_H
#define LOADING_DIALOG_H

#include <QDialog>
#include <QTimer>
#include <QString>

class QLabel;
class QPushButton;
class QPropertyAnimation;

/**
 * @brief 加载对话框类
 * 
 * 在应用启动和网页加载期间显示加载动画和状态信息，
 * 避免白屏给用户造成系统问题的误解。
 */
class LoadingDialog : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(qreal rotation READ rotation WRITE setRotation)

public:
    explicit LoadingDialog(QWidget *parent = nullptr);
    ~LoadingDialog();

    // 状态管理
    void setStatus(const QString& status);
    void setError(const QString& error);
    void setProgress(int value, int maximum = 100);
    
    // 控制方法
    void startAnimation();
    void stopAnimation();
    
    // 属性访问
    qreal rotation() const { return m_rotation; }
    void setRotation(qreal rotation);
    
signals:
    // 用户交互信号
    void retryClicked();
    void cancelClicked();
    
protected:
    // 事件处理
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    
private:
    // UI初始化
    void setupUI();
    void applyStyles();
    
    // 动画相关
    void createRotationAnimation();
    void drawLoadingIcon(QPainter& painter);
    
private:
    // UI组件
    QLabel* m_titleLabel;
    QLabel* m_statusLabel;
    QPushButton* m_retryButton;
    QPushButton* m_cancelButton;
    
    // 动画相关
    QTimer* m_animationTimer;
    QPropertyAnimation* m_rotationAnimation;
    qreal m_rotation;
    
    // 状态
    bool m_isError;
    int m_progressValue;
    int m_progressMax;
    
    // 样式常量
    static const int DIALOG_WIDTH = 800;
    static const int DIALOG_HEIGHT = 600;
    static const int ICON_SIZE = 48;
    static const int ANIMATION_DURATION = 1000;
};

#endif // LOADING_DIALOG_H