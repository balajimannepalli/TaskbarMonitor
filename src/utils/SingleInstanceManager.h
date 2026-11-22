#pragma once
#include <windows.h>
#include <string>
class SingleInstanceManager {
public:
    explicit SingleInstanceManager(const std::wstring& name = L"Local\\TaskbarMonitor.Foundation");
    ~SingleInstanceManager();
    SingleInstanceManager(const SingleInstanceManager&) = delete;
    SingleInstanceManager& operator=(const SingleInstanceManager&) = delete;
    bool primary() const { return primary_; }
    void notifyExisting(const wchar_t* windowClass) const noexcept;
private:
    HANDLE mutex_{};
    bool primary_{};
};
