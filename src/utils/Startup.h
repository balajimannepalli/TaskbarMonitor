#pragma once
#include <windows.h>
namespace startup {
inline constexpr wchar_t runKey[]=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
bool enabled(const wchar_t* key=runKey);
void set(bool enable,const wchar_t* key=runKey);
}
