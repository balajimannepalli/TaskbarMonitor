#include "SystemCollectors.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <algorithm>
#include <memory>
#include <utility>

namespace monitor {
void NetworkMonitor::sample(Snapshot& s) {
    PMIB_IF_TABLE2 raw{};
    if (GetIfTable2(&raw) != NO_ERROR) {
        previous_.clear();
        s.interfaces = 0;
        s.download.miss(Status::Error); s.upload.miss(Status::Error);
        return;
    }
    std::unique_ptr<MIB_IF_TABLE2,decltype(&FreeMibTable)> table(raw,FreeMibTable);
    current_.clear();
    for (ULONG i=0;i<table->NumEntries;++i) {
        const auto& r = table->Table[i];
        const auto flags = r.InterfaceAndOperStatusFlags;
        if ((r.Type == IF_TYPE_ETHERNET_CSMACD || r.Type == IF_TYPE_IEEE80211) &&
            r.OperStatus == IfOperStatusUp && flags.HardwareInterface && !flags.FilterInterface &&
            !flags.EndPointInterface && !flags.NotMediaConnected && r.TunnelType == TUNNEL_TYPE_NONE)
            current_.push_back({r.InterfaceLuid.Value,r.InOctets,r.OutOctets});
    }
    update(s,current_,Clock::now());
}
void NetworkMonitor::update(Snapshot& s,const std::vector<Traffic>& rows, Clock::time_point now) {
    const double seconds = std::chrono::duration<double>(now-previousTime_).count();
    double received{},sent{};
    bool comparable = false;
    for (const auto& row : rows) {
        const auto old = std::find_if(previous_.begin(),previous_.end(),
            [&](const Traffic& counter){return counter.luid == row.luid;});
        if (old == previous_.end() || row.received < old->received || row.sent < old->sent) continue;
        received += static_cast<double>(row.received-old->received);
        sent += static_cast<double>(row.sent-old->sent);
        comparable = true;
    }
    s.interfaces = static_cast<unsigned>(rows.size());
    if (seconds > 0 && comparable) {
        s.download.set(received/seconds); s.upload.set(sent/seconds);
    } else {
        const auto reason = previousTime_ == Clock::time_point{} && !rows.empty() ? Status::Pending : Status::Unavailable;
        s.download.miss(reason); s.upload.miss(reason);
    }
    previous_ = rows;
    previousTime_ = now;
}
void CpuMonitor::sample(Snapshot& s) {
    FILETIME idle,kernel,user;
    if (!GetSystemTimes(&idle,&kernel,&user)) {
        previous_.reset(); s.cpu.miss(Status::Error); return;
    }
    auto ticks = [](FILETIME t){return (uint64_t(t.dwHighDateTime)<<32) | t.dwLowDateTime;};
    update(s,{ticks(idle),ticks(kernel),ticks(user)});
}
void CpuMonitor::update(Snapshot& s,CpuTimes t) {
    auto old = std::exchange(previous_,t);
    if (!old) { s.cpu.miss(Status::Pending); return; }
    s.cpu.miss(Status::Unavailable);
    if (t.kernel < old->kernel || t.user < old->user || t.idle < old->idle) return;
    const auto k=t.kernel-old->kernel,u=t.user-old->user,idle=t.idle-old->idle;
    if (idle > k || u > UINT64_MAX-k || k+u == 0) return;
    s.cpu.set(100.0*static_cast<double>(k+u-idle)/static_cast<double>(k+u));
}
void RamMonitor::sample(Snapshot& s) {
    MEMORYSTATUSEX m{};
    m.dwLength=sizeof(m);
    if (!GlobalMemoryStatusEx(&m)) { update(s,0,0,0); return; }
    update(s,m.ullTotalPhys,m.ullAvailPhys,m.dwMemoryLoad);
}
void RamMonitor::update(Snapshot& s,uint64_t total,uint64_t available,unsigned load) {
    if (!total || available>total || load>100) {
        s.ram.miss(Status::Error);
        s.totalBytes=s.availableBytes=s.usedBytes=0;
        s.memoryLoad=0; return;
    }
    s.totalBytes=total; s.availableBytes=available; s.usedBytes=total-available;
    s.memoryLoad=load;
    s.ram.set(100.0*static_cast<double>(s.usedBytes)/static_cast<double>(total));
}
}
