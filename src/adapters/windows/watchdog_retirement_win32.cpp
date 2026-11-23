#include "reach/adapters/windows/watchdog_retirement.h"

#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <tlhelp32.h>

#include <cwchar>

static const wchar_t *REACH_WATCHDOG_TASK_NAME = L"ReachWatchdog";
static const wchar_t *REACH_WATCHDOG_EXE = L"reachWatchdog.exe";
static const DWORD REACH_WATCHDOG_EXIT_WAIT_MS = 3000;

static DWORD reach_watchdog_run_schtasks(const wchar_t *verb_arguments)
{
    wchar_t system_dir[MAX_PATH] = {};
    UINT length = GetSystemDirectoryW(system_dir, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return (DWORD)-1;
    }

    wchar_t command[MAX_PATH + 128] = {};
    swprintf_s(command, L"\"%ls\\schtasks.exe\" %ls /TN \"%ls\"", system_dir, verb_arguments,
               REACH_WATCHDOG_TASK_NAME);

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(nullptr, command, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        nullptr, &startup, &process))
    {
        return (DWORD)-1;
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = (DWORD)-1;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exit_code;
}

int32_t reach_windows_watchdog_task_present(void)
{
    return reach_watchdog_run_schtasks(L"/Query") == 0;
}

static void reach_watchdog_terminate_processes(void)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return;
    }

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    for (BOOL more = Process32FirstW(snapshot, &entry); more;
         more = Process32NextW(snapshot, &entry))
    {
        if (lstrcmpiW(entry.szExeFile, REACH_WATCHDOG_EXE) != 0)
        {
            continue;
        }

        HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, entry.th32ProcessID);
        if (process != nullptr)
        {
            if (TerminateProcess(process, 0))
            {
                WaitForSingleObject(process, REACH_WATCHDOG_EXIT_WAIT_MS);
            }
            CloseHandle(process);
        }
    }

    CloseHandle(snapshot);
}

reach_result reach_windows_delete_watchdog_task(void)
{
    if (!reach_windows_watchdog_task_present())
    {
        return REACH_OK;
    }
    // Deleting without /End leaves a running watchdog alone, so a Reach inside its job survives.
    return reach_watchdog_run_schtasks(L"/Delete /F") == 0 ? REACH_OK : REACH_ERROR;
}

static int32_t reach_watchdog_file_exists(const wchar_t *path)
{
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static void reach_watchdog_request_task_delete(const wchar_t *install_dir)
{
    wchar_t elevate[MAX_PATH] = {};
    if (wcscpy_s(elevate, install_dir) != 0 || !PathAppendW(elevate, L"reachElevate.exe"))
    {
        return;
    }

    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    info.lpVerb = L"runas";
    info.lpFile = elevate;
    info.lpParameters = L"retire-watchdog";
    info.nShow = SW_HIDE;
    if (ShellExecuteExW(&info) && info.hProcess != nullptr)
    {
        WaitForSingleObject(info.hProcess, INFINITE);
        DWORD exit_code = 1;
        GetExitCodeProcess(info.hProcess, &exit_code);
        CloseHandle(info.hProcess);
        if (exit_code == 0)
        {
            reach_log_info("watchdog task deleted");
        }
        else
        {
            reach_log_error("watchdog task delete failed");
        }
    }
    else if (GetLastError() == ERROR_CANCELLED)
    {
        reach_log_info("watchdog task delete declined");
    }
    else
    {
        reach_log_error("watchdog task delete could not start reachElevate");
    }
    if (SUCCEEDED(com))
    {
        CoUninitialize();
    }
}

static DWORD WINAPI reach_watchdog_cleanup_proc(void *)
{
    wchar_t install_dir[MAX_PATH] = {};
    DWORD length = GetModuleFileNameW(nullptr, install_dir, MAX_PATH);
    if (length == 0 || length >= MAX_PATH || !PathRemoveFileSpecW(install_dir))
    {
        return 1;
    }

    wchar_t exe[MAX_PATH] = {};
    wchar_t old_exe[MAX_PATH] = {};
    if (wcscpy_s(exe, install_dir) != 0 || !PathAppendW(exe, REACH_WATCHDOG_EXE) ||
        swprintf_s(old_exe, L"%ls.old", exe) < 0)
    {
        return 1;
    }

    // A running exe can be renamed, so the task loses its target even while the watchdog runs.
    if (reach_watchdog_file_exists(exe) && !MoveFileExW(exe, old_exe, MOVEFILE_REPLACE_EXISTING))
    {
        reach_log_error("watchdog file rename failed");
    }

    // Ending the watchdog ends its Task Scheduler job; a Reach inside that job would end with it.
    BOOL in_job = FALSE;
    if (IsProcessInJob(GetCurrentProcess(), nullptr, &in_job) && !in_job)
    {
        reach_watchdog_terminate_processes();
    }
    if (reach_watchdog_file_exists(old_exe))
    {
        (void)DeleteFileW(old_exe);
    }

    if (reach_windows_watchdog_task_present())
    {
        MessageBoxW(nullptr,
                    L"Some Reach components have been retired. Approve the next Windows prompt "
                    L"to clean them up.",
                    L"Reach", MB_OK | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);
        reach_watchdog_request_task_delete(install_dir);
    }
    return 0;
}

void reach_windows_start_install_cleanup(void)
{
    HANDLE thread = CreateThread(nullptr, 0, reach_watchdog_cleanup_proc, nullptr, 0, nullptr);
    if (thread != nullptr)
    {
        CloseHandle(thread);
    }
}
