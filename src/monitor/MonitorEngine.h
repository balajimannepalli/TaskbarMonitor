#pragma once
#include "Metrics.h"
#include <windows.h>
#include <thread>
#include <nlohmann/json.hpp>
namespace monitor {
struct Settings {
    bool operator==(const Settings&) const = default;
    std::array<int,JobCount> interval{1000,1000,2000,1000,5000,2000};
    std::array<bool,JobCount> enabled{true,true,true,true,true,true};
    bool download=true, upload=true,cpuTemperature=true,gpuTemperature=true;
    std::wstring target=L"1.1.1.1";
    unsigned timeout=1000;
    static Settings fromConfig(const nlohmann::json&);
};
Clock::time_point nextDeadline(Clock::time_point deadline,Clock::time_point now,std::chrono::milliseconds period);
void markFailure(Snapshot&,Job);
class MonitorEngine {
public:
    explicit MonitorEngine(Settings);
    ~MonitorEngine();
    void configure(Settings);
    MonitorEngine(const MonitorEngine&)=delete;
    Snapshot snapshot() const {return state_.read();}
    const MetricsState& metrics() const {return state_;}
private:
    void run(bool graphics);
    Settings settings_;
    std::mutex settingsMutex_;
    HANDLE changed_[2]{};
    MetricsState state_;
    HANDLE stop_{};
    std::thread primary_,graphics_;
};
}
