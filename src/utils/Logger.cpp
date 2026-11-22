#include "Logger.h"
void logMessage(LogLevel level, const wchar_t* message) noexcept {
#ifdef NDEBUG
    if (level == LogLevel::Info) return;
#endif
    const wchar_t* prefix = level == LogLevel::Info ? L"INFO " :
        level == LogLevel::Warning ? L"WARNING " : L"ERROR ";
    OutputDebugStringW(prefix);
    OutputDebugStringW(message);
    OutputDebugStringW(L"\n");
}
