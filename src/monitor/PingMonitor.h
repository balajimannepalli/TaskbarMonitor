#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include "Metrics.h"
namespace monitor {
Status pingStatus(DWORD code);
class PingMonitor {
public:
    PingMonitor() = default;
    PingMonitor(const PingMonitor&)=delete;
    ~PingMonitor();
    void start(Snapshot&,const std::wstring&,DWORD timeout);
    void complete(Snapshot&);
    HANDLE pendingEvent() const { return stage_==Stage::Idle ? nullptr : event_; }
private:
    enum class Stage { Idle, Resolve, Echo };
    void send(Snapshot&);
    void resolved(Snapshot&);
    Stage stage_=Stage::Idle;
    bool winsock_{};
    HANDLE icmp_=INVALID_HANDLE_VALUE,event_{},cancel_{};
    OVERLAPPED operation_{};
    ADDRINFOEXW* addresses_{};
    IN_ADDR address_{};
    DWORD timeout_{};
    alignas(ICMP_ECHO_REPLY) unsigned char buffer_[sizeof(ICMP_ECHO_REPLY)+64]{};
    char payload_[4]{'T','B','M','1'};
};
}
