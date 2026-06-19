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

static reach_result reach_settings_launcher_open(reach_settings_launcher *launcher)
{
    (void)launcher;

    wchar_t path[MAX_PATH] = {};
    DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH || !PathRemoveFileSpecW(path) ||
        !PathAppendW(path, L"reach-settings.exe"))
    {
        return REACH_ERROR;
    }

    SHELLEXECUTEINFOW execute = {};
    wchar_t parameters[64] = {};
    swprintf_s(parameters, L"--shell-pid %lu", GetCurrentProcessId());
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_NOCLOSEPROCESS;
    execute.lpFile = path;
    execute.lpParameters = parameters;
    execute.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&execute))
    {
        return REACH_ERROR;
    }
    if (execute.hProcess != nullptr)
    {
        CloseHandle(execute.hProcess);
    }
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
    out_port->ops.open = reach_settings_launcher_open;
    out_port->ops.destroy = reach_settings_launcher_destroy;
    return REACH_OK;
}
