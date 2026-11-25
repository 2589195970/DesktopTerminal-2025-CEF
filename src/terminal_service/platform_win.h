#pragma once

#include "terminal_core.h"

#include <functional>

namespace terminal {

using WaitForStop = std::function<bool(int milliseconds)>;

inline QString windowsFailure(const QString &message, unsigned long error)
{
    return QStringLiteral("%1（Windows 错误 %2）").arg(message).arg(error);
}

bool appIsRunning();
void publishExamSeatDisplay(const QJsonObject &display, const WaitForStop &waitForStop);
NetworkIdentity findNetworkIdentity(const QList<QHostAddress> &serverAddresses);
CommandResult executeAppOrWake(const Command &command, const Config &config, const NetworkIdentity &network,
                               const WaitForStop &waitForStop);
unsigned long initiatePowerChange(bool reboot);

} // namespace terminal
