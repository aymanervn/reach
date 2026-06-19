#include "reachctl_elevation_helper.h"

#include "../../adapters/windows/window_management/elevation_helper_task_win32.h"

#include <windows.h>
#include <shlwapi.h>

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

reach_result reachctl_install_elevation_helper(const wchar_t *user_id)
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

    return reach_elevation_helper_task_register(helper_path, user_id);
}

reach_result reachctl_unregister_elevation_helper(void)
{
    return reach_elevation_helper_task_unregister();
}
