#include "Startup.h"
#include <string>
#include <stdexcept>
namespace startup {
namespace {
constexpr wchar_t name[]=L"TaskbarMonitor";
std::wstring command() {
    std::wstring path(32768,L'\0');
    DWORD n=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
    if(!n || n>=path.size()) throw std::runtime_error("Cannot locate executable");
    path.resize(n);
    return L"\""+path+L"\"";
}
}
bool enabled(const wchar_t* key) {
    wchar_t value[32768]{};
    DWORD bytes=sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER,key,name,RRF_RT_REG_SZ,nullptr,value,&bytes)==ERROR_SUCCESS
        && !_wcsicmp(value,command().c_str());
}
void set(bool enable,const wchar_t* key) {
    LSTATUS result;
    if(enable) {
        const auto value=command();
        result=RegSetKeyValueW(HKEY_CURRENT_USER,key,name,REG_SZ,value.c_str(),
                               static_cast<DWORD>((value.size()+1)*sizeof(wchar_t)));
    } else {
        result=RegDeleteKeyValueW(HKEY_CURRENT_USER,key,name);
        if(result==ERROR_FILE_NOT_FOUND || result==ERROR_PATH_NOT_FOUND) return;
    }
    if(result!=ERROR_SUCCESS) throw std::runtime_error("Startup registration failed");
}
}
