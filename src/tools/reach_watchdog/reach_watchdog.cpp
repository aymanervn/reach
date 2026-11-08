#include <windows.h>
#include <shlwapi.h>
#include <tlhelp32.h>

#include <cstdarg>
#include <cstdio>
#include <cwchar>

#include "fallback_notice.h"
#include "reach/adapters/windows/diagnostics_log.h"
#include "reach/support/crash_loop.h"
#include "reach/support/util.h"

static const wchar_t *REACH_PROCESS_NAME = L"reach.exe";

static int reach_watchdog_sibling_path(const wchar_t *file_name, wchar_t *out_path, DWORD out_count)
{
    if (file_name == nullptr || file_name[0] == 0 || out_path == nullptr || out_count == 0)
    {
        return 0;
    }

    DWORD length = GetModuleFileNameW(nullptr, out_path, out_count);
    if (length == 0 || length >= out_count)
    {
        return 0;
    }

    if (!PathRemoveFileSpecW(out_path))
    {
        return 0;
    }

    if (!PathAppendW(out_path, file_name))
    {
        return 0;
    }

    return 1;
}

static int reach_watchdog_working_directory(const wchar_t *path, wchar_t *out_directory,
                                            DWORD out_count)
{
    if (path == nullptr || path[0] == 0 || out_directory == nullptr || out_count == 0)
    {
        return 0;
    }

    wcscpy_s(out_directory, out_count, path);
    if (!PathRemoveFileSpecW(out_directory))
    {
        out_directory[0] = 0;
        return 0;
    }

    return 1;
}

static int reach_watchdog_start_process(const wchar_t *path, const wchar_t *arguments,
                                        const wchar_t *working_directory)
{
    if (path == nullptr || path[0] == 0)
    {
        return 0;
    }

    wchar_t command_line[1024] = {};
    if (arguments != nullptr && arguments[0] != 0)
    {
        swprintf_s(command_line, L"\"%ls\" %ls", path, arguments);
    }
    else
    {
        swprintf_s(command_line, L"\"%ls\"", path);
    }

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);

    PROCESS_INFORMATION process = {};
    BOOL ok = CreateProcessW(nullptr, command_line, nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                             nullptr, working_directory, &startup, &process);

    if (!ok)
    {
        return 0;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 1;
}

static int reach_watchdog_start_process_with_handle(const wchar_t *path, const wchar_t *arguments,
                                                    const wchar_t *working_directory,
                                                    HANDLE *out_process)
{
    if (out_process != nullptr)
    {
        *out_process = nullptr;
    }

    if (path == nullptr || path[0] == 0 || out_process == nullptr)
    {
        return 0;
    }

    wchar_t command_line[1024] = {};
    if (arguments != nullptr && arguments[0] != 0)
    {
        swprintf_s(command_line, L"\"%ls\" %ls", path, arguments);
    }
    else
    {
        swprintf_s(command_line, L"\"%ls\"", path);
    }

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);

    PROCESS_INFORMATION process = {};
    BOOL ok = CreateProcessW(nullptr, command_line, nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                             nullptr, working_directory, &startup, &process);

    if (!ok)
    {
        return 0;
    }

    CloseHandle(process.hThread);
    *out_process = process.hProcess;
    return 1;
}

static int reach_watchdog_open_process_by_name(const wchar_t *process_name, HANDLE *out_process)
{
    if (out_process != nullptr)
    {
        *out_process = nullptr;
    }

    if (process_name == nullptr || process_name[0] == 0 || out_process == nullptr)
    {
        return 0;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    DWORD current_process_id = GetCurrentProcessId();

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);

    int found = 0;
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (entry.th32ProcessID == current_process_id)
            {
                continue;
            }

            if (lstrcmpiW(entry.szExeFile, process_name) != 0)
            {
                continue;
            }

            HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                         entry.th32ProcessID);

            if (process != nullptr)
            {
                if (WaitForSingleObject(process, 0) == WAIT_TIMEOUT)
                {
                    *out_process = process;
                    found = 1;
                    break;
                }

                CloseHandle(process);
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return found;
}

static void reach_watchdog_terminate_processes_by_name(const wchar_t *process_name)
{
    if (process_name == nullptr || process_name[0] == 0)
    {
        return;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return;
    }

    DWORD current_process_id = GetCurrentProcessId();

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (entry.th32ProcessID == current_process_id)
            {
                continue;
            }

            if (lstrcmpiW(entry.szExeFile, process_name) != 0)
            {
                continue;
            }

            HANDLE process =
                OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, entry.th32ProcessID);

            if (process == nullptr)
            {
                continue;
            }

            if (TerminateProcess(process, 1))
            {
                (void)WaitForSingleObject(process, 3000);
            }

            CloseHandle(process);
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
}

static void reach_watchdog_reset_direct_fallback(void)
{
    HKEY key = nullptr;
    LONG status = RegCreateKeyExW(HKEY_CURRENT_USER,
                                  L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon", 0,
                                  nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);

    if (status == ERROR_SUCCESS)
    {
        const wchar_t *explorer = L"explorer.exe";
        (void)RegSetValueExW(key, L"Shell", 0, REG_SZ, reinterpret_cast<const BYTE *>(explorer),
                             (DWORD)((wcslen(explorer) + 1) * sizeof(wchar_t)));
        RegCloseKey(key);
    }

    reach_watchdog_terminate_processes_by_name(REACH_PROCESS_NAME);

    wchar_t windows_dir[MAX_PATH] = {};
    UINT windows_length = GetWindowsDirectoryW(windows_dir, MAX_PATH);
    if (windows_length > 0 && windows_length < MAX_PATH)
    {
        wchar_t explorer_path[MAX_PATH] = {};
        wcscpy_s(explorer_path, windows_dir);
        if (PathAppendW(explorer_path, L"explorer.exe"))
        {
            (void)reach_watchdog_start_process(explorer_path, nullptr, windows_dir);
        }
    }
}

static void reach_watchdog_log(const char *format, ...)
{
    char message[256] = {};
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    reach_log_info(message);
}

static void reach_watchdog_fall_back(void)
{
    int notice_ready = reach_fallback_notice_prepare();
    reach_watchdog_reset_direct_fallback();
    reach_watchdog_log("fallback done shell=explorer.exe");
    if (notice_ready)
    {
        reach_fallback_notice_wait();
    }
}

static unsigned long long reach_watchdog_filetime_value(const FILETIME &time)
{
    return ((unsigned long long)time.dwHighDateTime << 32) | time.dwLowDateTime;
}

static unsigned long long reach_watchdog_uptime_seconds(HANDLE process)
{
    FILETIME created = {};
    FILETIME exited = {};
    FILETIME kernel = {};
    FILETIME user = {};
    if (!GetProcessTimes(process, &created, &exited, &kernel, &user))
    {
        return 0;
    }

    unsigned long long start = reach_watchdog_filetime_value(created);
    unsigned long long end = reach_watchdog_filetime_value(exited);
    return end > start ? (end - start) / 10000000ull : 0;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    reach_diagnostics_log_install("watchdog");

    wchar_t reach_path[MAX_PATH] = {};
    if (!reach_watchdog_sibling_path(REACH_PROCESS_NAME, reach_path, MAX_PATH))
    {
        reach_watchdog_log("fallback reason=reach_path_unresolved action=explorer");
        reach_watchdog_fall_back();
        return 1;
    }

    wchar_t working_directory[MAX_PATH] = {};
    if (!reach_watchdog_working_directory(reach_path, working_directory, MAX_PATH))
    {
        working_directory[0] = 0;
    }

    reach_crash_loop crash_loop = {};

    for (;;)
    {
        if (GetSystemMetrics(SM_SHUTTINGDOWN) != 0)
        {
            reach_watchdog_log("stop reason=session_ending");
            return 0;
        }

        HANDLE reach_process = nullptr;
        int start_failed = 0;
        DWORD start_error = 0;
        DWORD exit_code = 0;
        unsigned long long uptime_seconds = 0;

        if (reach_watchdog_open_process_by_name(REACH_PROCESS_NAME, &reach_process))
        {
            reach_watchdog_log("attach pid=%lu", (unsigned long)GetProcessId(reach_process));
        }
        else if (reach_watchdog_start_process_with_handle(
                     reach_path, L"--launch",
                     working_directory[0] != 0 ? working_directory : nullptr, &reach_process))
        {
            reach_watchdog_log("start pid=%lu", (unsigned long)GetProcessId(reach_process));
        }
        else
        {
            start_failed = 1;
            start_error = GetLastError();
        }

        HANDLE survivor = nullptr;
        int survivor_alive = 0;

        if (!start_failed)
        {
            if (WaitForSingleObject(reach_process, INFINITE) != WAIT_OBJECT_0)
            {
                reach_watchdog_log("wait failed error=%lu action=fallback",
                                   (unsigned long)GetLastError());
                CloseHandle(reach_process);
                reach_watchdog_fall_back();
                return 1;
            }

            exit_code = 1;
            (void)GetExitCodeProcess(reach_process, &exit_code);
            uptime_seconds = reach_watchdog_uptime_seconds(reach_process);
            CloseHandle(reach_process);

            if (exit_code == 0)
            {
                survivor_alive = reach_watchdog_open_process_by_name(REACH_PROCESS_NAME, &survivor);
            }
        }

        int shutting_down = GetSystemMetrics(SM_SHUTTINGDOWN) != 0;
        reach_exit_disposition disposition = reach_crash_loop_classify_exit(
            start_failed ? 1 : exit_code, shutting_down, survivor_alive);

        if (disposition == REACH_EXIT_DISPOSITION_ATTACH)
        {
            reach_watchdog_log("attach after exit 0 pid=%lu",
                               (unsigned long)GetProcessId(survivor));
            CloseHandle(survivor);
            continue;
        }

        if (survivor != nullptr)
        {
            CloseHandle(survivor);
        }

        if (disposition == REACH_EXIT_DISPOSITION_RETURN)
        {
            if (shutting_down)
            {
                reach_watchdog_log(
                    "exit code=0x%08lX uptime=%llus action=stop reason=session_ending",
                    (unsigned long)exit_code, uptime_seconds);
            }
            else
            {
                reach_watchdog_log("exit code=0x00000000 uptime=%llus action=stop intentional=1",
                                   uptime_seconds);
            }
            return 0;
        }

        reach_crash_loop_decision decision =
            reach_crash_loop_record_exit(&crash_loop, GetTickCount64());
        const char *action =
            decision.action == REACH_CRASH_LOOP_FALLBACK ? "fallback_explorer" : "restart";

        if (start_failed)
        {
            reach_watchdog_log("start_failed error=%lu window=%u/%u action=%s delay_ms=%u",
                               (unsigned long)start_error, (unsigned)decision.window_count,
                               (unsigned)REACH_CRASH_LOOP_FALLBACK_COUNT, action,
                               (unsigned)decision.delay_ms);
        }
        else
        {
            reach_watchdog_log(
                "exit code=0x%08lX uptime=%llus window=%u/%u action=%s delay_ms=%u",
                (unsigned long)exit_code, uptime_seconds, (unsigned)decision.window_count,
                (unsigned)REACH_CRASH_LOOP_FALLBACK_COUNT, action, (unsigned)decision.delay_ms);
        }

        if (decision.action == REACH_CRASH_LOOP_FALLBACK)
        {
            reach_watchdog_fall_back();
            return 1;
        }

        if (decision.delay_ms > 0)
        {
            Sleep(decision.delay_ms);
        }
    }
}
