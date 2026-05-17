#include <windows.h>
#include <objbase.h>

#include "reach/shell.h"

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

    reach_shell *shell = nullptr;
    reach_shell_desc desc = {};
    reach_result result = reach_shell_create(&desc, &shell);
    if (result == REACH_OK) {
        (void)reach_shell_start(shell);
    }

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (shell != nullptr) {
        (void)reach_shell_stop(shell);
        reach_shell_destroy(shell);
    }

    CoUninitialize();
    return (int)message.wParam;
}
