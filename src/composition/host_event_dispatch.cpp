#include "host_internal.h"

static void reach_host_dispatch_surface_events(reach_surface_runtime *surface)
{
    if (surface == nullptr || surface->window.ops.dispatch_events == nullptr)
    {
        return;
    }
    if (surface->window.ops.has_pending_events != nullptr &&
        !surface->window.ops.has_pending_events(surface->window.window))
    {
        return;
    }
    (void)surface->window.ops.dispatch_events(surface->window.window);
}

static int32_t reach_host_surface_has_pending_events(const reach_surface_runtime *surface)
{
    return surface != nullptr && surface->window.ops.has_pending_events != nullptr &&
           surface->window.ops.has_pending_events(surface->window.window);
}

int32_t reach_host_has_pending_events(const reach_host *host)
{
    return host != nullptr &&
           (reach_host_surface_has_pending_events(&host->launcher) ||
            reach_host_surface_has_pending_events(&host->dock) ||
            reach_host_surface_has_pending_events(&host->tray) ||
            reach_host_surface_has_pending_events(&host->switcher) ||
            reach_host_surface_has_pending_events(&host->context_menu) ||
            reach_host_surface_has_pending_events(&host->quick_settings) ||
            reach_host_surface_has_pending_events(&host->clipboard_surface) ||
            (host->dock_reveal_edge.ops.has_pending_events != nullptr &&
             host->dock_reveal_edge.ops.has_pending_events(host->dock_reveal_edge.edge)));
}

reach_result reach_host_dispatch_events(reach_host *host)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_host_dispatch_surface_events(&host->launcher);
    reach_host_dispatch_surface_events(&host->dock);
    reach_host_dispatch_surface_events(&host->tray);
    reach_host_dispatch_surface_events(&host->switcher);
    reach_host_dispatch_surface_events(&host->context_menu);
    reach_host_dispatch_surface_events(&host->quick_settings);
    reach_host_dispatch_surface_events(&host->clipboard_surface);
    if (host->dock_reveal_edge.ops.dispatch_events != nullptr &&
        (host->dock_reveal_edge.ops.has_pending_events == nullptr ||
         host->dock_reveal_edge.ops.has_pending_events(host->dock_reveal_edge.edge)))
    {
        (void)host->dock_reveal_edge.ops.dispatch_events(host->dock_reveal_edge.edge);
    }
    host->dirty.events_dispatched_this_cycle = 1;
    return REACH_OK;
}
