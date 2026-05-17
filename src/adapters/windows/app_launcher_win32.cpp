#include "reach/platform/windows_adapters.h"

#include <windows.h>
#include <shellapi.h>

#include <new>

struct reach_app_launcher {
    int unused;
};

static reach_result reach_app_launcher_launch(reach_app_launcher *launcher, const reach_app_launch_request *request)
{
    (void)launcher;
    if (request == nullptr || request->path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    SHELLEXECUTEINFOW execute = {};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_DEFAULT;
    execute.lpFile = reinterpret_cast<LPCWSTR>(request->path);
    execute.lpParameters = request->arguments[0] != 0 ? reinterpret_cast<LPCWSTR>(request->arguments) : nullptr;
    execute.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&execute) ? REACH_OK : REACH_ERROR;
}

static void reach_app_launcher_destroy(reach_app_launcher *launcher)
{
    delete launcher;
}

reach_result reach_windows_create_app_launcher(reach_app_launcher_port *out_port)
{
    if (out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_app_launcher *launcher = new (std::nothrow) reach_app_launcher();
    if (launcher == nullptr) {
        return REACH_ERROR;
    }

    out_port->launcher = launcher;
    out_port->ops.launch = reach_app_launcher_launch;
    out_port->ops.destroy = reach_app_launcher_destroy;
    return REACH_OK;
}
