#include "reach/platform/windows_adapters.h"

#include <windows.h>

#include <new>

struct reach_window_manager {
    HWND foreground;
};

static reach_result reach_window_manager_refresh(reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    if (manager == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    manager->foreground = GetForegroundWindow();
    return REACH_OK;
}

static reach_result reach_window_manager_snap(reach_window_manager *manager, uintptr_t window_id, reach_split_mode mode)
{
    REACH_ASSERT(manager != nullptr);
    REACH_ASSERT(window_id != 0);
    // Implement using monitor work area and SetWindowPos; keep snap policy outside the core.
    (void)manager;
    (void)window_id;
    (void)mode;
    return REACH_NOT_IMPLEMENTED;
}

static uintptr_t reach_window_manager_foreground(const reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    return reinterpret_cast<uintptr_t>(manager->foreground);
}

static int32_t reach_window_manager_foreground_is_maximized(const reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    return manager->foreground != nullptr && IsZoomed(manager->foreground);
}

static void reach_window_manager_destroy(reach_window_manager *manager)
{
    delete manager;
}

reach_result reach_windows_create_window_manager(reach_window_manager_port *out_port)
{
    REACH_ASSERT(out_port != nullptr);
    if (out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_window_manager *manager = new (std::nothrow) reach_window_manager();
    if (manager == nullptr) {
        return REACH_ERROR;
    }

    manager->foreground = GetForegroundWindow();
    out_port->manager = manager;
    out_port->ops.refresh = reach_window_manager_refresh;
    out_port->ops.snap = reach_window_manager_snap;
    out_port->ops.foreground = reach_window_manager_foreground;
    out_port->ops.foreground_is_maximized = reach_window_manager_foreground_is_maximized;
    out_port->ops.destroy = reach_window_manager_destroy;
    return REACH_OK;
}
