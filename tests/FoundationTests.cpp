#include "config/ConfigManager.h"
#include "utils/SingleInstanceManager.h"
#include "utils/Startup.h"
#include <fstream>
#include <iostream>
#include <thread>

int main() {
    int failures{};
    auto check = [&](bool condition, const char* name) {
        if (!condition) { std::cerr << name << "\n"; ++failures; }
    };
    const auto root = std::filesystem::temp_directory_path() /
        (L"TaskbarMonitorTests-" + std::to_wstring(GetCurrentProcessId()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove_all(path,error); }
    } cleanup{root};
    const auto path = root / L"config.json";
    try {
        ConfigManager config(path);
        config.load();
        check(config.settings()["overlay"]["inset"] == 0, "flush left default");
        check(config.settings()["transparentBackground"]==false,"existing configurations default to an opaque background");
        auto legacy=ConfigManager::validate({{"overlay",{{"right",true},{"inset",352}}}});
        check(legacy["overlay"]["right"]==true && legacy["overlay"]["inset"]==0,"legacy default inset migration");
        check(!std::filesystem::exists(path), "load should not write");
        auto values = config.settings();
        values["overlay"]["inset"] = 420;
        values["futureExtra"] = "preserved";
        values["metricOrder"][0]="upload";values["metricOrder"][1]="download";
        values["enabledMetrics"]["cpu"]=false;
        values["theme"]="custom";values["fontFamily"]="Arial";values["fontSize"]=15;
        values["textColor"]="#ABCDEF";values["backgroundColor"]="#010203";
        values["metricSpacing"]=9;values["padding"]=3;values["compact"]=true;values["transparentBackground"]=true;
        values["overlay"]["monitor"]="\\\\.\\DISPLAY2";values["overlay"]["right"]=true;
        values["overlay"]["verticalOffset"]=-8;values["startup"]=true;
        for(auto& interval:values["refreshIntervals"]) interval=900;
        config.save(values);
        const auto stamp = std::filesystem::last_write_time(path);
        config.save(values);
        check(stamp == std::filesystem::last_write_time(path), "unchanged write");
        // Whitespace would push a valid compact file past the loader's limit.
        auto large=values;
        large["futureExtra"]="";
        large["futureExtra"]=std::string(1024*1024-large.dump().size(),'x');
        config.save(large);
        ConfigManager sizeCheck(path);
        sizeCheck.load();
        check(sizeCheck.writable() && sizeCheck.settings()==large,"compact save remains readable");
        large["futureExtra"].get_ref<std::string&>().push_back('x');
        bool oversizedRejected=false;
        try {config.save(large);} catch(const std::length_error&) {oversizedRejected=true;}
        sizeCheck.load();
        check(oversizedRejected && sizeCheck.writable() && sizeCheck.settings().dump().size()==1024*1024,
              "reject oversized serialization before replacing file");
        config.save(values);
        ConfigManager reloaded(path);
        reloaded.load();
        check(reloaded.settings() == values, "round trip");
        auto invalid=values;
        invalid["padding"]=-1;invalid["textColor"]="red";invalid["metricOrder"]={"cpu","cpu",7};
        const auto repaired=ConfigManager::validate(invalid);
        check(repaired["metricOrder"].size()==8 && repaired["metricOrder"][0]=="cpu" &&
              repaired["padding"]==2 && repaired["textColor"]=="#F3F3F3","field and order repair");
        bool rejected=false;
        try {config.save(invalid);} catch(const std::invalid_argument&) {rejected=true;}
        reloaded.load();
        check(rejected && reloaded.settings()==values,"reject invalid save");
        auto key=L"Software\\TaskbarMonitorTest-"+std::to_wstring(GetCurrentProcessId());
        DWORD marker=42;
        RegSetKeyValueW(HKEY_CURRENT_USER,key.c_str(),L"AnotherApp",REG_DWORD,&marker,sizeof(marker));
        startup::set(true,key.c_str());check(startup::enabled(key.c_str()),"registered current executable");
        startup::set(false,key.c_str());check(!startup::enabled(key.c_str()),"unregistered");
        DWORD bytes=sizeof(marker);
        check(RegGetValueW(HKEY_CURRENT_USER,key.c_str(),L"AnotherApp",RRF_RT_REG_DWORD,nullptr,&marker,&bytes)==ERROR_SUCCESS &&
              marker==42,"unrelated startup value preserved");
        RegDeleteTreeW(HKEY_CURRENT_USER,key.c_str());
        HANDLE locked = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        check(locked != INVALID_HANDLE_VALUE, "lock destination for failure test");
        auto changed = values;
        changed["theme"] = "dark";
        bool failed = false;
        try { config.save(changed); } catch (const std::exception&) { failed = true; }
        if (locked != INVALID_HANDLE_VALUE) CloseHandle(locked);
        check(failed, "replacement failure reported");
        reloaded.load();
        check(reloaded.settings() == values, "failed replacement preserved previous file");
        std::ofstream(path) << R"({"fontSize":999,"theme":"dark","overlay":{"inset":99},"refreshIntervals":{"cpu":-1},"startup":"bad"})";
        reloaded.load();
        check(reloaded.settings()["theme"] == "dark" && reloaded.settings()["overlay"]["inset"] == 99,
              "preserve valid fields");
        check(reloaded.settings()["fontSize"] == 12 && reloaded.settings()["refreshIntervals"]["cpu"] == 1000 &&
              reloaded.settings()["startup"] == false, "validation");
        std::ofstream(path) << "{broken";
        reloaded.load();
        check(!reloaded.writable() && reloaded.settings() == ConfigManager::defaults(), "malformed fallback");
        reloaded.save(values);
        std::ifstream input(path);
        std::string preserved;
        std::getline(input,preserved);
        input.close();
        check(preserved == "{broken", "malformed preserved");
        std::ofstream(path) << R"({"version":2,"theme":"light"})";
        reloaded.load();
        check(!reloaded.writable() && reloaded.settings()["theme"] == "light", "future version");
        const std::wstring name = L"Local\\TaskbarMonitor.Test." + std::to_wstring(GetCurrentProcessId());
        {
            SingleInstanceManager first(name);
            check(first.primary(),"primary ownership");
            bool duplicate = false;
            std::thread contender([&] { SingleInstanceManager second(name); duplicate = !second.primary(); });
            contender.join();
            check(duplicate,"concurrent exclusion");
        }
        SingleInstanceManager again(name);
        check(again.primary(),"released ownership");
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
    return failures ? 1 : 0;
}
