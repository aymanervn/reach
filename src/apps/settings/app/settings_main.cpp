#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <tlhelp32.h>
#include <stdlib.h>
#include <wchar.h>

#include "settings_app.h"

#include "reach/protocol/reach_service_protocol.h"

static const wchar_t *REACH_SHELL_INSTANCE_MUTEX = REACH_SHELL_INSTANCE_MUTEX_NAME;
static const wchar_t *REACH_SETTINGS_INSTANCE_MUTEX = L"Local\\Reach.Settings.Instance";
static const wchar_t *REACH_SETTINGS_ACTIVATE_EVENT = L"Local\\Reach.Settings.Activate";
static const wchar_t *REACH_SETTINGS_HOST_ERROR = L"Unable to connect to Reach Host.";
static const wchar_t *REACH_SETTINGS_LAUNCH_ERROR =
    L"Launching Reach Settings failed. Please restart Reach.";

static void reach_settings_show_error(const wchar_t *message)
{
    MessageBoxW(nullptr, message, L"Reach Settings", MB_OK | MB_ICONERROR);
}

static int32_t reach_settings_shell_path(wchar_t *path, size_t capacity)
{
    if (path == nullptr || capacity == 0)
    {
        return 0;
    }
    DWORD length = GetModuleFileNameW(nullptr, path, (DWORD)capacity);
    if (length == 0 || length >= capacity)
    {
        return 0;
    }
    wchar_t *slash = wcsrchr(path, L'\\');
    if (slash == nullptr)
    {
        return 0;
    }
    return wcscpy_s(slash + 1, capacity - (size_t)(slash + 1 - path), L"reach.exe") == 0;
}

static HANDLE reach_settings_open_shell_process(DWORD process_id)
{
    if (process_id == 0)
    {
        return nullptr;
    }
    HANDLE process =
        OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr)
    {
        return nullptr;
    }

    wchar_t expected[MAX_PATH] = {};
    wchar_t actual[MAX_PATH] = {};
    DWORD actual_count = MAX_PATH;
    int32_t valid = reach_settings_shell_path(expected, MAX_PATH) &&
                    QueryFullProcessImageNameW(process, 0, actual, &actual_count) &&
                    lstrcmpiW(actual, expected) == 0 &&
                    WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
    if (!valid)
    {
        CloseHandle(process);
        return nullptr;
    }
    return process;
}

static DWORD reach_settings_shell_pid_from_arguments()
{
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    DWORD process_id = 0;
    for (int index = 1; argv != nullptr && index + 1 < argc; ++index)
    {
        if (lstrcmpiW(argv[index], L"--shell-pid") == 0)
        {
            wchar_t *end = nullptr;
            unsigned long value = wcstoul(argv[index + 1], &end, 10);
            if (end != argv[index + 1] && *end == 0)
            {
                process_id = (DWORD)value;
            }
            break;
        }
    }
    if (argv != nullptr)
    {
        LocalFree(argv);
    }
    return process_id;
}

static DWORD reach_settings_find_shell_process()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    DWORD process_id = 0;
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (lstrcmpiW(entry.szExeFile, L"reach.exe") != 0)
            {
                continue;
            }
            HANDLE process = reach_settings_open_shell_process(entry.th32ProcessID);
            if (process == nullptr)
            {
                continue;
            }
            CloseHandle(process);
            process_id = entry.th32ProcessID;
            break;
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return process_id;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance, PWSTR command_line,
                    int show_command)
{
    (void)instance;
    (void)previous_instance;
    (void)command_line;
    (void)show_command;

    HANDLE shell_mutex = OpenMutexW(SYNCHRONIZE, FALSE, REACH_SHELL_INSTANCE_MUTEX);
    if (shell_mutex == nullptr)
    {
        reach_settings_show_error(REACH_SETTINGS_HOST_ERROR);
        return 0;
    }

    DWORD shell_process_id = reach_settings_shell_pid_from_arguments();
    HANDLE shell_process = reach_settings_open_shell_process(shell_process_id);
    if (shell_process == nullptr)
    {
        shell_process_id = reach_settings_find_shell_process();
        shell_process = reach_settings_open_shell_process(shell_process_id);
    }
    CloseHandle(shell_mutex);
    if (shell_process == nullptr)
    {
        reach_settings_show_error(REACH_SETTINGS_HOST_ERROR);
        return 0;
    }

    HANDLE activate_event = CreateEventW(nullptr, FALSE, FALSE, REACH_SETTINGS_ACTIVATE_EVENT);
    if (activate_event == nullptr)
    {
        CloseHandle(shell_process);
        reach_settings_show_error(REACH_SETTINGS_LAUNCH_ERROR);
        return 1;
    }

    HANDLE instance_mutex = CreateMutexW(nullptr, TRUE, REACH_SETTINGS_INSTANCE_MUTEX);
    if (instance_mutex == nullptr)
    {
        CloseHandle(activate_event);
        CloseHandle(shell_process);
        reach_settings_show_error(REACH_SETTINGS_LAUNCH_ERROR);
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        BOOL activated = SetEvent(activate_event);
        CloseHandle(instance_mutex);
        CloseHandle(activate_event);
        CloseHandle(shell_process);
        if (!activated)
        {
            reach_settings_show_error(REACH_SETTINGS_LAUNCH_ERROR);
            return 1;
        }
        return 0;
    }

    (void)SetCurrentProcessExplicitAppUserModelID(L"Reach.Settings");
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        CloseHandle(activate_event);
        CloseHandle(instance_mutex);
        CloseHandle(shell_process);
        reach_settings_show_error(REACH_SETTINGS_LAUNCH_ERROR);
        return 1;
    }

    reach_settings_app *app = nullptr;
    reach_result result = reach_settings_app_create(&app);
    if (result == REACH_OK)
    {
        result = reach_settings_app_start(app);
    }
    if (result != REACH_OK)
    {
        reach_settings_app_destroy(app);
        CoUninitialize();
        CloseHandle(activate_event);
        CloseHandle(instance_mutex);
        CloseHandle(shell_process);
        reach_settings_show_error(REACH_SETTINGS_LAUNCH_ERROR);
        return 1;
    }

    LARGE_INTEGER frequency = {};
    LARGE_INTEGER previous_counter = {};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&previous_counter);

    HANDLE waits[] = {shell_process, activate_event};
    MSG message = {};
    int exit_code = 0;
    while (reach_settings_app_running(app))
    {
        DWORD wait_ms = reach_settings_app_needs_frame(app) ? 16 : INFINITE;
        DWORD wait_result =
            MsgWaitForMultipleObjectsEx(2, waits, wait_ms, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (wait_result == WAIT_OBJECT_0)
        {
            break;
        }
        if (wait_result == WAIT_OBJECT_0 + 1)
        {
            reach_settings_app_activate(app);
        }

        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                exit_code = (int)message.wParam;
                goto exit_loop;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (reach_settings_app_has_pending_events(app))
        {
            (void)reach_settings_app_dispatch_events(app);
        }
        if (reach_settings_app_needs_frame(app))
        {
            LARGE_INTEGER current_counter = {};
            QueryPerformanceCounter(&current_counter);
            double delta_seconds =
                frequency.QuadPart > 0
                    ? (double)(current_counter.QuadPart - previous_counter.QuadPart) /
                          (double)frequency.QuadPart
                    : 0.016;
            previous_counter = current_counter;
            if (delta_seconds > 0.025)
            {
                delta_seconds = 0.025;
            }
            if (reach_settings_app_update(app, delta_seconds) != REACH_OK)
            {
                exit_code = 1;
                break;
            }
        }
    }

exit_loop:
    reach_settings_app_destroy(app);
    CoUninitialize();
    CloseHandle(activate_event);
    CloseHandle(instance_mutex);
    CloseHandle(shell_process);
    return exit_code;
}
