#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include "reach/app/composition_root.h"

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

    //make it crash
    // *(volatile int *)0 = 1;
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
        }
    }

    if (result != REACH_OK) {
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

                if (delta_seconds > 0.025) {
                    delta_seconds = 0.025;
                }

                reach_result update_result = reach_app_update(app, delta_seconds);
                if (update_result != REACH_OK) {
                    running = 0;
                    exit_code = 1;
                    break;
                }
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
