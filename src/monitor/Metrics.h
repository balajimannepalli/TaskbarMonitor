#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace monitor {
using Clock = std::chrono::steady_clock;
enum class Status { Pending, Valid, Unavailable, Unsupported, Error, Timeout, Cancelled };
enum class Unit { BytesPerSecond, Percent, Celsius, Milliseconds };
enum Job : size_t { Network,Cpu,Ram,Gpu,Temperature,Ping,JobCount };
template<Unit U> struct Metric {
    static constexpr Unit unit = U;
    Status status = Status::Pending;
    std::optional<double> value;
    Clock::time_point lastSuccess{};
    void set(double reading) {
        value = reading;
        status = Status::Valid;
        lastSuccess = Clock::now();
    }
    void miss(Status reason) { value.reset(); status = reason; }
};
struct Snapshot {
    Metric<Unit::BytesPerSecond> download, upload;
    Metric<Unit::Percent> cpu, ram, gpu;
    Metric<Unit::Celsius> cpuTemperature, gpuTemperature;
    Metric<Unit::Milliseconds> ping;
    uint64_t totalBytes{}, availableBytes{}, usedBytes{};
    unsigned memoryLoad{}, interfaces{}, engines{};
    uint64_t gpuLuid{};
    unsigned physicalGpu{};
    std::array<uint64_t,JobCount> calls{}, elapsedUs{};
    uint64_t schedulerUs{}, wakes{};
};
class MetricsState {
public:
    Snapshot read() const { std::lock_guard lock(mutex_); return latest_; }
    bool tryRead(Snapshot& out) const {
        std::unique_lock lock(mutex_,std::try_to_lock);
        if (!lock) return false;
        out=latest_;
        return true;
    }
    void merge(const Snapshot& value,bool graphics);
private:
    mutable std::mutex mutex_;
    Snapshot latest_;
    std::array<uint64_t,2> overhead_{}, wakes_{};
};
std::string formatValue(Unit, Status, std::optional<double>);
template<Unit U> std::string format(const Metric<U>& m) { return formatValue(U,m.status,m.value); }
}
