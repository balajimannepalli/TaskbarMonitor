#include "core/Application.h"
#include "utils/Logger.h"
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    try {
        return Application{}.run(instance);
    } catch (...) {
        logMessage(LogLevel::Error,L"Application initialization or persistence failed.");
        return 5;
    }
}
