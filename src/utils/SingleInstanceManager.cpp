#include "SingleInstanceManager.h"
#include <stdexcept>
SingleInstanceManager::SingleInstanceManager(const std::wstring& name) {
    mutex_ = CreateMutexW(nullptr, FALSE, name.c_str());
    if (!mutex_) throw std::runtime_error("Instance synchronization failed");
    const DWORD result = WaitForSingleObject(mutex_, 0);
    primary_ = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
    if (result == WAIT_FAILED) {
        CloseHandle(mutex_);
        mutex_ = nullptr;
        throw std::runtime_error("Instance lock failed");
    }
}
SingleInstanceManager::~SingleInstanceManager() {
    if (primary_) ReleaseMutex(mutex_);
    if (mutex_) CloseHandle(mutex_);
}
void SingleInstanceManager::notifyExisting(const wchar_t* windowClass) const noexcept {
    // Only request geometry recovery. The overlay must preserve keyboard focus.
    if (const HWND window = FindWindowW(windowClass, nullptr))
        PostMessageW(window, WM_APP + 1, 0, 0);
}
