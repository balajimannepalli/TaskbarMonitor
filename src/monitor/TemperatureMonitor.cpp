#include "GpuMonitor.h"
#include <winternl.h>
#include <d3dkmthk.h>
namespace monitor {
void sampleTemperature(Snapshot& s) {
    s.cpuTemperature.miss(Status::Unsupported);
    s.gpuTemperature.miss(Status::Unavailable);
    D3DKMT_ENUMADAPTERS list{};
    if (D3DKMTEnumAdapters(&list)<0) return;
    for (ULONG i=0;i<list.NumAdapters;++i) {
        const auto& adapter=list.Adapters[i];
        const uint64_t luid=(uint64_t(static_cast<ULONG>(adapter.AdapterLuid.HighPart))<<32)|adapter.AdapterLuid.LowPart;
        if ((!s.gpuLuid || luid==s.gpuLuid) && !s.gpuTemperature.value) {
            D3DKMT_ADAPTER_PERFDATA data{};
            data.PhysicalAdapterIndex=s.gpuLuid ? s.physicalGpu : 0;
            D3DKMT_QUERYADAPTERINFO query{adapter.hAdapter,KMTQAITYPE_ADAPTERPERFDATA,&data,sizeof(data)};
            if (D3DKMTQueryAdapterInfo(&query)>=0 && data.Temperature>0 && data.Temperature<2000)
                s.gpuTemperature.set(data.Temperature/10.0);
        }
        D3DKMT_CLOSEADAPTER close{adapter.hAdapter};
        (void)D3DKMTCloseAdapter(&close);
    }
}
}
