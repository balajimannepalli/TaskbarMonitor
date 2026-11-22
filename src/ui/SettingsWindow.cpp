#include "SettingsWindow.h"
#include "core/Distribution.h"
#include "SettingsIds.h"
#include "MetricDisplay.h"
#include "utils/Startup.h"
#include <thread>
#include <cwchar>
#include <stdexcept>
#include <dwrite.h>
#include <wrl/client.h>
namespace {
using nlohmann::json;
constexpr UINT finished=WM_APP+82;
constexpr const char* intervals[]{"network","cpu","gpu","ram","temperature","ping"};
constexpr const wchar_t* labels[]{L"Download",L"Upload",L"CPU usage",L"GPU usage",L"Memory",L"CPU temperature",L"GPU temperature",L"Ping"};
constexpr const char* themes[]{"system","light","dark","custom"};
std::wstring wide(const std::string& s) {return std::filesystem::path(std::u8string(s.begin(),s.end())).wstring();}
std::string utf8(const std::wstring& s) {auto bytes=std::filesystem::path(s).u8string(); return {bytes.begin(),bytes.end()};}
std::wstring field(HWND w,int id) {
    HWND h=GetDlgItem(w,id);
    std::wstring s(GetWindowTextLengthW(h)+1,L'\0');
    s.resize(GetWindowTextW(h,s.data(),static_cast<int>(s.size())));
    return s;
}
struct Invalid {const wchar_t* message; int id;};
bool installedFamily(const std::wstring& name) {
    Microsoft::WRL::ComPtr<IDWriteFactory> factory;
    Microsoft::WRL::ComPtr<IDWriteFontCollection> fonts;
    UINT32 index{};
    BOOL exists=FALSE;
    return SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),
               reinterpret_cast<IUnknown**>(factory.GetAddressOf()))) &&
           SUCCEEDED(factory->GetSystemFontCollection(&fonts)) &&
           SUCCEEDED(fonts->FindFamilyName(name.c_str(),&index,&exists)) && exists;
}
int number(HWND w,int id,int lo,int hi,const wchar_t* error) {
    auto s=field(w,id);
    wchar_t* end{};
    long n=wcstol(s.c_str(),&end,10);
    if(s.empty() || *end || n<lo || n>hi) throw Invalid{error,id};
    return static_cast<int>(n);
}
LRESULT item(HWND w,int id,UINT msg,WPARAM wp=0,LPARAM lp=0) {return SendDlgItemMessageW(w,id,msg,wp,lp);}
BOOL CALLBACK addMonitor(HMONITOR h,HDC,LPRECT,LPARAM control) {
    MONITORINFOEXW m{};m.cbSize=sizeof(m);
    if(GetMonitorInfoW(h,&m)) SendMessageW(reinterpret_cast<HWND>(control),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(m.szDevice));
    return TRUE;
}
struct Editor {
    ConfigManager& config;
    json draft;
    std::thread worker;
    bool saved=false;
    bool busy=false;
    ~Editor(){if(worker.joinable()) worker.join();}
    int selected(HWND w) const {return static_cast<int>(item(w,ID_METRICS,LB_GETCURSEL));}
    void select(HWND w,int i) {
        item(w,ID_METRICS,LB_SETCURSEL,i);
        const auto key=draft["metricOrder"][i].get<std::string>();
        CheckDlgButton(w,ID_VISIBLE,draft["enabledMetrics"][key].get<bool>() ? BST_CHECKED : BST_UNCHECKED);
        EnableWindow(GetDlgItem(w,ID_UP),i>0); EnableWindow(GetDlgItem(w,ID_DOWN),i<7);
    }
    void list(HWND w,int index) {
        item(w,ID_METRICS,LB_RESETCONTENT);
        for(const auto& key:draft["metricOrder"]) {
            auto i=std::find(display::keys.begin(),display::keys.end(),key)-display::keys.begin();
            item(w,ID_METRICS,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(labels[i]));
        }
        select(w,index);
    }
    void fill(HWND w) {
        list(w,0);
        int theme=0;
        while(theme<3 && draft["theme"]!=themes[theme]) ++theme;
        item(w,ID_THEME,CB_SETCURSEL,theme);
        auto text=[&](int id,const char* key){SetDlgItemTextW(w,id,wide(draft.at(key).get<std::string>()).c_str());};
        text(ID_FONT,"fontFamily");text(ID_TEXT,"textColor");text(ID_BACK,"backgroundColor");
        SetDlgItemInt(w,ID_SIZE,draft["fontSize"],FALSE);SetDlgItemInt(w,ID_PAD,draft["padding"],FALSE);
        SetDlgItemInt(w,ID_SPACE,draft["metricSpacing"],FALSE);
        CheckDlgButton(w,ID_COMPACT,draft["compact"].get<bool>());
        const bool transparent=draft["transparentBackground"].get<bool>();
        CheckDlgButton(w,ID_TRANSPARENT_BACKGROUND,transparent);
        EnableWindow(GetDlgItem(w,ID_BACK),!transparent);
        CheckDlgButton(w,ID_STARTUP,draft["startup"].get<bool>());
        EnableWindow(GetDlgItem(w,ID_STARTUP),!distribution::portable);
        for(int i=0;i<6;++i) SetDlgItemInt(w,ID_NETWORK+i,draft["refreshIntervals"][intervals[i]],FALSE);
        auto& o=draft["overlay"];
        item(w,ID_ALIGN,CB_SETCURSEL,o["centered"].get<bool>() ? 1 : o["right"].get<bool>() ? 2 : 0);
        EnableWindow(GetDlgItem(w,ID_INSET),!o["centered"].get<bool>());
        SetDlgItemInt(w,ID_INSET,o["inset"],FALSE); SetDlgItemInt(w,ID_VERTICAL,o["verticalOffset"].get<int>(),TRUE);
        auto device=wide(o["monitor"].get<std::string>());
        auto ix=device.empty()?0:item(w,ID_MONITOR,CB_FINDSTRINGEXACT,static_cast<WPARAM>(-1),reinterpret_cast<LPARAM>(device.c_str()));
        if(ix==CB_ERR) ix=item(w,ID_MONITOR,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(device.c_str()));
        item(w,ID_MONITOR,CB_SETCURSEL,ix);
    }
    void save(HWND w) {
        if(!config.writable()) throw Invalid{L"Repair the protected configuration file before saving.",0};
        auto next=draft;
        next["theme"]=themes[item(w,ID_THEME,CB_GETCURSEL)];
        next["fontFamily"]=utf8(field(w,ID_FONT));
        next["textColor"]=utf8(field(w,ID_TEXT));next["backgroundColor"]=utf8(field(w,ID_BACK));
        next["fontSize"]=number(w,ID_SIZE,8,18,L"Font size must be 8–18.");
        next["metricSpacing"]=number(w,ID_SPACE,0,64,L"Spacing must be 0–64.");
        next["padding"]=number(w,ID_PAD,0,12,L"Padding must be 0–12.");
        next["compact"]=IsDlgButtonChecked(w,ID_COMPACT)==BST_CHECKED;
        next["transparentBackground"]=IsDlgButtonChecked(w,ID_TRANSPARENT_BACKGROUND)==BST_CHECKED;
        next["startup"]=IsDlgButtonChecked(w,ID_STARTUP)==BST_CHECKED;
        auto& o=next["overlay"];
        o["monitor"]=item(w,ID_MONITOR,CB_GETCURSEL)==0?"":utf8(field(w,ID_MONITOR));
        const auto anchor=item(w,ID_ALIGN,CB_GETCURSEL);
        o["right"]=anchor==2;
        o["centered"]=anchor==1;
        o["inset"]=number(w,ID_INSET,0,16384,L"Inset must be 0–16384.");
        o["verticalOffset"]=number(w,ID_VERTICAL,-128,128,L"Vertical offset must be -128 to 128.");
        for(int i=0;i<6;++i) next["refreshIntervals"][intervals[i]]=number(w,ID_NETWORK+i,250,3600000,L"Intervals must be 250–3600000 milliseconds.");
        auto valid=ConfigManager::validate(next);
        if(valid["fontFamily"]!=next["fontFamily"] || !installedFamily(field(w,ID_FONT)))
            throw Invalid{L"Enter the name of an installed font family.",ID_FONT};
        if(valid!=next) throw Invalid{L"Enter colors as # followed by six hexadecimal digits.",ID_TEXT};
        if(worker.joinable()) worker.join();
        worker=std::thread([this,w,next=std::move(next)] {
            try {
                const bool previous=!distribution::portable && startup::enabled();
                bool desired=next.at("startup").get<bool>();
                const bool changeStartup=!distribution::portable && previous!=desired;
                if(changeStartup) startup::set(desired);
                try {config.save(next);} catch(...) {if(changeStartup) startup::set(previous); throw;}
                PostMessageW(w,finished,1,0);
            } catch(...) {PostMessageW(w,finished,0,0);}
        });
        busy=true; EnableWindow(w,FALSE);
    }
};
INT_PTR CALLBACK dialog(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    auto* e=reinterpret_cast<Editor*>(GetWindowLongPtrW(w,DWLP_USER));
    try {
        if(msg==WM_INITDIALOG) {
            e=reinterpret_cast<Editor*>(lp);SetWindowLongPtrW(w,DWLP_USER,lp);
            for(auto name:{L"System",L"Light",L"Dark",L"Custom"}) item(w,ID_THEME,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
            for(auto name:{L"Left",L"Center",L"Right (before system tray)"}) item(w,ID_ALIGN,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
            item(w,ID_MONITOR,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Primary taskbar"));
            EnumDisplayMonitors(nullptr,nullptr,addMonitor,reinterpret_cast<LPARAM>(GetDlgItem(w,ID_MONITOR)));
            e->fill(w);return TRUE;
        }
        if(!e) return FALSE;
        if(msg==finished) {
            e->busy=false;
            if(wp) {e->saved=true; EndDialog(w,IDOK);}
            else {EnableWindow(w,TRUE);SetDlgItemTextW(w,ID_ERROR,L"Could not save configuration or update Windows startup.");}
            return TRUE;
        }
        if(e->busy) return msg==WM_CLOSE || msg==WM_COMMAND;
        if(msg==WM_CLOSE || (msg==WM_COMMAND && LOWORD(wp)==IDCANCEL)) {EndDialog(w,IDCANCEL);return TRUE;}
        if(msg!=WM_COMMAND) return FALSE;
        int i=e->selected(w);
        switch(LOWORD(wp)) {
        case ID_TRANSPARENT_BACKGROUND:
            EnableWindow(GetDlgItem(w,ID_BACK),IsDlgButtonChecked(w,ID_TRANSPARENT_BACKGROUND)!=BST_CHECKED);
            break;
        case ID_ALIGN:
            if(HIWORD(wp)==CBN_SELCHANGE) EnableWindow(GetDlgItem(w,ID_INSET),item(w,ID_ALIGN,CB_GETCURSEL)!=1);
            break;
        case ID_METRICS: if(HIWORD(wp)==LBN_SELCHANGE && i>=0)e->select(w,i);break;
        case ID_VISIBLE: e->draft["enabledMetrics"][e->draft["metricOrder"][i].get<std::string>()]=IsDlgButtonChecked(w,ID_VISIBLE)==BST_CHECKED;break;
        case ID_UP: case ID_DOWN: {
            int other=i+(LOWORD(wp)==ID_UP?-1:1);
            if(other>=0 && other<8) {std::swap(e->draft["metricOrder"][i],e->draft["metricOrder"][other]);e->list(w,other);}
            break;
        }
        case ID_DEFAULTS: e->draft.update(ConfigManager::defaults(),true);e->fill(w);SetDlgItemTextW(w,ID_ERROR,L"");break;
        case IDOK: e->save(w);break;
        default:return FALSE;
        }
        return TRUE;
    } catch(const Invalid& invalid) {
        SetDlgItemTextW(w,ID_ERROR,invalid.message);
        if(invalid.id) SetFocus(GetDlgItem(w,invalid.id));
    } catch(...) {SetDlgItemTextW(w,ID_ERROR,L"Could not apply the settings.");}
    return TRUE;
}
}
bool editSettings(HWND owner,ConfigManager& config,json& settings) {
    Editor e{config,settings,{}};
    e.draft["startup"]=!distribution::portable && startup::enabled();
    DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(ID_SETTINGS),owner,dialog,reinterpret_cast<LPARAM>(&e));
    if(e.worker.joinable()) e.worker.join();
    if(e.saved) settings=config.settings();
    return e.saved;
}
