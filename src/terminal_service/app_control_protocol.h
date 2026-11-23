#pragma once

namespace terminal {

inline constexpr wchar_t AppMutexName[] = L"Global\\UMTerminal.CEF.SingleInstance";
inline constexpr wchar_t ControlPipeName[] = L"\\\\.\\pipe\\UMTerminal.CEF.Control";
inline constexpr char StopCommand[] = "STOP\n";
inline constexpr char StopAccepted[] = "OK\n";
inline constexpr char StopRejected[] = "ERR\n";

} // namespace terminal
