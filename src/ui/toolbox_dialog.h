#ifndef TOOLBOX_DIALOG_H
#define TOOLBOX_DIALOG_H

#include <QDialog>

#include "../terminal_service/seat_display_store.h"

class QLabel;
class QLineEdit;
class QPushButton;

class ToolboxDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ToolboxDialog(const terminal::SeatDisplayStore &store, QWidget *parent = nullptr);

signals:
    void seatChanged();
    void reloadRequested();

protected:
    void reject() override;

private:
    void showHome();
    void showEditor();
    void save(const QString &number);
    void refreshSeat(bool fillInput = false);
    void showMessage(const QString &message, bool error);

    const terminal::SeatDisplayStore &m_store;
    QWidget *m_home = nullptr;
    QWidget *m_editor = nullptr;
    QLabel *m_number = nullptr;
    QLabel *m_source = nullptr;
    QLabel *m_message = nullptr;
    QLineEdit *m_input = nullptr;
    QPushButton *m_edit = nullptr;
    QPushButton *m_clear = nullptr;
};

#endif
