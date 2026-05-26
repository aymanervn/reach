#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <cwchar>

#include "reach/app/composition_root.h"
#include "reach/platform/shell_registration.h"

static void reach_write_console_line(const wchar_t *message)
{
    if (message == nullptr) {
        return;
    }

    AttachConsole(ATTACH_PARENT_PROCESS);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written = 0;
    if (output != INVALID_HANDLE_VALUE && output != nullptr) {
        if (!WriteConsoleW(output, message, (DWORD)wcslen(message), &written, nullptr)) {
            char utf8[1024] = {};
            int bytes = WideCharToMultiByte(CP_UTF8, 0, message, -1, utf8, (int)sizeof(utf8) - 3, nullptr, nullptr);
            if (bytes > 0) {
                utf8[bytes - 1] = '\r';
                utf8[bytes] = '\n';
                WriteFile(output, utf8, (DWORD)(bytes + 1), &written, nullptr);
            }
        } else {
            WriteConsoleW(output, L"\r\n", 2, &written, nullptr);
        }
    }
    OutputDebugStringW(message);
    OutputDebugStringW(L"\n");
}

static reach_result reach_get_current_exe(uint16_t *path, DWORD path_count)
{
    REACH_ASSERT(path != nullptr);
    if (path == nullptr || path_count == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD length = GetModuleFileNameW(nullptr, reinterpret_cast<wchar_t *>(path), path_count);
    return length > 0 && length < path_count ? REACH_OK : REACH_ERROR;
}

static int reach_handle_shell_command_line(void)
{
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr) {
        return -1;
    }

    uint16_t exe_path[260] = {};
    reach_result path_result = reach_get_current_exe(exe_path, 260);
    int exit_code = -1;
    for (int index = 1; index < argc; ++index) {
        if (lstrcmpiW(argv[index], L"--install-shell") == 0) {
            exit_code = path_result == REACH_OK && reach_windows_shell_install_current_user(exe_path) == REACH_OK ? 0 : 1;
            reach_write_console_line(exit_code == 0 ? L"Reach shell installed for current user." : L"Reach shell install failed.");
            break;
        }
        if (lstrcmpiW(argv[index], L"--restore-shell") == 0) {
            exit_code = reach_windows_shell_restore_current_user() == REACH_OK ? 0 : 1;
            reach_write_console_line(exit_code == 0 ? L"Windows shell restored for current user." : L"Windows shell restore failed.");
            break;
        }
        if (lstrcmpiW(argv[index], L"--print-shell-status") == 0) {
            reach_shell_registration_status status = {};
            exit_code = path_result == REACH_OK && reach_windows_shell_query_current_user(exe_path, &status) == REACH_OK ? 0 : 1;
            if (exit_code == 0) {
                wchar_t line[640] = {};
                swprintf_s(
                    line,
                    L"CurrentShell=%ls PreviousShell=%ls ReachIsShell=%d StartupAttemptCount=%u",
                    reinterpret_cast<const wchar_t *>(status.current_shell),
                    reinterpret_cast<const wchar_t *>(status.previous_shell),
                    status.reach_is_shell,
                    status.startup_attempt_count);
                reach_write_console_line(line);
            } else {
                reach_write_console_line(L"Reach shell status query failed.");
            }
            break;
        }
    }

    LocalFree(argv);
    return exit_code;
}

static void reach_restore_explorer_if_current_shell(void)
{
    uint16_t exe_path[260] = {};
    reach_shell_registration_status status = {};
    if (reach_get_current_exe(exe_path, 260) == REACH_OK &&
        reach_windows_shell_query_current_user(exe_path, &status) == REACH_OK &&
        status.reach_is_shell) {
        (void)reach_windows_shell_restore_current_user();
        (void)reach_windows_shell_launch_explorer();
    }
}

static int reach_guard_shell_startup(void)
{
    uint16_t exe_path[260] = {};
    uint32_t attempts = 0;
    int32_t restore_required = 0;
    if (reach_get_current_exe(exe_path, 260) != REACH_OK ||
        reach_windows_shell_mark_startup_attempt(exe_path, &attempts, &restore_required) != REACH_OK ||
        !restore_required) {
        return 0;
    }

    (void)attempts;
    reach_restore_explorer_if_current_shell();
    reach_write_console_line(L"Reach restored Explorer after repeated early startup failures.");
    return 1;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance, PWSTR command_line, int show_command)
{
    (void)instance;
    (void)previous_instance;
    (void)command_line;
    (void)show_command;

    int shell_command_exit = reach_handle_shell_command_line();
    if (shell_command_exit >= 0) {
        return shell_command_exit;
    }

    if (reach_guard_shell_startup()) {
        return 1;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return 1;
    }

    reach_app *app = nullptr;
    reach_shell_desc desc = {};
    reach_result result = reach_app_create(&desc, &app);
    if (result == REACH_OK) {
        result = reach_app_start(app);
        if (result == REACH_OK) {
            (void)reach_app_update(app, 0.0);
            (void)reach_windows_shell_clear_startup_attempts();
        }
    }
    if (result != REACH_OK) {
        reach_restore_explorer_if_current_shell();
        if (app != nullptr) {
            reach_app_destroy(app);
        }
        CoUninitialize();
        return 1;
    }

    MSG message = {};
    LARGE_INTEGER frequency = {};
    LARGE_INTEGER previous_counter = {};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&previous_counter);
    int running = 1;
    while (running) {
        int32_t needed_frame_before_wait = app != nullptr && reach_app_needs_frame(app);
        DWORD wait_ms = needed_frame_before_wait ? 16 : INFINITE;
        DWORD wait_result = MsgWaitForMultipleObjectsEx(
            0,
            nullptr,
            wait_ms,
            QS_ALLINPUT,
            MWMO_INPUTAVAILABLE);
        (void)wait_result;

        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running = 0;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (app != nullptr) {
            if (reach_app_has_pending_events(app)) {
                (void)reach_app_dispatch_events(app);
            }

            LARGE_INTEGER current_counter = {};
            QueryPerformanceCounter(&current_counter);

            int32_t needs_frame_after_messages = reach_app_needs_frame(app);
            if (needs_frame_after_messages) {
                double delta_seconds = frequency.QuadPart > 0
                    ? (double)(current_counter.QuadPart - previous_counter.QuadPart) / (double)frequency.QuadPart
                    : 0.016;
                previous_counter = current_counter;
                if (delta_seconds > 0.1) {
                    delta_seconds = 0.1;
                }
                (void)reach_app_update(app, delta_seconds);
            } else {
                previous_counter = current_counter;
            }
        }
    }

    if (app != nullptr) {
        (void)reach_app_stop(app);
        reach_app_destroy(app);
    }

    CoUninitialize();
    return (int)message.wParam;
}
