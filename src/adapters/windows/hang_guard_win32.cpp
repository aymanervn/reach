#include "reach/adapters/windows/hang_guard.h"

#include "reach/adapters/windows/diagnostics_log.h"
#include "reach/support/hang_history.h"
#include "reach/support/util.h"
#include "crash_dump_internal.h"

#include <windows.h>
#include <shlwapi.h>

#include <stdio.h>

static const DWORD REACH_HANG_GUARD_BUSY_MS = 5000;
static const DWORD REACH_HANG_GUARD_PROBE_MS = 5000;
static const DWORD REACH_HANG_GUARD_EXIT_CODE = 0xE0000004u;
static const UINT REACH_HANG_GUARD_PROBE_MESSAGE = WM_APP + 1;
static const wchar_t *REACH_HANG_GUARD_CLASS = L"ReachHangProbe";
static const wchar_t *REACH_HANG_GUARD_HISTORY_FILE = L"hang_history";

// Odd while the UI thread works between waits; each change starts a new pass.
static volatile LONG reach_hang_guard_pass = 0;
static volatile LONG reach_hang_guard_acknowledged = 0;
static HWND reach_hang_guard_window = nullptr;
static HANDLE reach_hang_guard_stop_event = nullptr;
static HANDLE reach_hang_guard_thread = nullptr;
static DWORD reach_hang_guard_ui_thread_id = 0;

static LRESULT CALLBACK reach_hang_guard_window_proc(HWND hwnd, UINT message, WPARAM wparam,
                                                     LPARAM lparam)
{
    if (message == REACH_HANG_GUARD_PROBE_MESSAGE)
    {
        InterlockedExchange(&reach_hang_guard_acknowledged, (LONG)wparam);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static uint64_t reach_hang_guard_now_ms(void)
{
    FILETIME now = {};
    GetSystemTimeAsFileTime(&now);
    return (((uint64_t)now.dwHighDateTime << 32) | now.dwLowDateTime) / 10000ull;
}

static int32_t reach_hang_guard_allow_restart(void)
{
    wchar_t path[MAX_PATH] = {};
    if (!reach_diagnostics_directory(path, MAX_PATH) ||
        !PathAppendW(path, REACH_HANG_GUARD_HISTORY_FILE))
    {
        return 1;
    }

    HANDLE file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 1;
    }

    reach_hang_history history = {};
    DWORD transferred = 0;
    if (!ReadFile(file, &history, sizeof(history), &transferred, nullptr) ||
        transferred != sizeof(history))
    {
        history = {};
    }

    int32_t restart = reach_hang_history_allow_restart(&history, reach_hang_guard_now_ms());
    if (restart)
    {
        SetFilePointer(file, 0, nullptr, FILE_BEGIN);
        WriteFile(file, &history, sizeof(history), &transferred, nullptr);
        SetEndOfFile(file);
    }
    CloseHandle(file);
    return restart;
}

static void reach_hang_guard_report(void)
{
    int32_t restart = reach_hang_guard_allow_restart();
    wchar_t name[96] = {};
    int written = reach_crash_dump_write_snapshot(name, 96);

    char message[256] = {};
    snprintf(message, sizeof(message), "hang thread=%lu action=%s dump=%ls",
             (unsigned long)reach_hang_guard_ui_thread_id, restart ? "restart" : "dump_only",
             written ? name : L"none");
    reach_log_error(message);

    if (restart)
    {
        TerminateProcess(GetCurrentProcess(), REACH_HANG_GUARD_EXIT_CODE);
    }
}

static int32_t reach_hang_guard_stopping(DWORD wait_ms)
{
    return WaitForSingleObject(reach_hang_guard_stop_event, wait_ms) != WAIT_TIMEOUT;
}

// Wakes once when an idle UI thread starts a pass, then at most once per busy interval while
// passes keep running. A pass that outlives the interval gets a posted probe: modal loops still
// retrieve it, while a thread blocked in a wait or a cross-process SendMessage does not.
// Wait timeouts exclude time the machine spends asleep.
static DWORD WINAPI reach_hang_guard_thread_main(void *)
{
    LONG probe = 0;
    LONG reported_pass = 0;
    for (;;)
    {
        LONG pass = ReadAcquire(&reach_hang_guard_pass);
        if ((pass & 1) == 0)
        {
            WaitOnAddress(&reach_hang_guard_pass, &pass, sizeof(pass), INFINITE);
            if (reach_hang_guard_stopping(0))
            {
                return 0;
            }
            pass = ReadAcquire(&reach_hang_guard_pass);
        }

        if (reach_hang_guard_stopping(REACH_HANG_GUARD_BUSY_MS))
        {
            return 0;
        }
        if ((pass & 1) == 0 || pass == reported_pass || ReadAcquire(&reach_hang_guard_pass) != pass)
        {
            continue;
        }

        probe += 1;
        if (!PostMessageW(reach_hang_guard_window, REACH_HANG_GUARD_PROBE_MESSAGE, (WPARAM)probe,
                          0))
        {
            continue;
        }
        if (reach_hang_guard_stopping(REACH_HANG_GUARD_PROBE_MS))
        {
            return 0;
        }
        if (ReadAcquire(&reach_hang_guard_acknowledged) == probe ||
            ReadAcquire(&reach_hang_guard_pass) != pass || IsDebuggerPresent())
        {
            continue;
        }

        reported_pass = pass;
        reach_hang_guard_report();
    }
}

void reach_hang_guard_start(void)
{
    REACH_ASSERT(reach_hang_guard_thread == nullptr);
    reach_hang_guard_ui_thread_id = GetCurrentThreadId();

    WNDCLASSW window_class = {};
    window_class.lpfnWndProc = reach_hang_guard_window_proc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.lpszClassName = REACH_HANG_GUARD_CLASS;
    RegisterClassW(&window_class);
    reach_hang_guard_window =
        CreateWindowExW(0, REACH_HANG_GUARD_CLASS, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                        window_class.hInstance, nullptr);
    reach_hang_guard_stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (reach_hang_guard_window == nullptr || reach_hang_guard_stop_event == nullptr)
    {
        reach_log_error("hang guard not started");
        return;
    }

    reach_hang_guard_thread =
        CreateThread(nullptr, 0, reach_hang_guard_thread_main, nullptr, 0, nullptr);
    if (reach_hang_guard_thread == nullptr)
    {
        reach_log_error("hang guard not started");
    }
}

void reach_hang_guard_busy(void)
{
    REACH_ASSERT(reach_hang_guard_ui_thread_id == 0 ||
                 GetCurrentThreadId() == reach_hang_guard_ui_thread_id);
    LONG pass = reach_hang_guard_pass;
    if ((pass & 1) == 0)
    {
        WriteRelease(&reach_hang_guard_pass, pass + 1);
        WakeByAddressSingle((void *)&reach_hang_guard_pass);
    }
}

void reach_hang_guard_idle(void)
{
    REACH_ASSERT(reach_hang_guard_ui_thread_id == 0 ||
                 GetCurrentThreadId() == reach_hang_guard_ui_thread_id);
    LONG pass = reach_hang_guard_pass;
    if ((pass & 1) != 0)
    {
        WriteRelease(&reach_hang_guard_pass, pass + 1);
    }
}

void reach_hang_guard_stop(void)
{
    if (reach_hang_guard_thread != nullptr)
    {
        SetEvent(reach_hang_guard_stop_event);
        InterlockedAdd(&reach_hang_guard_pass, 2);
        WakeByAddressAll((void *)&reach_hang_guard_pass);
        WaitForSingleObject(reach_hang_guard_thread, INFINITE);
        CloseHandle(reach_hang_guard_thread);
        reach_hang_guard_thread = nullptr;
    }
    if (reach_hang_guard_stop_event != nullptr)
    {
        CloseHandle(reach_hang_guard_stop_event);
        reach_hang_guard_stop_event = nullptr;
    }
    if (reach_hang_guard_window != nullptr)
    {
        DestroyWindow(reach_hang_guard_window);
        reach_hang_guard_window = nullptr;
    }
}
