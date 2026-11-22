#include "MetricDisplay.h"
#include <windows.h>
#include <algorithm>

namespace display {
std::optional<Dimensions> dimensions(int count,float fontSize,int spacing,int availableWidth,int availableHeight) {
    if (!count) return {};
    const int rows=count>1 ? 2 : 1;
    const int columns=(count+1)/2;
    const int width=std::min(availableWidth,columns*(static_cast<int>(fontSize*6.5f)+spacing));
    const int height=std::min(availableHeight,static_cast<int>(fontSize*1.5f)*rows+2);
    if (width<columns*54 || height<rows*12) return {};
    return Dimensions{width,height};
}
Cells cells(const monitor::Snapshot& snapshot,const Visibility& visible,const Order& order) {
    Cells result;
    std::array<std::wstring,keys.size()> values;
    auto add = [&](size_t index,const wchar_t* label,const auto& metric) {
        if (!visible[index]) return;
        std::wstring text(label);
        if (metric.status == monitor::Status::Valid && metric.value) {
            const auto value = monitor::format(metric);
            wchar_t converted[96]{};
            if (MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.c_str(),-1,converted,96))
                text += converted;
            else text += L"N/A";
        } else if (metric.status == monitor::Status::Timeout) text += L"Timeout";
        else text += L"N/A";
        values[index]=std::move(text);
    };
    add(0,L"↓ ",snapshot.download);
    add(1,L"↑ ",snapshot.upload);
    add(2,L"CPU ",snapshot.cpu);
    add(3,L"GPU ",snapshot.gpu);
    add(4,L"RAM ",snapshot.ram);
    add(5,L"CPU temp ",snapshot.cpuTemperature);
    add(6,L"GPU temp ",snapshot.gpuTemperature);
    add(7,L"Ping ",snapshot.ping);
    for(auto index:order) if(visible[index]) result.push_back(std::move(values[index]));
    return result;
}
}
