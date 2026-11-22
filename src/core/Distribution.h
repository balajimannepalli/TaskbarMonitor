#pragma once

namespace distribution {
#ifdef TASKBAR_PORTABLE
inline constexpr bool portable = true;
inline constexpr wchar_t windowClass[] = L"TaskbarMonitor.Portable";
inline constexpr wchar_t instanceName[] = L"Local\\TaskbarMonitor.Portable";
#else
inline constexpr bool portable = false;
inline constexpr wchar_t windowClass[] = L"TaskbarMonitor.Phase0";
inline constexpr wchar_t instanceName[] = L"Local\\TaskbarMonitor.Foundation";
#endif
}
