#include <windows.h>
#include <objbase.h>

#include "reach/app/composition_root.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance, PWSTR command_line, int show_command)
{
    (void)instance;
    (void)previous_instance;
    (void)command_line;
    (void)show_command;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return 1;
    }

    reach_app *app = nullptr;
    reach_shell_desc desc = {};
    reach_result result = reach_app_create(&desc, &app);
    if (result == REACH_OK) {
        (void)reach_app_start(app);
    }

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (app != nullptr) {
        (void)reach_app_stop(app);
        reach_app_destroy(app);
    }

    CoUninitialize();
    return (int)message.wParam;
}
