#ifndef EXAM_SEAT_PAGE_H
#define EXAM_SEAT_PAGE_H

#include <QWidget>

class QFrame;
class QLabel;
class QPaintEvent;
class QResizeEvent;

class ExamSeatPage final : public QWidget
{
public:
    enum class State { Waiting, Unused, Synchronizing };

    explicit ExamSeatPage(QWidget* parent = nullptr);
    void showSession(State state, const QString& seat, const QString& session);
    static QLabel* createTerminalInfo(QWidget* parent);
    static QString terminalDetails();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void adjustLayout();

    QFrame* m_card = nullptr;
    QLabel* m_session = nullptr;
    QWidget* m_seatDetails = nullptr;
    QLabel* m_seatNumber = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_message = nullptr;
    bool m_hasSeat = false;
};

#endif
