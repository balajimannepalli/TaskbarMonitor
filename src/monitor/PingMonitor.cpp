#include "PingMonitor.h"
namespace monitor {
Status pingStatus(DWORD code) {
    switch(code) {
    case IP_SUCCESS:return Status::Valid;
    case IP_REQ_TIMED_OUT:return Status::Timeout;
    case ERROR_OPERATION_ABORTED:return Status::Cancelled;
    case IP_DEST_HOST_UNREACHABLE: case IP_DEST_NET_UNREACHABLE:return Status::Unavailable;
    default:return Status::Error;
    }
}
PingMonitor::~PingMonitor() {
    if (stage_==Stage::Resolve && cancel_) GetAddrInfoExCancel(&cancel_);
    // ICMP has a bounded timeout. Drain completion before releasing its reply buffer.
    if (stage_!=Stage::Idle) WaitForSingleObject(event_,INFINITE);
    if (addresses_) FreeAddrInfoExW(addresses_);
    if (icmp_!=INVALID_HANDLE_VALUE) IcmpCloseHandle(icmp_);
    if (event_) CloseHandle(event_);
    if (winsock_) WSACleanup();
}
void PingMonitor::start(Snapshot& s,const std::wstring& target,DWORD timeout) {
    if (stage_!=Stage::Idle) return;
    WSADATA data{};
    if (!winsock_) winsock_=WSAStartup(MAKEWORD(2,2),&data)==0;
    if (!event_) event_=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if (icmp_==INVALID_HANDLE_VALUE) icmp_=IcmpCreateFile();
    if (!winsock_ || !event_ || icmp_==INVALID_HANDLE_VALUE) { s.ping.miss(Status::Error); return; }
    timeout_=timeout;
    if (InetPtonW(AF_INET,target.c_str(),&address_)==1) { send(s); return; }
    if (target.empty()) { s.ping.miss(Status::Error); return; }
    ADDRINFOEXW hints{};
    hints.ai_family=AF_INET;
    operation_={};
    operation_.hEvent=event_;
    ResetEvent(event_);
    cancel_=nullptr;
    int result=GetAddrInfoExW(target.c_str(),nullptr,NS_DNS,nullptr,&hints,&addresses_,nullptr,&operation_,nullptr,&cancel_);
    if (result==WSA_IO_PENDING) stage_=Stage::Resolve;
    else if (result==0) resolved(s);
    else s.ping.miss(Status::Unavailable);
}
void PingMonitor::resolved(Snapshot& s) {
    if (!addresses_) { s.ping.miss(Status::Unavailable); return; }
    address_=reinterpret_cast<sockaddr_in*>(addresses_->ai_addr)->sin_addr;
    FreeAddrInfoExW(addresses_); addresses_=nullptr;
    send(s);
}
void PingMonitor::send(Snapshot& s) {
    ResetEvent(event_);
    DWORD count=IcmpSendEcho2(icmp_,event_,nullptr,nullptr,address_.S_un.S_addr,payload_,sizeof(payload_),
        nullptr,buffer_,sizeof(buffer_),timeout_);
    if (!count) {
        DWORD error=GetLastError();
        if (error==ERROR_IO_PENDING) { stage_=Stage::Echo; return; }
        s.ping.miss(pingStatus(error)); return;
    }
    const auto& reply=*reinterpret_cast<ICMP_ECHO_REPLY*>(buffer_);
    if (reply.Status==IP_SUCCESS) s.ping.set(reply.RoundTripTime);
    else s.ping.miss(pingStatus(reply.Status));
}
void PingMonitor::complete(Snapshot& s) {
    const auto prior=stage_;
    stage_=Stage::Idle;
    if (prior==Stage::Resolve) {
        if (GetAddrInfoExOverlappedResult(&operation_)==0) resolved(s);
        else s.ping.miss(Status::Unavailable);
    } else if (prior==Stage::Echo) {
        const DWORD count=IcmpParseReplies(buffer_,sizeof(buffer_));
        const auto& reply=*reinterpret_cast<ICMP_ECHO_REPLY*>(buffer_);
        const DWORD code=count ? reply.Status : GetLastError();
        if (count && code==IP_SUCCESS) s.ping.set(reply.RoundTripTime);
        else s.ping.miss(code ? pingStatus(code) : Status::Unavailable);
    }
}
}
