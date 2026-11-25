#include "reach/adapters/windows/crash_dump.h"

#include "crash_dump_internal.h"

#include <windows.h>

#include <wchar.h>

// Windows Error Reporting honors LocalDumps only under HKLM, so this needs an elevated caller.
// It catches the fail-fast crashes that bypass reach_crash_dump_install's filter.
static const wchar_t *REACH_LOCAL_DUMPS_KEY =
    L"SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting\\LocalDumps\\";
static const wchar_t *REACH_LOCAL_DUMPS_EXECUTABLES[] = {L"reach.exe", L"reachSetting.exe",
                                                         L"reachService.exe"};
static const DWORD REACH_LOCAL_DUMPS_CUSTOM_TYPE = 0;
static const DWORD REACH_LOCAL_DUMPS_COUNT = 5;

reach_result reach_crash_dump_register_local_dumps(void)
{
    wchar_t folder[MAX_PATH] = {};
    swprintf_s(folder, L"%%LOCALAPPDATA%%\\Reach\\diagnostics\\%ls", REACH_CRASH_DUMP_DIRECTORY);
    DWORD folder_bytes = (DWORD)((wcslen(folder) + 1) * sizeof(wchar_t));
    DWORD flags = (DWORD)REACH_CRASH_DUMP_TYPE;

    reach_result result = REACH_OK;
    for (const wchar_t *executable : REACH_LOCAL_DUMPS_EXECUTABLES)
    {
        wchar_t path[256] = {};
        swprintf_s(path, L"%ls%ls", REACH_LOCAL_DUMPS_KEY, executable);
        HKEY key = nullptr;
        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key,
                            nullptr) != ERROR_SUCCESS)
        {
            result = REACH_ERROR;
            continue;
        }

        if (RegSetValueExW(key, L"DumpFolder", 0, REG_EXPAND_SZ,
                           reinterpret_cast<const BYTE *>(folder), folder_bytes) != ERROR_SUCCESS ||
            RegSetValueExW(key, L"DumpType", 0, REG_DWORD,
                           reinterpret_cast<const BYTE *>(&REACH_LOCAL_DUMPS_CUSTOM_TYPE),
                           sizeof(DWORD)) != ERROR_SUCCESS ||
            RegSetValueExW(key, L"CustomDumpFlags", 0, REG_DWORD,
                           reinterpret_cast<const BYTE *>(&flags),
                           sizeof(DWORD)) != ERROR_SUCCESS ||
            RegSetValueExW(key, L"DumpCount", 0, REG_DWORD,
                           reinterpret_cast<const BYTE *>(&REACH_LOCAL_DUMPS_COUNT),
                           sizeof(DWORD)) != ERROR_SUCCESS)
        {
            result = REACH_ERROR;
        }
        RegCloseKey(key);
    }
    return result;
}
