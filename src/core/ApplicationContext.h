#pragma once
#include "Distribution.h"
#include "../config/ConfigManager.h"
#include "../utils/SingleInstanceManager.h"
#include "../taskbar/OverlayWindow.h"
struct ApplicationContext {
    SingleInstanceManager instance{distribution::instanceName};
    ConfigManager config{distribution::portable ? startupPath() : ConfigManager::defaultPath()};
    OverlayOptions overlay;
private:
    static std::filesystem::path startupPath() {
        std::wstring path(32768,L'\0');
        const DWORD size=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
        if (!size || size>=path.size()) throw std::runtime_error("Portable path unavailable");
        path.resize(size);
        return std::filesystem::path(path).parent_path() / L"Data" / L"config.json";
    }
};
