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

static int32_t
reach_host_edge_reveal_has_pending_events(const reach_screen_hotspot_port *edge_reveal)
{
    return edge_reveal != nullptr && edge_reveal->ops.has_pending_events != nullptr &&
           edge_reveal->ops.has_pending_events(edge_reveal->hotspot);
}

static void reach_host_dispatch_edge_reveal_events(reach_screen_hotspot_port *edge_reveal)
{
    if (edge_reveal == nullptr || edge_reveal->ops.dispatch_events == nullptr)
    {
        return;
    }
    if (edge_reveal->ops.has_pending_events != nullptr &&
        !edge_reveal->ops.has_pending_events(edge_reveal->hotspot))
    {
        return;
    }
    (void)edge_reveal->ops.dispatch_events(edge_reveal->hotspot);
}

int32_t reach_host_has_pending_events(const reach_host *host)
{
    if (host == nullptr)
    {
        return 0;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        if (reach_host_surface_has_pending_events(host->feature_runtimes[index].surface))
        {
            return 1;
        }
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        if (reach_host_edge_reveal_has_pending_events(&host->edge_reveals[index].port))
        {
            return 1;
        }
    }
    return 0;
}

reach_result reach_host_dispatch_events(reach_host *host)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_host_dispatch_surface_events(host->feature_runtimes[index].surface);
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_host_dispatch_edge_reveal_events(&host->edge_reveals[index].port);
    }

    host->dirty.events_dispatched_this_cycle = 1;
    return REACH_OK;
}
