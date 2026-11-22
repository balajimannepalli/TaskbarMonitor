#include "ui/SettingsWindow.h"
#include "Application.h"
#include "ApplicationContext.h"
#include "monitor/MonitorEngine.h"
#include "utils/Startup.h"
#include "../utils/Logger.h"
#include <shellapi.h>
#include <cerrno>
#include <string_view>

namespace {
void readOptions(const nlohmann::json& settings,OverlayOptions& options) {
    options.inset = settings["overlay"]["inset"].get<int>();
    options.right = settings["overlay"]["right"].get<bool>();
    options.centered = settings["overlay"]["centered"].get<bool>();
    const auto monitor = settings["overlay"]["monitor"].get<std::string>();
    const std::u8string monitorUtf8(monitor.begin(), monitor.end());
    options.monitor = std::filesystem::path(monitorUtf8).wstring();
    options.fontSize = settings["fontSize"].get<float>();
    options.spacing = settings["metricSpacing"].get<int>();
    for (size_t i=0;i<display::keys.size();++i)
        options.visible[i]=settings["enabledMetrics"].at(display::keys[i]).get<bool>();
    options.theme=settings["theme"];
    options.compact=settings["compact"];
    options.transparentBackground=settings["transparentBackground"];
    options.padding=settings["padding"];
    options.verticalOffset=settings["overlay"]["verticalOffset"];
    const auto family=settings["fontFamily"].get<std::string>();
    options.family=std::filesystem::path(std::u8string(family.begin(),family.end())).wstring();
    options.foreground=std::stoul(settings["textColor"].get<std::string>().substr(1),nullptr,16);
    options.background=std::stoul(settings["backgroundColor"].get<std::string>().substr(1),nullptr,16);
    for(size_t i=0;i<options.order.size();++i) {
        auto key=settings["metricOrder"][i].get<std::string>();
        options.order[i]=std::find(display::keys.begin(),display::keys.end(),key)-display::keys.begin();
    }
}
}
int Application::run(HINSTANCE instance) {
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) return 1;
    int argc{};
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 1;
    struct Arguments { wchar_t** value; ~Arguments() { LocalFree(value); } } owner{argv};
    if (!distribution::portable && argc==2 && std::wstring_view(argv[1])==L"--register-startup") {
        startup::set(true);
        return 0;
    }
    bool openSettings=false;
    for(int i=1;i<argc;++i) if(std::wstring_view(argv[i])==L"--settings") openSettings=true;
    ApplicationContext context;
    if (!context.instance.primary()) {
        if(openSettings) {
            if(HWND w=FindWindowW(distribution::windowClass,nullptr)) PostMessageW(w,WM_APP+5,0,0);
        } else context.instance.notifyExisting(distribution::windowClass);
        return 0;
    }
    context.config.load();
    auto settings = context.config.settings();
    auto& options = context.overlay;
    readOptions(settings,options);
    bool development = false;
    bool overridePosition = false;
    for (int i = 1; i < argc; ++i) {
        const std::wstring_view flag(argv[i]);
        if(flag==L"--settings") continue;
        if (flag == L"--phase0-test") development = true;
        else if (flag == L"--right") { options.right = true; options.centered=false; overridePosition = true; }
        else if (flag == L"--monitor" && i + 1 < argc) {
            options.monitor = argv[++i];
            overridePosition = true;
        } else if (flag == L"--inset" && i + 1 < argc) {
            wchar_t* end{};
            const auto input = argv[++i];
            errno = 0;
            const long value = std::wcstol(input, &end, 10);
            if (errno || end == input || *end || value < 0 || value > 16384) return 2;
            options.inset = value;
            overridePosition = true;
        } else return 2;
    }
    if (development && !overridePosition) options = OverlayOptions{};
    logMessage(LogLevel::Info,L"Application initialized.");
    int result;
    {
        std::optional<monitor::MonitorEngine> engine;
        if (!development) engine.emplace(monitor::Settings::fromConfig(settings));
        auto edit=[&](HWND w) {
            auto draft=settings;
            draft["overlay"]["right"]=options.right;
            draft["overlay"]["centered"]=options.centered;
            if(!editSettings(w,context.config,draft)) return false;
            settings=std::move(draft);
            readOptions(settings,options);
            engine->configure(monitor::Settings::fromConfig(settings));
            return true;
        };
        result = runOverlay(instance, options,engine ? &engine->metrics() : nullptr,edit,openSettings);
    }
    if (result == 0 && !development) {
        if (!overridePosition) {
            settings["overlay"]["right"] = options.right;
            settings["overlay"]["centered"] = options.centered;
        }
        context.config.save(settings);
    }
    if (result) logMessage(LogLevel::Error,L"Overlay initialization or message processing failed.");
    logMessage(LogLevel::Info,L"Application stopped.");
    return result;
}

