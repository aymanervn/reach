#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include "reach/composition/composition_root.h"
#include "reach/core/frame_clock.h"
#include "reach/support/workload.h"
#include "reach/adapters/windows/diagnostics_log.h"
#include "reach/adapters/windows/frame_wait.h"
#include "reach/adapters/windows/install_check.h"
#include "reach/protocol/reach_service_protocol.h"

static const wchar_t *REACH_LAUNCH_ARG = L"--launch";
static const int REACH_EXIT_FAILURE = 1;
static const int REACH_EXIT_DEVICE_LOST = 3;

static int32_t reach_arg_present(int argc, wchar_t **argv, const wchar_t *arg)
{
    if (arg == nullptr)
    {
        return 0;
    }

    for (int index = 0; index < argc; ++index)
    {
        if (lstrcmpiW(argv[index], arg) == 0)
        {
            return 1;
        }
    }

    return 0;
}

static HANDLE reach_acquire_instance_mutex(void)
{
    HANDLE mutex = CreateMutexW(nullptr, TRUE, REACH_SHELL_INSTANCE_MUTEX_NAME);
    if (mutex == nullptr)
    {
        return nullptr;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(mutex);
        return nullptr;
    }

    return mutex;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance, PWSTR command_line,
                    int show_command)
{
    (void)instance;
    (void)previous_instance;
    (void)command_line;
    (void)show_command;

    reach_diagnostics_log_install("reach");

    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int32_t launch_allowed = argv != nullptr && reach_arg_present(argc, argv, REACH_LAUNCH_ARG);
    if (argv != nullptr)
    {
        LocalFree(argv);
    }

    if (!launch_allowed)
    {
        MessageBoxW(nullptr,
                    L"Reach cannot be launched directly. Run reachctl --start, or run "
                    L"reachctl --install first if Reach is not installed.",
                    L"Reach", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    if (!reach_windows_reach_is_installed())
    {
        reach_log_info("launch refused reason=not_installed exit=0");
        MessageBoxW(nullptr, L"Please install Reach first.", L"Reach", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    HANDLE instance_mutex = reach_acquire_instance_mutex();
    if (instance_mutex == nullptr)
    {
        return 0;
    }

    reach_workload_session workload("reach");
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        CloseHandle(instance_mutex);
        return REACH_EXIT_FAILURE;
    }

    int exit_code = 0;
    reach_app *app = nullptr;
    reach_host_desc desc = {};

    reach_result result = reach_app_create(&desc, &app);
    if (result == REACH_OK)
    {
        result = reach_app_start(app);
        if (result == REACH_OK)
        {
            (void)reach_app_update(app, 0.0);
        }
    }

    if (result != REACH_OK)
    {
        if (app != nullptr)
        {
            reach_app_destroy(app);
        }
        CoUninitialize();
        CloseHandle(instance_mutex);
        return result == REACH_DEVICE_LOST ? REACH_EXIT_DEVICE_LOST : REACH_EXIT_FAILURE;
    }

    MSG message = {};
    LARGE_INTEGER frequency = {};
    LARGE_INTEGER previous_counter = {};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&previous_counter);

    reach_frame_clock frame_clock = {};
    void *frame_timer = reach_windows_frame_timer_create();
    const reach_render_device_port *device_port = reach_app_render_device(app);
    void *device_wait_handles[1] = {device_port->ops.wait_handle(device_port->device)};
    uint32_t device_wait_count = device_wait_handles[0] != nullptr ? 1 : 0;
    int running = 1;
    while (running)
    {
        reach_render_device_status device_status = device_port->ops.status(device_port->device);
        int32_t device_lost = device_status.state != REACH_RENDER_DEVICE_READY;
        int32_t needed_frame_before_wait = !device_lost && reach_app_needs_frame(app);
        LARGE_INTEGER wait_counter = {};
        QueryPerformanceCounter(&wait_counter);
        double wait_seconds = (double)wait_counter.QuadPart / (double)frequency.QuadPart;
        uint32_t frame_wait_ms = reach_frame_clock_wait_ms(
            &frame_clock, wait_seconds, reach_app_frame_rate_hz(app), needed_frame_before_wait);
        DWORD wait_ms = INFINITE;
        if (device_lost)
        {
            wait_ms = device_status.retry_wait_ms;
        }
        else if (needed_frame_before_wait)
        {
            wait_ms = frame_wait_ms;
        }
        else if (app != nullptr)
        {
            uint32_t idle_wait = reach_app_idle_wait_ms(app);
            wait_ms = idle_wait == REACH_APP_WAIT_FOREVER ? INFINITE : (DWORD)idle_wait;
        }
        DWORD wait_result = reach_windows_frame_wait(
            frame_timer, device_wait_handles, device_wait_count, wait_ms,
            needed_frame_before_wait ? frame_clock.next_frame_seconds : -1.0);
        int32_t device_signalled = device_wait_count != 0 && wait_result == WAIT_OBJECT_0;
        reach_workload_add(REACH_WORKLOAD_WAKEUPS, 1);

        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                running = 0;
                exit_code = (int)message.wParam;
                break;
            }

            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (app != nullptr)
        {
            if (reach_app_has_pending_events(app))
            {
                (void)reach_app_dispatch_events(app);
            }

            if (device_signalled || device_lost)
            {
                if (reach_app_recover_render_device(app) == REACH_ERROR)
                {
                    running = 0;
                    exit_code = REACH_EXIT_DEVICE_LOST;
                    reach_log_error("render device recovery failed exit=3");
                    break;
                }
                device_lost =
                    device_port->ops.status(device_port->device).state != REACH_RENDER_DEVICE_READY;
            }

            LARGE_INTEGER current_counter = {};
            QueryPerformanceCounter(&current_counter);

            int32_t needs_frame_after_messages = !device_lost && reach_app_needs_frame(app);
            if (needs_frame_after_messages)
            {
                double delta_seconds =
                    needed_frame_before_wait && frequency.QuadPart > 0
                        ? (double)(current_counter.QuadPart - previous_counter.QuadPart) /
                              (double)frequency.QuadPart
                        : 0.0;

                previous_counter = current_counter;

                reach_result update_result = reach_app_update(app, delta_seconds);
                reach_frame_clock_presented(&frame_clock, (double)current_counter.QuadPart /
                                                              (double)frequency.QuadPart);
                if (update_result != REACH_OK && update_result != REACH_DEVICE_LOST)
                {
                    running = 0;
                    exit_code = REACH_EXIT_FAILURE;
                    break;
                }
            }
            else
            {
                previous_counter = current_counter;
            }
        }
    }

    if (app != nullptr)
    {
        (void)reach_app_stop(app);
        reach_app_destroy(app);
    }
    reach_windows_frame_timer_destroy(frame_timer);
    CoUninitialize();
    CloseHandle(instance_mutex);
    return exit_code;
}
