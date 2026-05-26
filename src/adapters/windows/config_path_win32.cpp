#include "windows_adapters_internal.h"

#include <windows.h>
#include <shlwapi.h>

reach_result reach_windows_default_config_path(uint16_t *path, uint32_t path_count)
{
    if (path == nullptr || path_count == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD length = GetModuleFileNameW(nullptr, reinterpret_cast<wchar_t *>(path), path_count);
    if (length == 0 || length >= path_count) {
        return REACH_ERROR;
    }

    wchar_t *path_w = reinterpret_cast<wchar_t *>(path);
    if (!PathRemoveFileSpecW(path_w)) {
        return REACH_ERROR;
    }
    if (!PathAppendW(path_w, L"reach.ini")) {
        return REACH_ERROR;
    }
    return REACH_OK;
}
