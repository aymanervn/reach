#include "host_internal.h"

static int32_t reach_host_surface_restores_focus(const reach_feature_runtime *runtime)
{
    return runtime != nullptr && runtime->definition != nullptr &&
           runtime->definition->surface.restores_focus_on_close;
}

void reach_host_capture_focus_restore(reach_host *host, reach_surface_id id)
{
    if (host == nullptr || id >= REACH_HOST_SURFACE_COUNT ||
        !reach_host_surface_restores_focus(&host->feature_runtimes[id]))
    {
        return;
    }

    host->focus_restore_pending[id] = 0;
    host->focus_restore_window[id] = reach_host_foreground_window(host);
}

void reach_host_arm_focus_restore(reach_host *host, reach_surface_id id)
{
    if (host != nullptr && id < REACH_HOST_SURFACE_COUNT &&
        reach_host_surface_restores_focus(&host->feature_runtimes[id]))
    {
        host->focus_restore_pending[id] = 1;
    }
}

void reach_host_cancel_focus_restore(reach_host *host, reach_surface_id id)
{
    if (host != nullptr && id < REACH_HOST_SURFACE_COUNT)
    {
        host->focus_restore_pending[id] = 0;
        host->focus_restore_window[id] = 0;
    }
}

void reach_host_flush_focus_restore(reach_host *host, reach_surface_id id)
{
    if (host == nullptr || id >= REACH_HOST_SURFACE_COUNT || !host->focus_restore_pending[id])
    {
        return;
    }

    uintptr_t window = host->focus_restore_window[id];
    host->focus_restore_pending[id] = 0;
    host->focus_restore_window[id] = 0;
    if (window == 0 || host->window_manager.ops.activate == nullptr ||
        reach_host_window_is_minimized(host, window))
    {
        return;
    }

    (void)reach_host_schedule_window_control(host, REACH_WINDOW_CONTROL_ACTIVATE, window);
}
