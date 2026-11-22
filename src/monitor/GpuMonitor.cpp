#include "GpuMonitor.h"
#include <pdhmsg.h>
#include <cmath>
#include <limits>
#include <string_view>
namespace monitor {
std::optional<EngineId> parseEngine(const wchar_t* name) {
    if (!name) return {};
    std::wstring_view rest(name);
    auto consume=[&](std::wstring_view prefix) {
        if (!rest.starts_with(prefix)) return false;
        rest.remove_prefix(prefix.size());
        return true;
    };
    auto number=[&](unsigned base,unsigned& value) {
        const auto initial=rest.size();
        value=0;
        while (!rest.empty()) {
            const wchar_t c=rest.front();
            unsigned digit;
            if (c>=L'0' && c<=L'9') digit=c-L'0';
            else if (c>=L'a' && c<=L'f') digit=c-L'a'+10;
            else if (c>=L'A' && c<=L'F') digit=c-L'A'+10;
            else break;
            if (digit>=base || value>(std::numeric_limits<unsigned>::max()-digit)/base) return false;
            value=value*base+digit;
            rest.remove_prefix(1);
        }
        return rest.size()!=initial;
    };
    unsigned pid,high,low,physical,engine;
    if (!consume(L"pid_") || !number(10,pid) || !consume(L"_luid_0x") || !number(16,high) ||
        !consume(L"_0x") || !number(16,low) || !consume(L"_phys_") || !number(10,physical) ||
        !consume(L"_eng_") || !number(10,engine) || !consume(L"_engtype_") || rest.empty()) return {};
    return EngineId{(uint64_t(high)<<32)|low,physical,engine};
}
void selectBusiest(Snapshot& s,const Engines& engines) {
    s.engines=0; s.gpuLuid=0; s.physicalGpu=0;
    s.gpu.miss(Status::Unavailable);
    for (const auto& [id,total]:engines) {
        if (!std::isfinite(total) || total<0) continue;
        ++s.engines;
        // Process instances share a single engine with a physical maximum of 100%.
        const double pct = (std::min)(100.0,total);
        if (!s.gpu.value || pct>*s.gpu.value) {
            s.gpu.set(pct); s.gpuLuid=id.luid; s.physicalGpu=id.physical;
        }
    }
}
GpuMonitor::~GpuMonitor(){ if (query_) PdhCloseQuery(query_); }
void GpuMonitor::reset() {
    if (query_) PdhCloseQuery(query_);
    query_=nullptr; counter_=nullptr; warmed_=false;
    retry_=Clock::now()+std::chrono::seconds(10);
}
void GpuMonitor::sample(Snapshot& s) {
    selectBusiest(s,{});
    if (!query_) {
        if (Clock::now()<retry_) return;
        if (PdhOpenQueryW(nullptr,0,&query_)!=ERROR_SUCCESS ||
            PdhAddEnglishCounterW(query_,L"\\GPU Engine(*)\\Utilization Percentage",0,&counter_)!=ERROR_SUCCESS) {
            reset(); return;
        }
    }
    if (PdhCollectQueryData(query_)!=ERROR_SUCCESS) { reset(); return; }
    if (!warmed_) { warmed_=true; s.gpu.miss(Status::Pending); return; }
    DWORD bytes=static_cast<DWORD>(storage_.size()*sizeof(storage_[0])), count{};
    constexpr DWORD format=PDH_FMT_DOUBLE|PDH_FMT_NOCAP100;
    auto result=PdhGetFormattedCounterArrayW(counter_,format,&bytes,&count,storage_.data());
    if (result==PDH_MORE_DATA) {
        storage_.resize((bytes+sizeof(storage_[0])-1)/sizeof(storage_[0]));
        result=PdhGetFormattedCounterArrayW(counter_,format,&bytes,&count,storage_.data());
    }
    if (result!=ERROR_SUCCESS) { reset(); return; }
    Engines engines;
    for (DWORD i=0;i<count;++i) {
        auto& item=storage_[i];
        const auto& v=item.FmtValue;
        if ((v.CStatus!=PDH_CSTATUS_VALID_DATA && v.CStatus!=PDH_CSTATUS_NEW_DATA) ||
            !std::isfinite(v.doubleValue) || v.doubleValue<0) continue;
        if (auto id=parseEngine(item.szName)) engines[*id]+=v.doubleValue;
    }
    selectBusiest(s,engines);
}
}
