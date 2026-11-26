#include "exam_seat_page.h"

#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QNetworkInterface>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QStringList>
#include <QSysInfo>
#include <QVBoxLayout>

namespace {

constexpr int PageInset = 24;
constexpr int CardWidth = 1040;
constexpr int ContentInset = 38;
constexpr int SeatNumberSize = 184;
constexpr int MinimumSeatNumberSize = 12;
constexpr int StatusWithSeatSize = 20;
constexpr int StatusWithoutSeatSize = 32;
// 与学生端 PC 登录页 .left-bg 使用同一背景色。
constexpr char StudentLoginBlue[] = "#4f99fd";
constexpr char TerminalInfoText[] = "#152b48";

QLabel* makeLabel(QWidget* parent, const QString& text, int size, const char* color, bool strong = false)
{
    auto* label = new QLabel(text, parent);
    label->setTextFormat(Qt::PlainText);
    QFont font = QApplication::font();
#ifdef Q_OS_WIN
    font.setFamily(QStringLiteral("Microsoft YaHei"));
#endif
    font.setPixelSize(size);
    font.setWeight(strong ? QFont::DemiBold : QFont::Normal);
    label->setFont(font);
    label->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(QLatin1String(color)));
    return label;
}

}

ExamSeatPage::ExamSeatPage(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("examSeatPage"));
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(QStringLiteral(
        "QWidget#examSeatPage { background: %1; }"
        "QFrame#examSeatCard { background: #ffffff; border-radius: 26px; }"
        "QFrame#examSeatSessionMark { background: %1; border-radius: 1px; }"
        "QFrame#examSeatDivider { background: #e9eef5; }"
        "QFrame#examSeatNotice { background: #eff5ff; border-radius: 12px; }")
        .arg(QLatin1String(StudentLoginBlue)));
    auto* pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(PageInset, 16, PageInset, 16);
    pageLayout->addStretch();

    m_card = new QFrame(this);
    m_card->setObjectName(QStringLiteral("examSeatCard"));
    m_card->setMaximumWidth(CardWidth);
    auto* shadow = new QGraphicsDropShadowEffect(m_card);
    shadow->setColor(QColor(19, 52, 111, 42));
    shadow->setBlurRadius(48);
    shadow->setOffset(0, 10);
    m_card->setGraphicsEffect(shadow);
    auto* cardLayout = new QVBoxLayout(m_card);
    cardLayout->setContentsMargins(ContentInset, 0, ContentInset, 0);
    cardLayout->setSpacing(0);
    auto* centered = new QHBoxLayout;
    centered->addStretch();
    centered->addWidget(m_card, 24);
    centered->addStretch();
    pageLayout->addLayout(centered);
    pageLayout->addStretch();

    auto* sessionLayout = new QHBoxLayout;
    sessionLayout->setContentsMargins(0, 24, 0, 24);
    sessionLayout->setSpacing(12);
    auto* sessionMark = new QFrame(m_card);
    sessionMark->setObjectName(QStringLiteral("examSeatSessionMark"));
    sessionMark->setFixedSize(4, 22);
    sessionLayout->addWidget(sessionMark);
    auto* sessionLabel = makeLabel(m_card, QStringLiteral("考试场次"), 18, "#52637a");
    sessionLayout->addWidget(sessionLabel);
    m_session = makeLabel(m_card, {}, 26, "#182943", true);
    m_session->setWordWrap(true);
    m_session->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    sessionLayout->addWidget(m_session, 1);
    cardLayout->addLayout(sessionLayout);
    auto* divider = new QFrame(m_card);
    divider->setObjectName(QStringLiteral("examSeatDivider"));
    divider->setFixedHeight(1);
    cardLayout->addWidget(divider);

    auto* contentLayout = new QVBoxLayout;
    contentLayout->setContentsMargins(0, 24, 0, 24);
    contentLayout->setSpacing(8);
    m_seatDetails = new QWidget(m_card);
    auto* seatLayout = new QVBoxLayout(m_seatDetails);
    seatLayout->setContentsMargins(0, 0, 0, 0);
    seatLayout->setSpacing(0);
    auto* seatLabel = makeLabel(m_seatDetails, QStringLiteral("考试座号"), 30, "#52637a", true);
    m_seatNumber = makeLabel(m_seatDetails, {}, SeatNumberSize, "#10294d", true);
    m_seatNumber->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    auto* notice = new QFrame(m_card);
    notice->setObjectName(QStringLiteral("examSeatNotice"));
    auto* noticeLayout = new QVBoxLayout(notice);
    noticeLayout->setContentsMargins(24, 10, 24, 10);
    noticeLayout->setSpacing(6);
    m_status = makeLabel(notice, {}, StatusWithoutSeatSize, "#234e86", true);
    m_status->setWordWrap(true);
    m_message = makeLabel(notice, {}, 18, "#52637a");
    m_message->setWordWrap(true);
    for (auto* label : {seatLabel, m_seatNumber, m_status, m_message}) {
        label->setAlignment(Qt::AlignCenter);
    }
    seatLayout->addWidget(seatLabel);
    seatLayout->addWidget(m_seatNumber);
    noticeLayout->addWidget(m_status);
    noticeLayout->addWidget(m_message);
    auto* noticeRow = new QHBoxLayout;
    noticeRow->addStretch();
    noticeRow->addWidget(notice);
    noticeRow->addStretch();
    contentLayout->addStretch();
    contentLayout->addWidget(m_seatDetails);
    contentLayout->addLayout(noticeRow);
    contentLayout->addStretch();
    cardLayout->addLayout(contentLayout, 1);
    adjustLayout();
}

void ExamSeatPage::showSession(State state, const QString& seat, const QString& session)
{
    const bool hasSeat = (state == State::Waiting || state == State::PreparingLogin) && !seat.isEmpty();
    m_seatDetails->setVisible(hasSeat);
    m_seatNumber->setText(hasSeat ? seat : QString());
    const QString sessionName = session.trimmed();
    m_session->setText(sessionName.isEmpty() ? QStringLiteral("待获取") : sessionName);

    QString message;
    switch (state) {
    case State::Waiting:
        m_status->setText(hasSeat ? QStringLiteral("等待开考") : QStringLiteral("座号待分配"));
        break;
    case State::PreparingLogin:
        m_status->setText(QStringLiteral("正在加载登录页"));
        break;
    case State::Unused:
        m_status->setText(QStringLiteral("本场不使用"));
        message = QStringLiteral("请按考试座号到对应机位就座。");
        break;
    case State::Synchronizing:
        m_status->setText(QStringLiteral("正在获取考场状态"));
        message = QStringLiteral("如长时间未更新，请联系监考老师。");
        break;
    }
    m_message->setText(message);
    m_message->setVisible(!message.isEmpty());
    QFont statusFont = m_status->font();
    statusFont.setPixelSize(hasSeat ? StatusWithSeatSize : StatusWithoutSeatSize);
    m_status->setFont(statusFont);
    adjustLayout();
}

void ExamSeatPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    adjustLayout();
}

void ExamSeatPage::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal spacing = qBound<qreal>(16, width() * 0.026, 38);
    for (int line = 0; line < 4; ++line) {
        const qreal offset = line * spacing;
        const qreal leftY = height() * 0.78 + offset;
        const qreal rightY = height() * 0.08 + offset;
        QPainterPath path(QPointF(-1, leftY));
        path.cubicTo(width() * 0.42, leftY, width() * 0.58, rightY, width() + 1, rightY);
        painter.setPen(QPen(QColor(255, 255, 255, 72 - line * 10), 2.0));
        painter.drawPath(path);
    }
}

void ExamSeatPage::adjustLayout()
{
    m_card->setMinimumHeight(qBound(320, height() * 64 / 100, 560));
    const int inset = qBound(16, width() / 20, ContentInset);
    m_card->layout()->setContentsMargins(inset, 0, inset, 0);
    QFont seatFont = m_seatNumber->font();
    const int preferred = qBound(64, qMin(width() / 3, height() / 4), SeatNumberSize);
    seatFont.setPixelSize(preferred);
    const int measure = QFontMetrics(seatFont).horizontalAdvance(m_seatNumber->text());
    const int available = qMax(1, qMin(width() - 2 * PageInset, CardWidth) - 2 * inset);
    if (measure > available) {
        int fitted = qMax(MinimumSeatNumberSize, preferred * available / measure);
        seatFont.setPixelSize(fitted);
        while (fitted > MinimumSeatNumberSize
               && QFontMetrics(seatFont).horizontalAdvance(m_seatNumber->text()) > available) {
            seatFont.setPixelSize(--fitted);
        }
    }
    m_seatNumber->setFont(seatFont);
}

QLabel* ExamSeatPage::createTerminalInfo(QWidget* parent)
{
    auto* label = makeLabel(parent, {}, 13, TerminalInfoText);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    label->setWordWrap(true);
    label->setMinimumHeight(38);
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    label->setStyleSheet(QStringLiteral("background: %1; color: %2; padding: 8px 18px;")
        .arg(QLatin1String(StudentLoginBlue), QLatin1String(TerminalInfoText)));
    return label;
}

QString ExamSeatPage::terminalDetails()
{
    const QHostAddress linkLocal(QStringLiteral("169.254.0.0"));
    QStringList addresses;
    QStringList linkLocalAddresses;
    for (const auto& network : QNetworkInterface::allInterfaces()) {
        const auto flags = network.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        for (const auto& entry : network.addressEntries()) {
            const QHostAddress address = entry.ip();
            if (address.protocol() != QAbstractSocket::IPv4Protocol || address.toIPv4Address() == 0) {
                continue;
            }
            (address.isInSubnet(linkLocal, 16) ? linkLocalAddresses : addresses).append(address.toString());
        }
    }
    if (addresses.isEmpty()) {
        addresses = linkLocalAddresses;
    }
    addresses.removeDuplicates();
    addresses.sort();
    const auto known = [](const QString& value) {
        return value.isEmpty() ? QStringLiteral("未获取") : value;
    };
    return QStringLiteral("版本 %1    本机 IP %2    计算机名 %3")
        .arg(known(QCoreApplication::applicationVersion()), known(addresses.join(QStringLiteral(", "))),
             known(QSysInfo::machineHostName()));
}
