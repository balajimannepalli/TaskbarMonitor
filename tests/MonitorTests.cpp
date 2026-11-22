#include "monitor/PingMonitor.h"
#include "monitor/MonitorEngine.h"
#include "monitor/GpuMonitor.h"
#include "monitor/SystemCollectors.h"
#include <iostream>
#include <atomic>
using namespace monitor;
using namespace std::chrono_literals;
int main() {
    int failures=0;
    auto check=[&](bool condition,const char* name) {
        if (!condition) { std::cerr<<name<<"\n"; ++failures; }
    };
    Snapshot s;
    NetworkMonitor net;
    const auto t=Clock::now();
    net.update(s,{{1,100,200},{2,200,300}},t);
    check(!s.download.value && s.download.status==Status::Pending,"network warmup");
    net.update(s,{{1,200,400},{2,400,700}},t+2s);
    check(s.download.value==150 && s.upload.value==300,"multiple physical interfaces / actual seconds");
    net.update(s,{{2,500,900}},t+3s);
    check(s.download.value==100 && s.upload.value==200,"removed interface");
    auto last=s.download.lastSuccess;
    net.update(s,{{2,2,4}},t+4s);
    check(!s.download.value && s.download.lastSuccess==last,"reset retains last successful time");
    net.update(s,{{2,UINT64_MAX,UINT64_MAX}},t+5s);
    net.update(s,{{2,0,0}},t+6s);
    check(!s.download.value,"wrapped counter");
    net.update(s,{},t+7s);
    net.update(s,{{2,5000,8000}},t+8s);
    check(!s.upload.value,"reconnection rebaselines");
    CpuMonitor cpu;
    cpu.update(s,{10,20,30});
    check(s.cpu.status==Status::Pending,"CPU first sample");
    cpu.update(s,{20,50,50});
    check(s.cpu.value==80,"CPU utilization");
    cpu.update(s,{20,50,50});
    check(!s.cpu.value,"zero CPU delta");
    cpu.update(s,{30,49,51});
    check(!s.cpu.value,"invalid CPU delta");
    RamMonitor::update(s,800,300,63);
    check(s.usedBytes==500 && s.ram.value==62.5 && s.memoryLoad==63,"physical memory");
    RamMonitor::update(s,100,101,0);
    check(!s.ram.value,"invalid RAM");
    const auto parsed=parseEngine(L"pid_10_luid_0x00000001_0x000000ab_phys_1_eng_2_engtype_Copy");
    check(parsed && parsed->luid==0x1000000ab && parsed->physical==1 && parsed->engine==2,"engine identity");
    check(!parseEngine(L"malformed") && !parseEngine(L"pid_10_luid_0x0_0xab_phys_1_eng_2"),"invalid instance");
    check(!parseEngine(L"pid_1_luid_0x0_0x100000000_phys_0_eng_1_engtype_Copy") &&
          !parseEngine(L"pid_1_luid_0x0_0xab_phys_0_eng_-1_engtype_Copy") &&
          !parseEngine(L"pid_1_luid_0x0_0xab_phys_0_eng_4294967296_engtype_Copy"),
          "invalid counters cannot alias a valid engine");
    const auto limit=parseEngine(L"pid_4294967295_luid_0xFFFFFFFF_0xFFFFFFFF_phys_0_eng_0_engtype_3D");
    check(limit && limit->luid==UINT64_MAX,"full-width uppercase LUID");
    Engines engines;
    engines[{12,0,1}]+=25; engines[{12,0,1}]+=30;
    engines[{12,0,2}]=40; engines[{13,0,1}]=50;
    selectBusiest(s,engines);
    check(s.gpu.value==55 && s.gpuLuid==12 && s.engines==3,"busiest physical engine");
    selectBusiest(s,{});
    check(!s.gpu.value,"missing GPU");
    selectBusiest(s,{{{1,0,0},NAN}});
    check(!s.gpu.value,"invalid GPU value");
    check(pingStatus(IP_SUCCESS)==Status::Valid && pingStatus(IP_REQ_TIMED_OUT)==Status::Timeout &&
          pingStatus(ERROR_OPERATION_ABORTED)==Status::Cancelled,"probe status");
    PingMonitor ping;
    ping.start(s,L"",500);
    check(s.ping.status==Status::Error,"empty target");
    std::wstring localhost=L"localhost";
    ping.start(s,localhost,1000);
    while (auto event=ping.pendingEvent()) {
        if (WaitForSingleObject(event,5000)!=WAIT_OBJECT_0) {check(false,"loopback completion");break;}
        ping.complete(s);
    }
    check(s.ping.value.has_value(),"resolved loopback echo");
    // Exercise destruction while DNS or ICMP may still own the overlapped storage.
    auto cancelStart=Clock::now();
    {
        PingMonitor cancelling;
        cancelling.start(s,localhost,100);
    }
    check(Clock::now()-cancelStart<2s,"pending probe drains on cancellation");
    check(nextDeadline(t,t+3100ms,1s)==t+4s && nextDeadline(t,t+3100ms,2s)==t+4s &&
          nextDeadline(t,t+200ms,2s)==t+2s,"independent skipped deadlines");
    MetricsState shared;
    std::atomic<bool> torn=false;
    std::jthread writer([&] {Snapshot v; for (int i=1;i<=10000;++i) {
        v.download.set(i);v.upload.set(2*i); shared.merge(v,false);
    }});
    for(int i=0;i<10000;++i) {
        auto v=shared.read();
        if(v.download.value && v.upload.value!=2*(*v.download.value)) torn=true;
    }
    writer.join(); check(!torn,"coherent snapshots");
    Settings settings;
    settings.enabled={false,true,true,false,false,true};
    settings.interval={1000,100,250,1000,1000,100};
    settings.target=L"";
    auto begin=Clock::now();
    {
        MonitorEngine engine(settings);
        std::this_thread::sleep_for(650ms);
        auto v=engine.snapshot();
        check(v.cpu.value && v.ram.value && v.ping.status==Status::Error,"failed ping isolated");
        check(v.calls[Cpu]>v.calls[Ram] && !v.calls[Gpu],"independent scheduled collectors");
        begin=Clock::now();
        settings.interval.fill(250);
        settings.enabled[Ram]=false;
        engine.configure(settings);
        std::this_thread::sleep_for(350ms);
        auto revised=engine.snapshot();
        std::this_thread::sleep_for(350ms);
        check(engine.snapshot().calls[Ram]==revised.calls[Ram] && engine.snapshot().calls[Cpu]>revised.calls[Cpu],
              "live interval and visibility changes");
        begin=Clock::now();
    }
    check(Clock::now()-begin<1s,"interruptible shutdown");
    Metric<Unit::BytesPerSecond> rate;
    check(format(rate)=="pending","missing formatted");
    rate.set(1250000);check(format(rate)=="1.2 MB/s","decimal units");
    return failures ? 1 : 0;
}
