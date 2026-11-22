#pragma once
#include <windows.h>

enum class LogLevel { Info, Warning, Error };
void logMessage(LogLevel level, const wchar_t* message) noexcept;
