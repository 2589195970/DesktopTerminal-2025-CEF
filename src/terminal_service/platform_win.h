#pragma once

#include "terminal_core.h"

#include <functional>

namespace terminal {

using WaitForStop = std::function<bool(int milliseconds)>;

bool appIsRunning();
NetworkIdentity findNetworkIdentity(const QList<QHostAddress> &serverAddresses);
CommandResult executeAppOrWake(const Command &command, const Config &config, const NetworkIdentity &network,
                               const WaitForStop &waitForStop);
unsigned long initiatePowerChange(bool reboot);

} // namespace terminal
