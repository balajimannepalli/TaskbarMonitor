#include "ui/MetricDisplay.h"
#include "ui/Renderer.h"
#include "taskbar/Placement.h"
#include <iostream>
int main() {
    Renderer renderer;
    if (FAILED(renderer.initialize()) ||
        renderer.setCells({L"CPU 0%",L"GPU N/A"},8)!=S_OK ||
        renderer.setCells({L"CPU 1%",L"GPU N/A"},8)!=S_OK ||
        renderer.setCells({L"CPU 1%",L"GPU N/A"},8)!=S_FALSE ||
        renderer.setCells({L"CPU 1%"},4)!=S_OK ||
        renderer.layoutsCreated()!=3) return 10;
    display::Palette custom{0x123456,0xABCDEF};
    if(display::palette("light",false,custom).background!=0xf3f3f3 ||
       display::palette("dark",true,custom).background!=0x252525 ||
       display::palette("system",true,custom).text!=0x181818 ||
       display::palette("system",false,custom).text!=0xf3f3f3 ||
       display::palette("custom",true,custom).text!=custom.text) return 9;
    monitor::Snapshot s;
    display::Visibility visible;
    visible.fill(true);
    s.cpu.set(0); s.cpuTemperature.miss(monitor::Status::Unsupported);
    s.download.set(1200); s.ping.miss(monitor::Status::Timeout);
    const auto first=display::cells(s,visible);
    if(first!=display::Cells{L"↓ 1.2 KB/s",L"↑ N/A",L"CPU 0%",L"GPU N/A",L"RAM N/A",
        L"CPU temp N/A",L"GPU temp N/A",L"Ping Timeout"}) return 1;
    s.cpu.set(0.1);
    auto order=display::defaultOrder;
    std::swap(order[0],order[7]);
    if(display::cells(s,visible,order).front()!=L"Ping Timeout") return 8;
    if(display::cells(s,visible)!=first) return 2;
    s.cpu.set(1);
    if(display::cells(s,visible)==first) return 3;
    visible.fill(false); visible[6]=true;
    s.gpuTemperature.set(42.5);
    if(display::cells(s,visible)!=display::Cells{L"GPU temp 42.5 °C"}) return 4;
    RECT bar{0,0,350,60};
    auto narrow=display::dimensions(8,12.f,8,220,30);
    if(!narrow || narrow->width!=220 || narrow->height!=30 ||
       display::dimensions(8,12.f,8,160,30) || display::dimensions(0,12.f,8,300,30)) return 7;
    if(prototype::placement(bar,bar,144,0,false,344,38)) return 5;
    auto scaled=prototype::placement(bar,bar,144,0,false,220,38);
    return scaled && scaled->right==330 && scaled->bottom-scaled->top==57 ? 0 : 6;
}
