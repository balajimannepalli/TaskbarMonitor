#include "OverlayWindow.h"
#include "core/Distribution.h"
#include "Placement.h"
#include "../ui/Renderer.h"
#include <shellapi.h>
#include <algorithm>
#include <cerrno>
#include <cwchar>
#include <string>
#include <string_view>

#if !defined(_M_X64)
#error The Phase 0 prototype targets native x64.
#endif

namespace {
using distribution::windowClass;
constexpr UINT refresh = WM_APP + 1;
constexpr UINT paintCount = WM_APP + 2;
constexpr UINT creationCount = WM_APP + 3;
constexpr UINT_PTR watchdog = 1;


bool monitorInfo(HWND window, MONITORINFOEXW& info) {
    const auto monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONULL);
    info.cbSize = sizeof(info);
    return monitor && GetMonitorInfoW(monitor, &info);
}

struct Search {
    const std::wstring& device;
    HWND result{};
};

BOOL CALLBACK matchBar(HWND candidate, LPARAM param) {
    auto& search = *reinterpret_cast<Search*>(param);
    wchar_t name[64]{};
    if (!GetClassNameW(candidate, name, ARRAYSIZE(name))) return TRUE;
    if (wcscmp(name, L"Shell_TrayWnd") && wcscmp(name, L"Shell_SecondaryTrayWnd")) return TRUE;
    MONITORINFOEXW info{};
    if (monitorInfo(candidate, info) && !_wcsicmp(info.szDevice, search.device.c_str())) {
        search.result = candidate;
        return FALSE;
    }
    return TRUE;
}

HWND findBar(const OverlayOptions& options) {
    if (options.monitor.empty()) return FindWindowW(L"Shell_TrayWnd", nullptr);
    Search search{options.monitor};
    EnumWindows(matchBar, reinterpret_cast<LPARAM>(&search));
    return search.result;
}

BOOL CALLBACK notifyOverlay(HWND window, LPARAM) {
    wchar_t name[64]{};
    if (GetClassNameW(window, name, ARRAYSIZE(name)) && !wcscmp(name, windowClass))
        PostMessageW(window, refresh, 0, 0);
    return TRUE;
}

void CALLBACK barMoved(HWINEVENTHOOK, DWORD, HWND window, LONG object, LONG child,
                       DWORD, DWORD) {
    if (!window || object != OBJID_WINDOW || child != CHILDID_SELF) return;
    wchar_t name[64]{};
    if (GetClassNameW(window, name, ARRAYSIZE(name)) &&
        (!wcscmp(name, L"Shell_TrayWnd") || !wcscmp(name, L"Shell_SecondaryTrayWnd"))) {
        EnumThreadWindows(GetCurrentThreadId(), notifyOverlay, 0);
    }
}

bool foregroundCoversMonitor(HWND bar, RECT monitor) {
    HWND foreground = GetForegroundWindow();
    if (!foreground || foreground == bar || foreground == GetShellWindow()) return false;
    wchar_t name[64]{};
    GetClassNameW(foreground, name, ARRAYSIZE(name));
    if (!wcscmp(name, L"WorkerW") || !wcscmp(name, L"Progman")) return false;
    RECT bounds{};
    if (!GetWindowRect(foreground, &bounds) || bounds.left > monitor.left ||
        bounds.top > monitor.top || bounds.right < monitor.right || bounds.bottom < monitor.bottom)
        return false;
    // A maximized window can fill an auto-hide monitor even when Explorer has
    // revealed the taskbar above it. Respect the shell's actual stacking order.
    for (HWND above = GetWindow(bar, GW_HWNDPREV); above; above = GetWindow(above, GW_HWNDPREV))
        if (above == foreground) return true;
    return false;
}

struct Overlay {
    Overlay(OverlayOptions& value,const monitor::MetricsState* source) : options(value),metrics(source) {}
    OverlayOptions& options;
    const monitor::MetricsState* metrics;
    unsigned changes{};
    std::function<bool(HWND)> edit;
    bool editing{};
    void palette() {
        DWORD light=0,bytes=sizeof(light);
        if(options.theme=="system") RegGetValueW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"SystemUsesLightTheme",
            RRF_RT_REG_DWORD,nullptr,&light,&bytes);
        const auto colors=display::palette(options.theme,light!=0,{options.foreground,options.background});
        renderer.colors(colors.text,colors.background,options.padding);
    }
    HRESULT style() {
        renderer=Renderer{};
        palette();
        return renderer.initialize(options.fontSize,options.family.c_str(),options.transparentBackground);
    }
    void updateText(HWND window) {
        if (!metrics) return;
        monitor::Snapshot snapshot;
        if (!metrics->tryRead(snapshot)) return;
        auto next=display::cells(snapshot,options.visible,options.order);
        const auto hr=renderer.setCells(std::move(next),options.compact ? 0 : options.spacing);
        if (hr==S_FALSE) return;
        if (FAILED(hr)) { PostQuitMessage(4); return; }
        ++changes;
        InvalidateRect(window,nullptr,FALSE);
    }
    Renderer renderer;
    UINT taskbarCreated{};
    unsigned paints{}, shellMessages{};
    bool retryPaint{};
    HWINEVENTHOOK geometryHook{};
    DWORD observedPid{};

    ~Overlay() {
        if (geometryHook) UnhookWinEvent(geometryHook);
    }

    void synchronize(HWND window) {
        const HWND bar = findBar(options);
        DWORD pid{};
        if (bar) GetWindowThreadProcessId(bar, &pid);
        if (pid != observedPid) {
            if (geometryHook) UnhookWinEvent(geometryHook);
            geometryHook = pid ? SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE,
                EVENT_OBJECT_LOCATIONCHANGE, nullptr, barMoved, pid, 0, WINEVENT_OUTOFCONTEXT) : nullptr;
            observedPid = pid;
        }
        MONITORINFOEXW info{};
        RECT bounds{};
        std::optional<RECT> target;
        if (bar && IsWindowVisible(bar) && GetWindowRect(bar, &bounds) && monitorInfo(bar, info)
            && !foregroundCoversMonitor(bar, info.rcMonitor)) {
            const auto dpi=GetDpiForWindow(bar);
            const int count=static_cast<int>(std::count(options.visible.begin(),options.visible.end(),true));
            if (!metrics) target=prototype::placement(bounds,info.rcMonitor,dpi,options.inset,options.right);
            else if (count && dpi) {
                auto size=display::dimensions(count,options.fontSize,(options.compact ? 0 : options.spacing)+2*(options.padding-2),
                    MulDiv(bounds.right-bounds.left,96,dpi),MulDiv(bounds.bottom-bounds.top,96,dpi));
                if (size) {
                    const int availableHeight=MulDiv(bounds.bottom-bounds.top,96,dpi);
                    size->height=std::min(availableHeight,size->height+2*options.padding);
                    if(options.compact) size->height=std::max(count>1 ? 24 : 12,size->height-4);
                    RECT usable=bounds;
                    if (options.right) {
                        RECT tray{};
                        HWND notify=FindWindowExW(bar,nullptr,L"TrayNotifyWnd",nullptr);
                        if (!notify) notify=FindWindowExW(bar,nullptr,L"ClockButton",nullptr);
                        if (notify && GetWindowRect(notify,&tray) && tray.left>=bounds.left && tray.left<bounds.right)
                            usable.right=tray.left;
                        else if (bar==FindWindowW(L"Shell_TrayWnd",nullptr))
                            usable.right=usable.left; // Wait for Explorer to recreate the notification area.
                    }
                    target=prototype::placement(usable,info.rcMonitor,dpi,options.inset,
                                                options.right,size->width,size->height,options.centered);
                }
                if(target) {
                    LONG delta=std::clamp<LONG>(MulDiv(options.verticalOffset,dpi,96),
                        bounds.top-target->top,bounds.bottom-target->bottom);
                    OffsetRect(&*target,0,delta);
                }
            }
        }
        if (!target) {
            if (IsWindowVisible(window)) ShowWindow(window, SW_HIDE);
            return;
        }
        RECT old{};
        try { updateText(window); }
        catch (...) { PostQuitMessage(4); return; }
        const auto& r = *target;
        const POINT middle{(r.left + r.right) / 2, (r.top + r.bottom) / 2};
        const HWND under = GetAncestor(WindowFromPoint(middle), GA_ROOT);
        if (!IsWindowVisible(window) || !GetWindowRect(window, &old) || !EqualRect(&old, &r)
            || under == bar) {
            SetWindowPos(window, HWND_TOPMOST, r.left, r.top, r.right - r.left, r.bottom - r.top,
                         SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOOWNERZORDER);
        }
    }
};

LRESULT CALLBACK procedure(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    auto state = reinterpret_cast<Overlay*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        state = static_cast<Overlay*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (!state) return DefWindowProcW(window, msg, wp, lp);
    if (msg == state->taskbarCreated) {
        ++state->shellMessages;
        PostMessageW(window, refresh, 0, 0);
        return 0;
    }
    switch (msg) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND: return 1;
    case WM_RBUTTONUP:
        if (state->metrics) {
            POINT p{}; GetCursorPos(&p);
            HMENU menu=CreatePopupMenu();
            if(!menu) return 0;
            AppendMenuW(menu,MF_STRING,1,L"Settings…");
            AppendMenuW(menu,MF_STRING,2,L"Exit Taskbar Monitor");
            auto choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,window,nullptr);
            DestroyMenu(menu);
            if(choice) PostMessageW(window,choice==1 ? WM_APP+5 : WM_CLOSE,0,0);
            return 0;
        }
        [[fallthrough]];
    case WM_CLOSE: DestroyWindow(window); return 0;
    case WM_APP+5:
        if(state->metrics && !state->editing) {
            state->editing=true;
            if(state->edit(window)) {
                if(FAILED(state->style())) PostQuitMessage(4);
                else {
                    const auto style=GetWindowLongPtrW(window,GWL_EXSTYLE);
                    const auto updated=state->options.transparentBackground ? style|WS_EX_LAYERED : style&~WS_EX_LAYERED;
                    if (updated!=style) SetWindowLongPtrW(window,GWL_EXSTYLE,updated);
                    state->synchronize(window);
                    InvalidateRect(window,nullptr,FALSE);
                }
            }
            state->editing=false;
        }
        return 0;
    case WM_LBUTTONUP:
        // A click moves the sample without requiring keyboard focus.
        state->options.right = !state->options.right;
        state->options.centered = false;
        state->synchronize(window);
        return 0;
    case WM_DPICHANGED:
        InvalidateRect(window, nullptr, FALSE);
        [[fallthrough]];
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
        if(msg==WM_SETTINGCHANGE && state->options.theme=="system") {
            state->palette(); InvalidateRect(window,nullptr,FALSE);
        }
        PostMessageW(window, refresh, 0, 0);
        return 0;
    case WM_TIMER:
        if (wp != watchdog) break;
        if (state->retryPaint) InvalidateRect(window, nullptr, FALSE);
        [[fallthrough]];
    case refresh:
        state->synchronize(window);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        BeginPaint(window, &ps);
        const HRESULT hr = state->renderer.paint(window);
        EndPaint(window, &ps);
        state->retryPaint = hr == D2DERR_RECREATE_TARGET;
        if (FAILED(hr) && !state->retryPaint) {
            PostQuitMessage(4);
        } else if (hr == S_OK) {
            ++state->paints;
        }
        return 0;
    }
    case paintCount: return state->paints;
    case WM_APP+4: return state->changes;
    case WM_APP+6: {
        monitor::Snapshot snapshot;
        if(!state->metrics || !state->metrics->tryRead(snapshot)) return -1;
        return static_cast<LRESULT>(snapshot.schedulerUs);
    }
    case creationCount: return state->shellMessages;
    case WM_DESTROY:
        KillTimer(window, watchdog);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, msg, wp, lp);
}
}

int runOverlay(HINSTANCE instance, OverlayOptions& options,const monitor::MetricsState* metrics,
               std::function<bool(HWND)> edit,bool openSettings) {
    Overlay state(options,metrics);
    state.edit=std::move(edit);
    state.taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    if (!state.taskbarCreated || FAILED(state.style())) return 3;
    if (!metrics && FAILED(state.renderer.setCells({L"Overlay test"},0))) return 3;

    WNDCLASSW wc{};
    wc.lpfnWndProc = procedure;
    wc.hInstance = instance;
    wc.lpszClassName = windowClass;
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassW(&wc)) return 3;
    const DWORD exStyle=WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | (options.transparentBackground ? WS_EX_LAYERED : 0);
    HWND window = CreateWindowExW(exStyle, windowClass,
        L"Taskbar Monitor (Phase 0)", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, &state);
    if (!window) return 3;
    struct WindowOwner { HWND value; ~WindowOwner() { if (IsWindow(value)) DestroyWindow(value); } } owner{window};
    if (!SetTimer(window, watchdog, 1000, nullptr)) {
        DestroyWindow(window);
        return 3;
    }
    state.synchronize(window);
    if(openSettings) PostMessageW(window,WM_APP+5,0,0);
    MSG msg{};
    BOOL result{};
    while ((result = GetMessageW(&msg, nullptr, 0, 0)) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (IsWindow(window)) DestroyWindow(window);
    return result < 0 ? 3 : static_cast<int>(msg.wParam);
}

