#ifndef SEAT_DISPLAY_STORE_H
#define SEAT_DISPLAY_STORE_H

#include <QString>

namespace terminal {

struct SeatDisplay
{
    QString assigned;
    QString manual;

    QString number() const;
};

class SeatDisplayStore
{
public:
    explicit SeatDisplayStore(const QString &directory);

    static QString dataDirectory();
    static bool isValidNumber(const QString &number);
    bool read(SeatDisplay &seat, QString &error) const;
    bool setManual(const QString &number, QString &error) const;
    bool applyAssignment(const QString &number, QString &error) const;
    bool clearAssignment(QString &error) const;

private:
    enum class Update { Manual, Backend, Unbind };
    bool update(Update type, const QString &number, QString &error) const;

    QString m_path;
};

} // namespace terminal

#endif
