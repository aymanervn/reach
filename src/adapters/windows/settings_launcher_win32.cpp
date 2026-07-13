#include "windows_adapters_internal.h"

#include "reach/ports/settings_launcher.h"

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>

#include <new>
#include <corecrt_wstdio.h>

struct reach_settings_launcher
{
    int32_t reserved;
};

static reach_result reach_settings_launcher_resolve(reach_settings_launcher *launcher,
                                                    uint16_t *out_path, size_t path_capacity,
                                                    uint16_t *out_arguments,
                                                    size_t arguments_capacity)
{
    (void)launcher;
    if (out_path == nullptr || path_capacity == 0 || out_arguments == nullptr ||
        arguments_capacity == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    wchar_t path[MAX_PATH] = {};
    DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH || !PathRemoveFileSpecW(path) ||
        !PathAppendW(path, L"reachSetting.exe"))
    {
        return REACH_ERROR;
    }

    wchar_t parameters[64] = {};
    swprintf_s(parameters, L"--shell-pid %lu", GetCurrentProcessId());

    (void)reach_copy_utf16(out_path, path_capacity, reinterpret_cast<const uint16_t *>(path));
    (void)reach_copy_utf16(out_arguments, arguments_capacity,
                           reinterpret_cast<const uint16_t *>(parameters));
    return REACH_OK;
}

static void reach_settings_launcher_destroy(reach_settings_launcher *launcher)
{
    delete launcher;
}

reach_result reach_windows_create_settings_launcher(reach_settings_launcher_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_settings_launcher *launcher = new (std::nothrow) reach_settings_launcher();
    if (launcher == nullptr)
    {
        return REACH_ERROR;
    }

    out_port->launcher = launcher;
    out_port->ops.resolve = reach_settings_launcher_resolve;
    out_port->ops.destroy = reach_settings_launcher_destroy;
    return REACH_OK;
}
