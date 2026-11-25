#include "seat_display_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>

namespace terminal {

QString SeatDisplay::number() const
{
    return manual.isEmpty() ? assigned : manual;
}

SeatDisplayStore::SeatDisplayStore(const QString &directory)
    : m_path(directory.isEmpty() ? QString() : QDir(directory).filePath(QStringLiteral("seat-display.json")))
{
}

QString SeatDisplayStore::dataDirectory()
{
#ifdef Q_OS_WIN
    const QString programData = qEnvironmentVariable("ProgramData");
    return QDir::isAbsolutePath(programData) ? QDir(programData).filePath(QStringLiteral("UMTerminal")) : QString();
#else
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
#endif
}

bool SeatDisplayStore::isValidNumber(const QString &number)
{
    if (number.isEmpty() || number.size() > 255 || number != number.trimmed()) {
        return false;
    }
    for (QChar character : number) {
        if (!character.isPrint() || character.category() == QChar::Other_Format) {
            return false;
        }
    }
    return true;
}

bool SeatDisplayStore::read(SeatDisplay &seat, QString &error) const
{
    seat = {};
    if (m_path.isEmpty()) {
        error = QStringLiteral("本机座号目录不可用。");
        return false;
    }

    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (!file.exists()) {
            return true;
        }
        error = QStringLiteral("无法读取本机座号。");
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.read(8193));
    if (!document.isObject() || !file.atEnd()) {
        error = QStringLiteral("本机座号文件损坏。");
        return false;
    }

    const QJsonObject object = document.object();
    seat = {object.value(QStringLiteral("assigned")).toString(), object.value(QStringLiteral("manual")).toString()};
    if ((!seat.assigned.isEmpty() && !isValidNumber(seat.assigned))
        || (!seat.manual.isEmpty() && !isValidNumber(seat.manual))) {
        error = QStringLiteral("本机座号文件含有无效数据。");
        seat = {};
        return false;
    }
    return true;
}

bool SeatDisplayStore::setManual(const QString &number, QString &error) const
{
    if (!number.isEmpty() && !isValidNumber(number)) {
        error = QStringLiteral("请输入有效座号。");
        return false;
    }
    return update(Update::Manual, number, error);
}

bool SeatDisplayStore::applyAssignment(const QString &number, QString &error) const
{
    if (!number.isEmpty() && !isValidNumber(number)) {
        error = QStringLiteral("后台座号无效。");
        return false;
    }
    return update(Update::Backend, number, error);
}

bool SeatDisplayStore::clearAssignment(QString &error) const
{
    return update(Update::Unbind, {}, error);
}

bool SeatDisplayStore::update(Update type, const QString &number, QString &error) const
{
    if (m_path.isEmpty()) {
        error = QStringLiteral("本机座号目录不可用。");
        return false;
    }
#ifndef Q_OS_WIN
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath())) {
        error = QStringLiteral("无法创建本机座号目录。");
        return false;
    }
#endif
    QLockFile lock(m_path + QStringLiteral(".lock"));
    if (!lock.tryLock(200)) {
        error = QStringLiteral("无法写入本机座号。请检查终端服务或稍后重试。");
        return false;
    }

    SeatDisplay seat;
    if (!read(seat, error)) {
        return false;
    }
    const SeatDisplay previous = seat;
    switch (type) {
    case Update::Manual:
        seat.manual = number;
        break;
    case Update::Backend:
        seat.assigned = number;
        if (!number.isEmpty()) {
            seat.manual.clear();
        }
        break;
    case Update::Unbind:
        seat = {};
        break;
    }
    if (seat.assigned == previous.assigned && seat.manual == previous.manual) {
        return true;
    }

    QSaveFile file(m_path);
    const auto bytes = QJsonDocument(QJsonObject{
        {QStringLiteral("assigned"), seat.assigned},
        {QStringLiteral("manual"), seat.manual}
    }).toJson(QJsonDocument::Compact);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        error = QStringLiteral("无法保存本机座号。请稍后重试。");
        return false;
    }
    return true;
}

} // namespace terminal
