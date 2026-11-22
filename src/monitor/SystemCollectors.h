#pragma once
#include "Metrics.h"
#include <vector>
namespace monitor {
struct Traffic { uint64_t luid, received, sent; };
class NetworkMonitor {
public:
    void sample(Snapshot&);
    void update(Snapshot&, const std::vector<Traffic>&, Clock::time_point);
private:
    std::vector<Traffic> previous_, current_;
    Clock::time_point previousTime_{};
};
struct CpuTimes { uint64_t idle,kernel,user; };
class CpuMonitor {
public:
    void sample(Snapshot&);
    void update(Snapshot&,CpuTimes);
private:
    std::optional<CpuTimes> previous_;
};
class RamMonitor {
public:
    static void sample(Snapshot&);
    static void update(Snapshot&,uint64_t total,uint64_t available,unsigned load);
};
}
