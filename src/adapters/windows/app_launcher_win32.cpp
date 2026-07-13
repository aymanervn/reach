#include "windows_adapters_internal.h"

#include "reach/ports/app_launcher.h"

#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlwapi.h>

#include <new>

struct reach_app_launcher
{
    int32_t reserved;
};

static reach_result reach_app_launcher_launch(reach_app_launcher *launcher,
                                              const reach_app_launch_request *request)
{
    (void)launcher;
    if (request == nullptr || request->path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    SHELLEXECUTEINFOW execute = {};
    execute.cbSize = sizeof(execute);
    /* ASYNCOK: don't wait for DDE-class launchees; the worker still absorbs
       whatever ShellExecuteEx insists on waiting for. */
    execute.fMask = SEE_MASK_ASYNCOK | SEE_MASK_FLAG_NO_UI;
    execute.lpVerb = request->run_as_admin ? L"runas" : nullptr;
    execute.lpFile = reinterpret_cast<const wchar_t *>(request->path);
    execute.lpParameters = request->arguments[0] != 0
                               ? reinterpret_cast<const wchar_t *>(request->arguments)
                               : nullptr;
    execute.nShow = SW_SHOWNORMAL;

    wchar_t working_directory[MAX_PATH] = {};
    lstrcpynW(working_directory, reinterpret_cast<const wchar_t *>(request->path), MAX_PATH);
    if (PathRemoveFileSpecW(working_directory))
    {
        execute.lpDirectory = working_directory;
    }

    HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    BOOL launched = ShellExecuteExW(&execute);
    if (SUCCEEDED(com_result))
    {
        CoUninitialize();
    }

    return launched ? REACH_OK : REACH_ERROR;
}

static void reach_app_launcher_destroy(reach_app_launcher *launcher)
{
    delete launcher;
}

reach_result reach_windows_create_app_launcher(reach_app_launcher_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_app_launcher *launcher = new (std::nothrow) reach_app_launcher();
    if (launcher == nullptr)
    {
        return REACH_ERROR;
    }

    out_port->launcher = launcher;
    out_port->ops.launch = reach_app_launcher_launch;
    out_port->ops.destroy = reach_app_launcher_destroy;
    return REACH_OK;
}
