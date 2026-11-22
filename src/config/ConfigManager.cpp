#include "ConfigManager.h"
#include "../utils/Logger.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <stdexcept>
#include "ui/MetricDisplay.h"

using nlohmann::json;
ConfigManager::ConfigManager(std::filesystem::path path) : path_(std::move(path)), settings_(defaults()) {}

std::filesystem::path ConfigManager::defaultPath() {
    PWSTR raw{};
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &raw)))
        throw std::runtime_error("Local application data unavailable");
    struct FreePath { PWSTR value; ~FreePath() { CoTaskMemFree(value); } } owner{raw};
    return std::filesystem::path(raw) / L"TaskbarMonitor" / L"config.json";
}

json ConfigManager::defaults() {
    return {{"version",1},{"startup",false},{"theme","system"},{"metricOrder",display::keys},
        {"fontFamily","Segoe UI"},{"textColor","#F3F3F3"},{"backgroundColor","#252525"},
        {"padding",2},{"compact",false},{"transparentBackground",false},
        {"enabledMetrics",{{"download",true},{"upload",true},{"cpu",true},{"gpu",true},
            {"ram",true},{"cpuTemperature",false},{"gpuTemperature",false},{"ping",true}}},
        {"refreshIntervals",{{"network",1000},{"cpu",1000},{"gpu",1000},{"ram",2000},
            {"temperature",5000},{"ping",2000}}},
        {"overlay",{{"monitor",""},{"inset",0},{"right",false},{"centered",false},{"verticalOffset",0}}},
        {"fontSize",12},{"metricSpacing",8},{"pingTarget","1.1.1.1"},{"pingTimeoutMs",1000}};
}

namespace {
constexpr size_t maxConfigBytes = 1024 * 1024;
void mergeKnown(json& result, const json& defaults) {
    for (const auto& [key, value] : defaults.items()) {
        if (!result.contains(key)) result[key] = value;
        else if (value.is_object()) {
            if (!result[key].is_object()) result[key] = value;
            else mergeKnown(result[key], value);
        } else if (value.is_boolean() && !result[key].is_boolean()) result[key] = value;
        else if (value.is_string() && !result[key].is_string()) result[key] = value;
    }
}
void integer(json& value, int low, int high, int fallback) {
    if (!value.is_number_integer() || value < low || value > high) value = fallback;
}
}
json ConfigManager::validate(const json& input) {
    json result = input.is_object() ? input : json::object();
    const auto baseline = defaults();
    mergeKnown(result, baseline);
    if (input.contains("overlay") && !input["overlay"].contains("centered") &&
        result["overlay"]["inset"]==352) result["overlay"]["inset"]=0;
    if (result["overlay"]["centered"]==true) result["overlay"]["right"]=false;
    auto order=json::array();
    if(result["metricOrder"].is_array()) for(const auto& key:result["metricOrder"]) {
        if(std::find(baseline["metricOrder"].begin(),baseline["metricOrder"].end(),key)!=baseline["metricOrder"].end()
            && std::find(order.begin(),order.end(),key)==order.end()) order.push_back(key);
    }
    for(const auto& key:display::keys) if(std::find(order.begin(),order.end(),key)==order.end()) order.push_back(key);
    result["metricOrder"]=std::move(order);
    integer(result["padding"],0,12,2);
    integer(result["overlay"]["verticalOffset"],-128,128,0);
    for(const char* key:{"textColor","backgroundColor"}) {
        const auto& color=result[key].get_ref<const std::string&>();
        if(color.size()!=7 || color[0]!='#' || color.find_first_not_of("0123456789ABCDEFabcdef",1)!=std::string::npos)
            result[key]=baseline[key];
    }
    const auto& family=result["fontFamily"].get_ref<const std::string&>();
    if(family.empty() || family.size()>124 || family.find('\0')!=std::string::npos) result["fontFamily"]="Segoe UI";
    integer(result["version"],1,1,1);
    if (result["theme"] != "system" && result["theme"] != "light" &&
        result["theme"] != "dark" && result["theme"] != "custom") result["theme"] = "system";
    for (auto& [key,value] : result["refreshIntervals"].items()) {
        if (baseline["refreshIntervals"].contains(key))
            integer(value,250,3600000,baseline["refreshIntervals"][key].get<int>());
    }
    integer(result["overlay"]["inset"],0,16384,0);
    integer(result["fontSize"],8,18,12);
    integer(result["metricSpacing"],0,64,8);
    integer(result["pingTimeoutMs"],100,5000,1000);
    auto& monitor = result["overlay"]["monitor"];
    const auto monitorText = monitor.get<std::string>();
    if (monitorText.size() > 32 || monitorText.find('\0') != std::string::npos) monitor = "";
    const auto target = result["pingTarget"].get<std::string>();
    if (target.empty() || target.size() > 253 || target.find_first_of(" \t\r\n") != std::string::npos ||
        target.find('\0') != std::string::npos) result["pingTarget"] = "1.1.1.1";
    return result;
}
void ConfigManager::load() {
    settings_ = defaults();
    writable_ = true;
    original_.clear();
    std::filesystem::create_directories(path_.parent_path());
    if (!std::filesystem::exists(path_)) return;
    if (std::filesystem::file_size(path_) > maxConfigBytes) {
        writable_ = false;
        logMessage(LogLevel::Warning,L"Configuration exceeds size limit; using defaults.");
        return;
    }
    std::ifstream stream(path_, std::ios::binary);
    if (!stream) throw std::runtime_error("Configuration read failed");
    original_.assign(std::istreambuf_iterator<char>(stream), {});
    if (stream.bad()) throw std::runtime_error("Configuration read failed");
    const auto parsed = json::parse(original_, nullptr, false);
    if (!parsed.is_object()) {
        writable_ = false;
        logMessage(LogLevel::Warning,L"Malformed configuration preserved; using defaults.");
        return;
    }
    if (parsed.contains("version") && parsed["version"].is_number() && parsed["version"] > 1) {
        writable_ = false;
        logMessage(LogLevel::Warning,L"Future configuration version preserved.");
    }
    settings_ = validate(parsed);
}
void ConfigManager::save(const json& settings) {
    if (!writable_) return;
    const auto normalized = validate(settings);
    if(normalized!=settings) throw std::invalid_argument("Settings failed validation");
    auto bytes = normalized.dump(2) + "\n";
    if (bytes.size() > maxConfigBytes) {
        bytes = normalized.dump();
        if (bytes.size() > maxConfigBytes)
            throw std::length_error("Configuration exceeds the 1 MiB size limit");
    }
    if (bytes == original_) return;
    // A single application instance owns this path. Keep the replacement on the same volume.
    auto temporary = path_;
    temporary += L".tmp";
    const HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Configuration write failed");
    DWORD written{};
    const bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr)
        && written == bytes.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if (!ok || !MoveFileExW(temporary.c_str(), path_.c_str(),
                            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        throw std::runtime_error("Configuration replacement failed");
    }
    original_ = bytes;
    settings_ = normalized;
}
