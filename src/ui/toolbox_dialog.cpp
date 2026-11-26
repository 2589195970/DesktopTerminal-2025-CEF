#include "toolbox_dialog.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

ToolboxDialog::ToolboxDialog(const terminal::SeatDisplayStore &store, QWidget *parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint),
      m_store(store)
{
    setWindowTitle(QStringLiteral("管理员工具箱"));
    setObjectName(QStringLiteral("toolbox"));
    setStyleSheet(QStringLiteral(R"(
        QDialog#toolbox { background: #ffffff; border: 1px solid #acb9cc; }
        QLabel { background: transparent; color: #192538; font-size: 14px; }
        QLabel#title { font-size: 23px; font-weight: 700; }
        QLabel#toolboxTitle { font-size: 19px; font-weight: 700; }
        QLabel#hint { color: #475569; }
        QFrame#toolGrid { background: #cbd7e6; }
        QFrame#emptyToolCell { background: #ffffff; }
        QFrame#seatCard { background: #f3f7fc; border: 1px solid #d5e0ef; border-radius: 12px; }
        QLabel#seatNumber { font-size: 26px; font-weight: 700; }
        QLabel#seatSource { color: #24466d; }
        QLineEdit, QPushButton {
            background: #ffffff; color: #192538; border: 2px solid #c9d4e2;
            border-radius: 8px; padding: 0 12px; min-height: 40px; font-size: 14px;
        }
        QPushButton:hover { background: #edf3fa; }
        QPushButton#toolEntry {
            background: #f3f7fc; color: #183d6b; border: 1px solid transparent;
            border-radius: 0; min-height: 34px; padding: 0 8px; font-weight: 600;
        }
        QPushButton#toolEntry:hover { background: #e6effa; }
        QPushButton#closeToolbox { min-height: 30px; }
        QPushButton#primary { background: #1554a0; color: #ffffff; border-color: #1554a0; font-weight: 600; }
        QPushButton#primary:hover { background: #103f7c; }
        QLineEdit:focus, QPushButton:focus { border-color: #0874d0; }
        QPushButton#toolEntry:focus { border-color: #0874d0; }
        QPushButton#primary:focus { border-color: #72b9ff; }
        QPushButton:disabled { background: #f4f6f9; color: #64748b; border-color: #e0e5ed; }
    )"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(26, 24, 26, 24);

    m_home = new QWidget(this);
    m_home->setMinimumWidth(300);
    auto *homeLayout = new QVBoxLayout(m_home);
    homeLayout->setContentsMargins(0, 0, 0, 0);
    homeLayout->setSpacing(10);
    auto *title = new QLabel(QStringLiteral("管理员工具箱"), m_home);
    title->setObjectName(QStringLiteral("toolboxTitle"));
    homeLayout->addWidget(title);

    auto *tools = new QFrame(m_home);
    tools->setObjectName(QStringLiteral("toolGrid"));
    auto *toolsLayout = new QGridLayout(tools);
    toolsLayout->setContentsMargins(1, 1, 1, 1);
    toolsLayout->setSpacing(1);

    m_edit = new QPushButton(QStringLiteral("设置座号"), tools);
    m_edit->setObjectName(QStringLiteral("toolEntry"));
    toolsLayout->addWidget(m_edit, 0, 0);
    toolsLayout->setColumnStretch(0, 1);
    auto *reload = new QPushButton(QStringLiteral("刷新页面"), tools);
    reload->setObjectName(QStringLiteral("toolEntry"));
    toolsLayout->addWidget(reload, 0, 1);
    toolsLayout->setColumnStretch(1, 1);
    connect(reload, &QPushButton::clicked, this, [this] {
        emit reloadRequested();
        accept();
    });
    for (int column = 2; column < 3; ++column) {
        auto *cell = new QFrame(tools);
        cell->setObjectName(QStringLiteral("emptyToolCell"));
        toolsLayout->addWidget(cell, 0, column);
        toolsLayout->setColumnStretch(column, 1);
    }
    homeLayout->addWidget(tools);

    auto *close = new QPushButton(QStringLiteral("关闭"), m_home);
    close->setObjectName(QStringLiteral("closeToolbox"));
    homeLayout->addWidget(close, 0, Qt::AlignRight);
    layout->addWidget(m_home);

    m_editor = new QWidget(this);
    m_editor->setMinimumWidth(388);
    auto *form = new QVBoxLayout(m_editor);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(12);
    auto *editorTitle = new QLabel(QStringLiteral("设置座号"), m_editor);
    editorTitle->setObjectName(QStringLiteral("title"));
    form->addWidget(editorTitle);

    auto *card = new QFrame(m_editor);
    card->setObjectName(QStringLiteral("seatCard"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 12, 16, 12);
    cardLayout->setSpacing(4);
    cardLayout->addWidget(new QLabel(QStringLiteral("当前座号"), card));
    m_number = new QLabel(card);
    m_number->setObjectName(QStringLiteral("seatNumber"));
    m_number->setTextFormat(Qt::PlainText);
    m_number->setWordWrap(true);
    cardLayout->addWidget(m_number);
    m_source = new QLabel(card);
    m_source->setObjectName(QStringLiteral("seatSource"));
    cardLayout->addWidget(m_source);
    form->addWidget(card);

    auto *label = new QLabel(QStringLiteral("手动座号"), m_editor);
    m_input = new QLineEdit(m_editor);
    m_input->setMaxLength(255);
    m_input->setPlaceholderText(QStringLiteral("例如 07 或 A01"));
    m_input->setAccessibleName(label->text());
    label->setBuddy(m_input);
    form->addWidget(label);
    form->addWidget(m_input);
    auto *hint = new QLabel(QStringLiteral("补填只用于本机显示。后台成功下发后，以后台座号为准。"), m_editor);
    hint->setObjectName(QStringLiteral("hint"));
    hint->setWordWrap(true);
    form->addWidget(hint);

    m_message = new QLabel(m_editor);
    m_message->setWordWrap(true);
    m_message->hide();
    form->addWidget(m_message);

    auto *actions = new QHBoxLayout;
    m_clear = new QPushButton(QStringLiteral("清除手动座号"), m_editor);
    auto *back = new QPushButton(QStringLiteral("返回工具箱"), m_editor);
    auto *saveButton = new QPushButton(QStringLiteral("保存"), m_editor);
    saveButton->setObjectName(QStringLiteral("primary"));
    actions->addWidget(m_clear);
    actions->addStretch();
    actions->addWidget(back);
    actions->addWidget(saveButton);
    form->addLayout(actions);
    layout->addWidget(m_editor);
    m_editor->hide();

    for (auto *button : {m_clear, back, saveButton}) {
        button->setAutoDefault(false);
    }
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_edit, &QPushButton::clicked, this, &ToolboxDialog::showEditor);
    connect(back, &QPushButton::clicked, this, &ToolboxDialog::showHome);
    connect(m_clear, &QPushButton::clicked, this, [this] { save({}); });
    const auto submit = [this] {
        const QString number = m_input->text().trimmed();
        if (!terminal::SeatDisplayStore::isValidNumber(number)) {
            showMessage(QStringLiteral("请输入有效座号。"), true);
            m_input->setFocus();
            return;
        }
        save(number);
    };
    connect(saveButton, &QPushButton::clicked, this, submit);
    connect(m_input, &QLineEdit::returnPressed, this, submit);
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        if (m_editor->isVisible()) {
            refreshSeat();
        }
    });
    timer->start(1500);
    m_edit->setFocus();
}

void ToolboxDialog::reject()
{
    if (m_editor->isVisible()) {
        showHome();
        return;
    }
    QDialog::reject();
}

void ToolboxDialog::showHome()
{
    m_editor->hide();
    m_home->show();
    m_edit->setFocus();
    adjustSize();
}

void ToolboxDialog::showEditor()
{
    m_message->hide();
    m_home->hide();
    m_editor->show();
    refreshSeat(true);
    m_input->setFocus();
    m_input->selectAll();
    adjustSize();
}

void ToolboxDialog::save(const QString &number)
{
    QString error;
    if (!m_store.setManual(number, error)) {
        showMessage(error, true);
        return;
    }

    emit seatChanged();
    refreshSeat(true);
    showMessage(number.isEmpty() ? QStringLiteral("已清除手动座号。") : QStringLiteral("座号已保存。"), false);
}

void ToolboxDialog::refreshSeat(bool fillInput)
{
    terminal::SeatDisplay seat;
    QString error;
    const bool read = m_store.read(seat, error);
    m_number->setText(seat.number().isEmpty() ? QStringLiteral("待设置") : seat.number());
    m_source->setText(!read ? QStringLiteral("本机座号读取失败")
                     : !seat.manual.isEmpty() ? QStringLiteral("手动补填")
                     : !seat.assigned.isEmpty() ? QStringLiteral("后台下发") : QStringLiteral("等待后台下发或手动设置"));
    m_clear->setEnabled(!seat.manual.isEmpty());
    if (fillInput) {
        m_input->setText(seat.manual);
        if (!read) {
            showMessage(error, true);
        }
    }
}

void ToolboxDialog::showMessage(const QString &message, bool error)
{
    m_message->setStyleSheet(error ? QStringLiteral("color: #a02020;") : QStringLiteral("color: #176346;"));
    m_message->setText(message);
    m_message->show();
    adjustSize();
}
