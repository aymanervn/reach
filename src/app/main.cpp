#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include "reach/app/composition_root.h"
#include "reach/platform/shell_registration.h"

static const wchar_t *REACH_LAUNCH_ARG = L"--launch";
static const wchar_t *REACH_INSTANCE_MUTEX = L"Local\\Reach.Shell.Instance";

static int32_t reach_arg_present(int argc, wchar_t **argv, const wchar_t *arg)
{
    if (arg == nullptr) {
        return 0;
    }

    for (int index = 0; index < argc; ++index) {
        if (lstrcmpiW(argv[index], arg) == 0) {
            return 1;
        }
    }

    return 0;
}

static HANDLE reach_acquire_instance_mutex(void)
{
    HANDLE mutex = CreateMutexW(nullptr, TRUE, REACH_INSTANCE_MUTEX);
    if (mutex == nullptr) {
        return nullptr;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return nullptr;
    }

    return mutex;
}

static reach_result reach_current_exe_path(uint16_t *path, DWORD path_count)
{
    if (path == nullptr || path_count == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD length = GetModuleFileNameW(nullptr, reinterpret_cast<wchar_t *>(path), path_count);
    return length > 0 && length < path_count ? REACH_OK : REACH_ERROR;
}

static void reach_log_startup_line(const wchar_t *message)
{
    if (message == nullptr) {
        return;
    }

    OutputDebugStringW(message);
    OutputDebugStringW(L"\n");
}

static void reach_restore_explorer_if_current_shell(void)
{
    uint16_t exe_path[260] = {};
    reach_shell_registration_status status = {};

    if (reach_current_exe_path(exe_path, 260) == REACH_OK &&
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

    if (reach_current_exe_path(exe_path, 260) != REACH_OK ||
        reach_windows_shell_mark_startup_attempt(exe_path, &attempts, &restore_required) != REACH_OK ||
        !restore_required) {
        return 0;
    }

    (void)attempts;
    reach_restore_explorer_if_current_shell();
    reach_log_startup_line(L"Reach restored Explorer after repeated early startup failures.");
    return 1;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance, PWSTR command_line, int show_command)
{
    (void)instance;
    (void)previous_instance;
    (void)command_line;
    (void)show_command;

    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int32_t launch_allowed = argv != nullptr && reach_arg_present(argc, argv, REACH_LAUNCH_ARG);
    if (argv != nullptr) {
        LocalFree(argv);
    }

    if (!launch_allowed) {
        MessageBoxW(
            nullptr,
            L"Reach must be started with reachctl --install.",
            L"Reach",
            MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    HANDLE instance_mutex = reach_acquire_instance_mutex();
    if (instance_mutex == nullptr) {
        return 0;
    }

    if (reach_guard_shell_startup()) {
        CloseHandle(instance_mutex);
        return 1;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        CloseHandle(instance_mutex);
        return 1;
    }

    int exit_code = 0;
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
        CloseHandle(instance_mutex);
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

    exit_code = (int)message.wParam;

    if (app != nullptr) {
        (void)reach_app_stop(app);
        reach_app_destroy(app);
    }

    CoUninitialize();
    CloseHandle(instance_mutex);
    return exit_code;
}
