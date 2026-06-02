#include "windows_adapters_internal.h"

#include "reach/ports/explorer_service.h"

#include <windows.h>
#include <shellapi.h>

#include <new>

struct reach_explorer_service
{
    int unused;
};

static reach_result reach_explorer_execute(const wchar_t *target)
{
    if (target == nullptr || target[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOASYNC;
    info.lpFile = target;
    info.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&info) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_explorer_open_default(reach_explorer_service *service)
{
    REACH_ASSERT(service != nullptr);
    if (service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    return reach_explorer_execute(L"shell:MyComputerFolder");
}

static reach_result reach_explorer_open_path(reach_explorer_service *service, const uint16_t *path)
{
    REACH_ASSERT(service != nullptr);
    REACH_ASSERT(path != nullptr);
    if (service == nullptr || path == nullptr || path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    return reach_explorer_execute(reinterpret_cast<const wchar_t *>(path));
}

static reach_result reach_explorer_open_shell_location(reach_explorer_service *service,
                                                       const uint16_t *shell_location)
{
    REACH_ASSERT(service != nullptr);
    REACH_ASSERT(shell_location != nullptr);
    if (service == nullptr || shell_location == nullptr || shell_location[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    return reach_explorer_execute(reinterpret_cast<const wchar_t *>(shell_location));
}

static int32_t reach_explorer_path_exists(reach_explorer_service *service, const uint16_t *path)
{
    if (service == nullptr || path == nullptr || path[0] == 0)
    {
        return 0;
    }

    DWORD attributes = GetFileAttributesW(reinterpret_cast<const wchar_t *>(path));
    return attributes != INVALID_FILE_ATTRIBUTES;
}

static void reach_explorer_destroy(reach_explorer_service *service)
{
    delete service;
}

reach_result reach_windows_create_explorer_service(reach_explorer_service_port *out_port)
{
    REACH_ASSERT(out_port != nullptr);
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_explorer_service *service = new (std::nothrow) reach_explorer_service();
    if (service == nullptr)
    {
        return REACH_ERROR;
    }

    out_port->service = service;
    out_port->ops.open_default = reach_explorer_open_default;
    out_port->ops.open_path = reach_explorer_open_path;
    out_port->ops.open_shell_location = reach_explorer_open_shell_location;
    out_port->ops.path_exists = reach_explorer_path_exists;
    out_port->ops.destroy = reach_explorer_destroy;
    return REACH_OK;
}
