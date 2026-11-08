#include "reach/adapters/windows/install_check.h"

#include "windows_adapters_internal.h"

#include <windows.h>
#include <shlwapi.h>

#include <wchar.h>

static int32_t reach_install_file_exists(const wchar_t *path)
{
    DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
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

    return 1;
}
