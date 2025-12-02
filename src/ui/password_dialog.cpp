#include "password_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QApplication>
#include <QScreen>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFrame>

PasswordDialog::PasswordDialog(const QString& title, const QString& label, QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_backgroundFrame(nullptr)
    , m_contentCard(nullptr)
    , m_titleLabel(nullptr)
    , m_promptLabel(nullptr)
    , m_passwordEdit(nullptr)
    , m_confirmButton(nullptr)
    , m_cancelButton(nullptr)
{
    // 设置为模态对话框
    setModal(true);

    setupUI(title, label);
    applyStyles();

    // 居中显示 - 增大尺寸
    int dialogWidth = 480;
    int dialogHeight = 280;
    setFixedSize(dialogWidth, dialogHeight);

    if (QScreen *screen = QApplication::primaryScreen()) {
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - dialogWidth) / 2;
        int y = (screenGeometry.height() - dialogHeight) / 2;
        move(x, y);
    }
}

PasswordDialog::~PasswordDialog()
{
}

QString PasswordDialog::password() const
{
    return m_passwordEdit ? m_passwordEdit->text() : QString();
}

void PasswordDialog::setupUI(const QString& title, const QString& label)
{
    setObjectName("passwordDialogRoot");

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    m_backgroundFrame = new QFrame(this);
    m_backgroundFrame->setObjectName("passwordBackgroundFrame");
    outerLayout->addWidget(m_backgroundFrame);

    auto* backgroundLayout = new QVBoxLayout(m_backgroundFrame);
    backgroundLayout->setContentsMargins(32, 32, 32, 32);
    backgroundLayout->setSpacing(0);

    // 内容卡片
    m_contentCard = new QFrame(m_backgroundFrame);
    m_contentCard->setObjectName("passwordCard");
    auto* cardLayout = new QVBoxLayout(m_contentCard);
    cardLayout->setContentsMargins(32, 28, 32, 28);
    cardLayout->setSpacing(20);

    // 标题
    m_titleLabel = new QLabel(title, m_contentCard);
    m_titleLabel->setObjectName("passwordTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(m_titleLabel);

    // 提示文字
    m_promptLabel = new QLabel(label, m_contentCard);
    m_promptLabel->setObjectName("passwordPrompt");
    m_promptLabel->setAlignment(Qt::AlignCenter);
    m_promptLabel->setWordWrap(true);
    cardLayout->addWidget(m_promptLabel);

    // 密码输入框
    m_passwordEdit = new QLineEdit(m_contentCard);
    m_passwordEdit->setObjectName("passwordInput");
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("请输入密码");
    m_passwordEdit->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(m_passwordEdit);

    // 按钮布局
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);

    buttonLayout->addStretch();

    m_cancelButton = new QPushButton("取消", m_contentCard);
    m_cancelButton->setObjectName("passwordCancelButton");
    m_cancelButton->setMinimumSize(120, 44);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_cancelButton);

    m_confirmButton = new QPushButton("确认", m_contentCard);
    m_confirmButton->setObjectName("passwordConfirmButton");
    m_confirmButton->setMinimumSize(120, 44);
    m_confirmButton->setDefault(true);
    connect(m_confirmButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(m_confirmButton);

    buttonLayout->addStretch();

    cardLayout->addLayout(buttonLayout);

    backgroundLayout->addWidget(m_contentCard);

    // 设置焦点到密码输入框
    m_passwordEdit->setFocus();
}

void PasswordDialog::applyStyles()
{
    setStyleSheet(R"(
        PasswordDialog#passwordDialogRoot {
            background-color: transparent;
        }

        QFrame#passwordBackgroundFrame {
            border-radius: 20px;
            background: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1,
                stop:0 #fafafa,
                stop:1 #f0f0f2);
            border: 1px solid #e5e5e7;
        }

        QFrame#passwordCard {
            background-color: #ffffff;
            border-radius: 16px;
            border: 1px solid #e8e8ea;
        }

        QLabel#passwordTitle {
            font-size: 24px;
            font-weight: 600;
            color: #1d1d1f;
            padding-bottom: 4px;
        }

        QLabel#passwordPrompt {
            font-size: 15px;
            color: #6e6e73;
            padding-bottom: 4px;
        }

        QLineEdit#passwordInput {
            font-size: 18px;
            color: #1d1d1f;
            background-color: #f5f5f7;
            border: 1px solid #d2d2d7;
            border-radius: 10px;
            padding: 14px 20px;
            min-height: 24px;
        }

        QLineEdit#passwordInput:focus {
            border: 2px solid #007aff;
            background-color: #ffffff;
        }

        QPushButton#passwordCancelButton {
            font-size: 15px;
            font-weight: 500;
            color: #6e6e73;
            background-color: #ffffff;
            border: 1px solid #d2d2d7;
            border-radius: 10px;
            padding: 10px 24px;
        }

        QPushButton#passwordCancelButton:hover {
            background-color: #f5f5f7;
        }

        QPushButton#passwordCancelButton:pressed {
            background-color: #e8e8ea;
        }

        QPushButton#passwordConfirmButton {
            font-size: 15px;
            font-weight: 500;
            color: #ffffff;
            background-color: #007aff;
            border: none;
            border-radius: 10px;
            padding: 10px 24px;
        }

        QPushButton#passwordConfirmButton:hover {
            background-color: #0051d5;
        }

        QPushButton#passwordConfirmButton:pressed {
            background-color: #004bb8;
        }
    )");
}

void PasswordDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        reject();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        accept();
        return;
    }
    QDialog::keyPressEvent(event);
}
