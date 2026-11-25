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

constexpr int PageInset = 32;
constexpr int CardWidth = 776;
constexpr int ContentInset = 30;
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
        "QFrame#examSeatCard { background: #ffffff; border-radius: 22px; }"
        "QFrame#examSeatSessionMark { background: %1; border-radius: 1px; }"
        "QFrame#examSeatDivider { background: #e9eef5; }"
        "QFrame#examSeatNotice { background: #eff5ff; border-radius: 10px; }")
        .arg(QLatin1String(StudentLoginBlue)));
    auto* pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(PageInset, 16, PageInset, 16);
    pageLayout->addStretch();

    m_card = new QFrame(this);
    m_card->setObjectName(QStringLiteral("examSeatCard"));
    m_card->setMaximumWidth(CardWidth);
    auto* shadow = new QGraphicsDropShadowEffect(m_card);
    shadow->setColor(QColor(19, 52, 111, 52));
    shadow->setBlurRadius(48);
    shadow->setOffset(0, 14);
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
    sessionLayout->setContentsMargins(0, 20, 0, 20);
    sessionLayout->setSpacing(12);
    auto* sessionMark = new QFrame(m_card);
    sessionMark->setObjectName(QStringLiteral("examSeatSessionMark"));
    sessionMark->setFixedSize(3, 17);
    sessionLayout->addWidget(sessionMark);
    auto* sessionLabel = makeLabel(m_card, QStringLiteral("考试场次"), 14, "#52637a");
    sessionLayout->addWidget(sessionLabel);
    m_session = makeLabel(m_card, {}, 21, "#182943", true);
    m_session->setWordWrap(true);
    m_session->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    sessionLayout->addWidget(m_session, 1);
    cardLayout->addLayout(sessionLayout);
    auto* divider = new QFrame(m_card);
    divider->setObjectName(QStringLiteral("examSeatDivider"));
    divider->setFixedHeight(1);
    cardLayout->addWidget(divider);

    auto* contentLayout = new QVBoxLayout;
    contentLayout->setContentsMargins(0, 16, 0, 24);
    contentLayout->setSpacing(8);
    m_seatDetails = new QWidget(m_card);
    auto* seatLayout = new QVBoxLayout(m_seatDetails);
    seatLayout->setContentsMargins(0, 0, 0, 0);
    seatLayout->setSpacing(0);
    auto* seatLabel = makeLabel(m_seatDetails, QStringLiteral("考试座号"), 16, "#52637a");
    m_seatNumber = makeLabel(m_seatDetails, {}, 128, "#10294d", true);
    m_seatNumber->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    auto* notice = new QFrame(m_card);
    notice->setObjectName(QStringLiteral("examSeatNotice"));
    auto* noticeLayout = new QVBoxLayout(notice);
    noticeLayout->setContentsMargins(12, 11, 12, 11);
    noticeLayout->setSpacing(4);
    m_status = makeLabel(notice, {}, 18, "#234e86", true);
    m_status->setWordWrap(true);
    m_message = makeLabel(notice, {}, 14, "#52637a");
    m_message->setWordWrap(true);
    for (auto* label : {seatLabel, m_seatNumber, m_status, m_message}) {
        label->setAlignment(Qt::AlignCenter);
    }
    seatLayout->addStretch();
    seatLayout->addWidget(seatLabel);
    seatLayout->addWidget(m_seatNumber);
    seatLayout->addStretch();
    noticeLayout->addWidget(m_status);
    noticeLayout->addWidget(m_message);
    contentLayout->addWidget(m_seatDetails, 1);
    contentLayout->addWidget(notice, 0, Qt::AlignVCenter);
    cardLayout->addLayout(contentLayout, 1);
    adjustLayout();
}

void ExamSeatPage::showSession(State state, const QString& seat, const QString& session)
{
    m_hasSeat = state == State::Waiting && !seat.isEmpty();
    m_seatDetails->setVisible(m_hasSeat);
    m_seatNumber->setText(m_hasSeat ? seat : QString());
    const QString sessionName = session.trimmed();
    m_session->setText(sessionName.isEmpty() ? QStringLiteral("待获取") : sessionName);

    QString message;
    switch (state) {
    case State::Waiting:
        m_status->setText(m_hasSeat ? QStringLiteral("等待开考") : QStringLiteral("座号待分配"));
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
    const qreal spacing = qBound<qreal>(12, width() * 0.015, 22);
    for (int line = 0; line < 4; ++line) {
        const qreal offset = line * spacing;
        QPainterPath path(QPointF(-1, height() * 0.71 + offset));
        path.cubicTo(width() * 0.42, height() * 0.71 + offset, width() * 0.58,
                     height() * 0.13 + offset, width() + 1, height() * 0.13 + offset);
        painter.setPen(QPen(QColor(255, 255, 255, 96 - line * 13), 1.4));
        painter.drawPath(path);
    }
}

void ExamSeatPage::adjustLayout()
{
    m_card->setMinimumHeight(qBound(320, height() * 55 / 100, 434));
    QFont seatFont = m_seatNumber->font();
    const int preferred = qBound(64, height() / 5, 128);
    seatFont.setPixelSize(preferred);
    const int measure = QFontMetrics(seatFont).horizontalAdvance(m_seatNumber->text());
    const int available = qMax(1, qMin(width() - 2 * PageInset, CardWidth) - 2 * ContentInset);
    if (measure > available) {
        seatFont.setPixelSize(qMax(12, preferred * available / measure));
    }
    m_seatNumber->setFont(seatFont);

    QFont statusFont = m_status->font();
    statusFont.setPixelSize(m_hasSeat ? 18 : 26);
    m_status->setFont(statusFont);
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
