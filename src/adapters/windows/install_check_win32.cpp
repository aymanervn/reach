#include "reach/adapters/windows/install_check.h"

#include "windows_adapters_internal.h"

#include <windows.h>
#include <shlwapi.h>

#include <wchar.h>

static const DWORD REACH_INSTALL_QUERY_WAIT_MS = 15000;

static int32_t reach_install_file_exists(const wchar_t *path)
{
    DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static int32_t reach_install_watchdog_task_registered(void)
{
    wchar_t command_line[] =
        L"\"C:\\Windows\\System32\\schtasks.exe\" /Query /TN \"ReachWatchdog\"";

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);

    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(nullptr, command_line, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        nullptr, &startup, &process))
    {
        return 0;
    }

    CloseHandle(process.hThread);

    DWORD wait_result = WaitForSingleObject(process.hProcess, REACH_INSTALL_QUERY_WAIT_MS);
    DWORD exit_code = 1;
    if (wait_result == WAIT_OBJECT_0)
    {
        (void)GetExitCodeProcess(process.hProcess, &exit_code);
    }

    CloseHandle(process.hProcess);
    return wait_result == WAIT_OBJECT_0 && exit_code == 0;
}

int32_t reach_windows_reach_is_installed(void)
{
    wchar_t reach_exe[MAX_PATH] = {};
    DWORD length = GetModuleFileNameW(nullptr, reach_exe, MAX_PATH);
    if (length == 0 || length >= MAX_PATH || !reach_install_file_exists(reach_exe))
    {
        return 0;
    }

    wchar_t watchdog_exe[MAX_PATH] = {};
    wcscpy_s(watchdog_exe, reach_exe);
    if (!PathRemoveFileSpecW(watchdog_exe) || !PathAppendW(watchdog_exe, L"reachWatchdog.exe") ||
        !reach_install_file_exists(watchdog_exe))
    {
        return 0;
    }

    reach_shell_registration_status shell_status = {};
    if (reach_windows_shell_query_current_user(reinterpret_cast<const uint16_t *>(reach_exe),
                                               &shell_status) != REACH_OK ||
        !shell_status.reach_is_shell)
    {
        return 0;
    }

    return reach_install_watchdog_task_registered();
}
