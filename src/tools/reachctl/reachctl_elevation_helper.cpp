#include "reachctl_elevation_helper.h"

#include "reachctl_common.h"

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <cwchar>

static const wchar_t *REACHCTL_HELPER_TASK_NAME = L"reach-helper";

static reach_result reachctl_elevation_helper_exe(uint16_t *path, DWORD path_count)
{
    if (path == nullptr || path_count == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD length = GetModuleFileNameW(nullptr, reinterpret_cast<wchar_t *>(path), path_count);
    if (length == 0 || length >= path_count)
    {
        return REACH_ERROR;
    }

    wchar_t *path_w = reinterpret_cast<wchar_t *>(path);
    if (!PathRemoveFileSpecW(path_w))
    {
        return REACH_ERROR;
    }

    if (!PathAppendW(path_w, L"reach_elevation_helper.exe"))
    {
        return REACH_ERROR;
    }

    return REACH_OK;
}

reach_result reachctl_install_elevation_helper(void)
{
    uint16_t helper_path_u16[260] = {};
    reach_result path_result = reachctl_elevation_helper_exe(helper_path_u16, 260);
    if (path_result != REACH_OK)
    {
        return path_result;
    }

    const wchar_t *helper_path = reinterpret_cast<const wchar_t *>(helper_path_u16);

    DWORD attributes = GetFileAttributesW(helper_path);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        return REACH_ERROR;
    }

    wchar_t arguments[1024] = {};
    swprintf_s(arguments, L"/Create /F /SC ONLOGON /TN \"%ls\" /TR \"\\\"%ls\\\"\" /RL HIGHEST",
               REACHCTL_HELPER_TASK_NAME, helper_path);

    return reachctl_run_process_wait(L"C:\\Windows\\System32\\schtasks.exe", arguments, nullptr);
}

reach_result reachctl_unregister_elevation_helper(void)
{
    wchar_t arguments[256] = {};
    swprintf_s(arguments, L"/Delete /F /TN \"%ls\"", REACHCTL_HELPER_TASK_NAME);

    return reachctl_run_process_wait(L"C:\\Windows\\System32\\schtasks.exe", arguments, nullptr);
}

reach_result reachctl_start_elevation_helper(void)
{
    wchar_t arguments[256] = {};
    swprintf_s(arguments, L"/Run /TN \"%ls\"", REACHCTL_HELPER_TASK_NAME);

    HINSTANCE result = ShellExecuteW(nullptr, L"open", L"C:\\Windows\\System32\\schtasks.exe",
                                     arguments, nullptr, SW_HIDE);
    return reinterpret_cast<intptr_t>(result) > 32 ? REACH_OK : REACH_ERROR;
}

int32_t reachctl_elevation_helper_is_installed(void)
{
    wchar_t arguments[256] = {};
    swprintf_s(arguments, L"/Query /TN \"%ls\"", REACHCTL_HELPER_TASK_NAME);

    return reachctl_run_process_wait(L"C:\\Windows\\System32\\schtasks.exe", arguments, nullptr) ==
           REACH_OK;
}
