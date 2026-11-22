#include "Metrics.h"
#include <format>
namespace monitor {
void MetricsState::merge(const Snapshot& s,bool graphics) {
    std::lock_guard lock(mutex_);
    auto& out=latest_;
    if (graphics) {
        out.gpu=s.gpu; out.cpuTemperature=s.cpuTemperature; out.gpuTemperature=s.gpuTemperature;
        out.engines=s.engines; out.gpuLuid=s.gpuLuid; out.physicalGpu=s.physicalGpu;
    } else {
        out.download=s.download; out.upload=s.upload; out.interfaces=s.interfaces; out.cpu=s.cpu;
        out.ram=s.ram; out.totalBytes=s.totalBytes; out.usedBytes=s.usedBytes;
        out.availableBytes=s.availableBytes; out.memoryLoad=s.memoryLoad; out.ping=s.ping;
    }
    for (size_t i=0;i<s.calls.size();++i) {
        if (graphics==(i==Gpu || i==Temperature)) {
            out.calls[i]=s.calls[i];
            out.elapsedUs[i]=s.elapsedUs[i];
        }
    }
    overhead_[graphics]=s.schedulerUs; wakes_[graphics]=s.wakes;
    out.schedulerUs=overhead_[0]+overhead_[1]; out.wakes=wakes_[0]+wakes_[1];
}
std::string formatValue(Unit unit, Status status, std::optional<double> value) {
    if (!value || status != Status::Valid) {
        constexpr const char* states[]{"pending","N/A","unavailable","unsupported","error","timeout","cancelled"};
        return states[static_cast<unsigned>(status)];
    }
    double v = *value;
    if (unit == Unit::BytesPerSecond) {
        constexpr const char* units[]{"B/s","KB/s","MB/s","GB/s"};
        unsigned scale = 0;
        while (v >= 1000 && scale < 3) { v /= 1000; ++scale; }
        return std::format("{:.{}f} {}",v,scale && v < 100 ? 1 : 0,units[scale]);
    }
    if (unit == Unit::Milliseconds) return std::format("{:.0f} ms",v);
    if (unit == Unit::Celsius) return std::format("{:.1f} °C",v);
    return std::format("{:.0f}%",v);
}
}
