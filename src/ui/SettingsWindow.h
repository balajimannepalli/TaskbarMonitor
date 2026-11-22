#pragma once
#include "config/ConfigManager.h"
#include <windows.h>
bool editSettings(HWND owner, ConfigManager&, nlohmann::json& settings);
