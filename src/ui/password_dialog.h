#ifndef PASSWORD_DIALOG_H
#define PASSWORD_DIALOG_H

#include <QDialog>

class QLineEdit;
class QLabel;
class QPushButton;
class QFrame;
class QKeyEvent;

class PasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PasswordDialog(const QString& title, const QString& label, QWidget *parent = nullptr);
    ~PasswordDialog();

    QString password() const;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setupUI(const QString& title, const QString& label);
    void applyStyles();

    QFrame* m_backgroundFrame;
    QFrame* m_contentCard;
    QLabel* m_titleLabel;
    QLabel* m_promptLabel;
    QLineEdit* m_passwordEdit;
    QPushButton* m_confirmButton;
    QPushButton* m_cancelButton;
};

#endif // PASSWORD_DIALOG_H
