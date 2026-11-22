#pragma once
#include "monitor/Metrics.h"
#include <array>
#include <vector>

namespace display {
inline constexpr std::array keys{"download","upload","cpu","gpu","ram","cpuTemperature","gpuTemperature","ping"};
using Visibility = std::array<bool,keys.size()>;
using Cells = std::vector<std::wstring>;
struct Palette { unsigned text,background; };
inline Palette palette(const std::string& theme,bool systemLight,Palette custom) {
    if(theme=="custom") return custom;
    if(theme=="light" || (theme=="system" && systemLight)) return {0x181818,0xf3f3f3};
    return {0xf3f3f3,0x252525};
}
using Order=std::array<size_t,keys.size()>;
inline constexpr Order defaultOrder{0,1,2,3,4,5,6,7};
Cells cells(const monitor::Snapshot&, const Visibility&,const Order& order=defaultOrder);
struct Dimensions { int width, height; };
std::optional<Dimensions> dimensions(int count,float fontSize,int spacing,int availableWidth,int availableHeight);
}
