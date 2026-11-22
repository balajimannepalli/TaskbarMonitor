#pragma once
#include <windows.h>
#include <string>
#include <functional>
#include "ui/MetricDisplay.h"
struct OverlayOptions {
    std::wstring monitor;
    int inset = 352;
    bool right = false;
    bool centered = false;
    float fontSize = 12.f;
    display::Visibility visible{};
    int spacing = 8;
    display::Order order=display::defaultOrder;
    std::wstring family=L"Segoe UI";
    std::string theme="dark";
    unsigned foreground=0xf3f3f3,background=0x252525;
    int padding=2,verticalOffset=0;
    bool compact=false;
    bool transparentBackground=false;
};
int runOverlay(HINSTANCE instance, OverlayOptions& options,const monitor::MetricsState* metrics,
               std::function<bool(HWND)> edit,bool openSettings);
