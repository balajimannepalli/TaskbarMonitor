#include "PingMonitor.h"
#include "MonitorEngine.h"
#include "SystemCollectors.h"
#include "GpuMonitor.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
namespace monitor {
using namespace std::chrono;
Settings Settings::fromConfig(const nlohmann::json& j) {
    Settings s;
    constexpr const char* keys[]{"network","cpu","ram","gpu","temperature","ping"};
    for (size_t i=0;i<JobCount;++i) s.interval[i]=j.at("refreshIntervals").at(keys[i]).get<int>();
    const auto& e=j.at("enabledMetrics");
    s.download=e.at("download"); s.upload=e.at("upload");
    s.cpuTemperature=e.at("cpuTemperature"); s.gpuTemperature=e.at("gpuTemperature");
    s.enabled={s.download||s.upload,e.at("cpu"),e.at("ram"),e.at("gpu"),
               s.cpuTemperature||s.gpuTemperature,e.at("ping")};
    const auto t=j.at("pingTarget").get<std::string>();
    s.target=std::filesystem::path(std::u8string(t.begin(),t.end())).wstring();
    s.timeout=j.at("pingTimeoutMs").get<unsigned>();
    return s;
}
Clock::time_point nextDeadline(Clock::time_point due,Clock::time_point now,milliseconds period) {
    return due+period*((now-due)/period+1);
}
void markFailure(Snapshot& s,Job j) {
    switch(j) {
    case Network:s.download.miss(Status::Error);s.upload.miss(Status::Error);break;
    case Cpu:s.cpu.miss(Status::Error);break;
    case Ram:s.ram.miss(Status::Error);break;
    case Gpu:s.gpu.miss(Status::Error);break;
    case Temperature:s.cpuTemperature.miss(Status::Error);s.gpuTemperature.miss(Status::Error);break;
    case Ping:s.ping.miss(Status::Error);break;
    default:break;
    }
}
MonitorEngine::MonitorEngine(Settings settings):settings_(std::move(settings)) {
    for (int period:settings_.interval) if (period<=0) throw std::invalid_argument("Invalid interval");
    stop_=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if (!stop_) throw std::runtime_error("Monitoring stop event unavailable");
    try {
        for(auto& event:changed_) {
            event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
            if(!event) throw std::runtime_error("Cannot create scheduler event");
        }
        graphics_=std::thread(&MonitorEngine::run,this,true);
        primary_=std::thread(&MonitorEngine::run,this,false);
    } catch (...) {
        SetEvent(stop_); if (graphics_.joinable()) graphics_.join(); for(auto event:changed_) if(event) CloseHandle(event); CloseHandle(stop_); throw;
    }
}
MonitorEngine::~MonitorEngine() {
    SetEvent(stop_);
    if (primary_.joinable()) primary_.join();
    if (graphics_.joinable()) graphics_.join();
    for(auto event:changed_) CloseHandle(event);
    CloseHandle(stop_);
}
void MonitorEngine::configure(Settings next) {
    for(int period:next.interval) if(period<250 || period>3600000) throw std::invalid_argument("Invalid interval");
    std::lock_guard lock(settingsMutex_);
    if(next==settings_) return;
    settings_=std::move(next);
    for(auto event:changed_) SetEvent(event);
}
void MonitorEngine::run(bool graphics) {
    NetworkMonitor network;
    CpuMonitor cpu;
    GpuMonitor gpu;
    PingMonitor ping;
    Snapshot local;
    std::array<Clock::time_point,JobCount> deadlines;
    Settings active;
    bool initialize=true;
    bool changed=true;
    deadlines.fill(Clock::time_point::max());
    while (WaitForSingleObject(stop_,0)==WAIT_TIMEOUT) {
        if(changed) {
            std::lock_guard lock(settingsMutex_);
            for(size_t i=0;i<JobCount;++i) {
                if(graphics!=(i==Gpu || i==Temperature)) continue;
                if(initialize || active.enabled[i]!=settings_.enabled[i] || active.interval[i]!=settings_.interval[i])
                    deadlines[i]=settings_.enabled[i] ? Clock::now() : Clock::time_point::max();
            }
            active=settings_;
            initialize=false;
            changed=false;
        }
        const auto wake=Clock::now();
        auto work=Clock::duration::zero();
        if (ping.pendingEvent() && WaitForSingleObject(ping.pendingEvent(),0)==WAIT_OBJECT_0) {
            const auto start=Clock::now();
            ping.complete(local);
            const auto duration=Clock::now()-start;
            local.elapsedUs[Ping]+=duration_cast<microseconds>(duration).count();
            work+=duration;
        }
        for (size_t i=0;i<JobCount;++i) {
            const auto start=Clock::now();
            if (start<deadlines[i]) continue;
            const Job job=static_cast<Job>(i);
            try {
                switch(job) {
                case Network:
                    network.sample(local);
                    if (!active.download) local.download.miss(Status::Unavailable);
                    if (!active.upload) local.upload.miss(Status::Unavailable);
                    break;
                case Cpu:cpu.sample(local);break;
                case Ram:RamMonitor::sample(local);break;
                case Gpu: {
                    const auto priorLuid=local.gpuLuid;
                    const auto priorPhysical=local.physicalGpu;
                    gpu.sample(local);
                    if (active.gpuTemperature &&
                        (local.gpuLuid!=priorLuid || local.physicalGpu!=priorPhysical))
                        local.gpuTemperature.miss(Status::Unavailable);
                    break;
                }
                case Temperature:
                    if (active.gpuTemperature) sampleTemperature(local);
                    if (active.cpuTemperature) local.cpuTemperature.miss(Status::Unsupported);
                    else local.cpuTemperature.miss(Status::Pending);
                    break;
                case Ping:ping.start(local,active.target,active.timeout);break;
                default:break;
                }
            } catch (...) {markFailure(local,job);}
            const auto finish=Clock::now();
            work+=finish-start;
            ++local.calls[i];
            local.elapsedUs[i]+=duration_cast<microseconds>(finish-start).count();
            deadlines[i]=nextDeadline(deadlines[i],finish,milliseconds(active.interval[i]));
        }
        state_.merge(local,graphics);
        local.schedulerUs+=duration_cast<microseconds>(Clock::now()-wake-work).count();
        ++local.wakes;
        const auto next=*std::min_element(deadlines.begin(),deadlines.end());
        const auto wait=next==Clock::time_point::max() ? INFINITE : std::max<int64_t>(0,ceil<milliseconds>(next-Clock::now()).count());
        HANDLE events[]{stop_,changed_[graphics],ping.pendingEvent()};
        const DWORD ready=WaitForMultipleObjects(events[2]?3:2,events,FALSE,static_cast<DWORD>(wait));
        if(ready==WAIT_FAILED) break;
        changed=ready==WAIT_OBJECT_0+1;
    }
}
}

