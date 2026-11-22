#pragma once
#include "Metrics.h"
#include <windows.h>
#include <pdh.h>
#include <map>
#include <vector>
namespace monitor {
struct EngineId {
    uint64_t luid;
    unsigned physical, engine;
    auto operator<=>(const EngineId&) const = default;
};
std::optional<EngineId> parseEngine(const wchar_t*);
using Engines = std::map<EngineId,double>;
void selectBusiest(Snapshot&,const Engines&);
class GpuMonitor {
public:
    GpuMonitor() = default;
    GpuMonitor(const GpuMonitor&) = delete;
    ~GpuMonitor();
    void sample(Snapshot&);
private:
    void reset();
    PDH_HQUERY query_{};
    PDH_HCOUNTER counter_{};
    Clock::time_point retry_{};
    std::vector<PDH_FMT_COUNTERVALUE_ITEM_W> storage_;
    bool warmed_{};
};
void sampleTemperature(Snapshot&);
}
