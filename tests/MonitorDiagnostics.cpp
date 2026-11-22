#include "monitor/MonitorEngine.h"
#include "config/ConfigManager.h"
#include <iostream>
using namespace monitor;
int main(int argc,char** argv) {
    try {
        int samples=argc>1 ? std::stoi(argv[1]) : 6;
        if (samples<1 || samples>3600) return 2;
        ConfigManager config(ConfigManager::defaultPath());
        config.load();
        auto settings=Settings::fromConfig(config.settings());
        settings.enabled.fill(true);
        settings.download=settings.upload=settings.cpuTemperature=settings.gpuTemperature=true;
        MonitorEngine engine(settings);
        for (int i=0;i<samples;++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            auto s=engine.snapshot();
            std::cout<<"Down "<<format(s.download)<<"  Up "<<format(s.upload)<<" ("<<s.interfaces<<" adapters)"
                <<"  CPU "<<format(s.cpu)<<"  RAM "<<format(s.ram)<<"  GPU "<<format(s.gpu)
                <<" ("<<s.engines<<" engines, LUID "<<std::hex<<s.gpuLuid<<std::dec<<")"
                <<"  CPU temp "<<format(s.cpuTemperature)<<"  GPU temp "<<format(s.gpuTemperature)
                <<"  Ping "<<format(s.ping)<<"\n";
            if (i+1==samples) {
                std::cout<<"Mean collector wall time (us), network/cpu/ram/gpu/temperature/ping:";
                for (size_t j=0;j<JobCount;++j) std::cout<<" "<<s.elapsedUs[j]/(std::max)(uint64_t{1},s.calls[j]);
                std::cout<<"\nScheduler bookkeeping "<<s.schedulerUs<<" us in "<<s.wakes<<" wakeups\n";
            }
        }
    } catch (...) {return 1;}
}
