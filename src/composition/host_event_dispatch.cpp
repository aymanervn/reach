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

static int32_t reach_host_hotspot_has_pending_events(const reach_screen_hotspot_port *hotspot)
{
    return hotspot != nullptr && hotspot->ops.has_pending_events != nullptr &&
           hotspot->ops.has_pending_events(hotspot->hotspot);
}

static void reach_host_dispatch_hotspot_events(reach_screen_hotspot_port *hotspot)
{
    if (hotspot == nullptr || hotspot->ops.dispatch_events == nullptr)
    {
        return;
    }
    if (hotspot->ops.has_pending_events != nullptr &&
        !hotspot->ops.has_pending_events(hotspot->hotspot))
    {
        return;
    }
    (void)hotspot->ops.dispatch_events(hotspot->hotspot);
}

int32_t reach_host_has_pending_events(const reach_host *host)
{
    if (host == nullptr)
    {
        return 0;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        if (reach_host_surface_has_pending_events(host->surface_descs[index].surface))
        {
            return 1;
        }
    }

    return reach_host_hotspot_has_pending_events(&host->dock_reveal_edge) ||
           reach_host_hotspot_has_pending_events(&host->top_bar_reveal_edge) ||
           reach_host_hotspot_has_pending_events(&host->stage_reveal_corner);
}

reach_result reach_host_dispatch_events(reach_host *host)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_host_dispatch_surface_events(host->surface_descs[index].surface);
    }

    reach_host_dispatch_hotspot_events(&host->dock_reveal_edge);
    reach_host_dispatch_hotspot_events(&host->top_bar_reveal_edge);
    reach_host_dispatch_hotspot_events(&host->stage_reveal_corner);

    host->dirty.events_dispatched_this_cycle = 1;
    return REACH_OK;
}
