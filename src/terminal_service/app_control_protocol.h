#pragma once

namespace terminal {

inline constexpr wchar_t AppMutexName[] = L"Global\\UMTerminal.CEF.SingleInstance";
inline constexpr wchar_t ControlPipeName[] = L"\\\\.\\pipe\\UMTerminal.CEF.Control";
inline constexpr char StopCommand[] = "STOP\n";
inline constexpr char StopAccepted[] = "OK\n";
inline constexpr char StopRejected[] = "ERR\n";
inline constexpr char ExamSeatPrefix[] = "SEAT ";
inline constexpr int MaxControlMessageBytes = 1024;

} // namespace terminal
