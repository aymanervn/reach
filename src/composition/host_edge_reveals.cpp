#include "host_internal.h"

static void reach_host_on_edge_reveal_event(void *user, reach_screen_hotspot_event event)
{
    reach_host_edge_reveal_runtime *runtime = static_cast<reach_host_edge_reveal_runtime *>(user);
    if (runtime == nullptr || runtime->host == nullptr || runtime->owner == nullptr)
    {
        return;
    }
    if (runtime->host->window_manipulation.relevant)
    {
        return;
    }

    const reach_feature_runtime *owner = runtime->owner;
    if (owner->definition->surface.bar_reveal.ops != nullptr)
    {
        if (event == REACH_SCREEN_HOTSPOT_ENTER &&
            owner->definition->surface.bar_reveal.ops->begin_session != nullptr)
        {
            owner->definition->surface.bar_reveal.ops->begin_session(owner->capsule);
        }
        reach_host_request_bar_visibility_update(runtime->host);
        return;
    }

    if (owner->definition->surface.edge_reveal.handle_event != nullptr)
    {
        owner->definition->surface.edge_reveal.handle_event(runtime->host, event);
    }
}

static int32_t reach_host_resolve_edge_reveal_bounds(const reach_edge_reveal_geometry *geometry,
                                                     reach_rect_f32 monitor_bounds, float dpi_scale,
                                                     reach_rect_f32 *out_bounds)
{
    if (geometry == nullptr || out_bounds == nullptr ||
        geometry->anchor == REACH_EDGE_REVEAL_ANCHOR_MANAGED || geometry->width_dp <= 0.0f ||
        geometry->height_dp <= 0.0f)
    {
        return 0;
    }

    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    reach_rect_f32 bounds = {};
    bounds.width = geometry->width_dp * scale;
    bounds.height = geometry->height_dp * scale;
    bounds.x = monitor_bounds.x;
    bounds.y = monitor_bounds.y;

    switch (geometry->anchor)
    {
    case REACH_EDGE_REVEAL_ANCHOR_TOP:
        bounds.x += (monitor_bounds.width - bounds.width) * 0.5f;
        break;
    case REACH_EDGE_REVEAL_ANCHOR_TOP_RIGHT:
        bounds.x += monitor_bounds.width - bounds.width;
        break;
    case REACH_EDGE_REVEAL_ANCHOR_RIGHT:
        bounds.x += monitor_bounds.width - bounds.width;
        bounds.y += (monitor_bounds.height - bounds.height) * 0.5f;
        break;
    case REACH_EDGE_REVEAL_ANCHOR_BOTTOM_RIGHT:
        bounds.x += monitor_bounds.width - bounds.width;
        bounds.y += monitor_bounds.height - bounds.height;
        break;
    case REACH_EDGE_REVEAL_ANCHOR_BOTTOM:
        bounds.x += (monitor_bounds.width - bounds.width) * 0.5f;
        bounds.y += monitor_bounds.height - bounds.height;
        break;
    case REACH_EDGE_REVEAL_ANCHOR_BOTTOM_LEFT:
        bounds.y += monitor_bounds.height - bounds.height;
        break;
    case REACH_EDGE_REVEAL_ANCHOR_LEFT:
        bounds.y += (monitor_bounds.height - bounds.height) * 0.5f;
        break;
    case REACH_EDGE_REVEAL_ANCHOR_TOP_LEFT:
        break;
    case REACH_EDGE_REVEAL_ANCHOR_MANAGED:
    default:
        return 0;
    }

    *out_bounds = bounds;
    return 1;
}

reach_host_edge_reveal_runtime *reach_host_edge_reveal_for_surface(reach_host *host,
                                                                   reach_surface_id id)
{
    if (host == nullptr || (size_t)id >= REACH_HOST_SURFACE_COUNT)
    {
        return nullptr;
    }
    return host->feature_runtimes[id].definition->surface.edge_reveal.enabled
               ? &host->edge_reveals[id]
               : nullptr;
}

reach_result reach_host_create_edge_reveals(reach_host *host)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_host_edge_reveal_runtime *runtime = &host->edge_reveals[index];
        *runtime = {};
        const reach_feature_runtime *owner = &host->feature_runtimes[index];
        if (!owner->definition->surface.edge_reveal.enabled)
        {
            continue;
        }
        if (host->screen_hotspots.ops.create == nullptr)
        {
            reach_host_destroy_edge_reveals(host);
            return REACH_INVALID_ARGUMENT;
        }

        runtime->host = host;
        runtime->owner = owner;
        reach_result result =
            host->screen_hotspots.ops.create(host->screen_hotspots.factory, &runtime->port);
        if (result != REACH_OK)
        {
            reach_host_destroy_edge_reveals(host);
            return result;
        }
    }
    return REACH_OK;
}

reach_result reach_host_start_edge_reveals(reach_host *host)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_host_edge_reveal_runtime *runtime = &host->edge_reveals[index];
        if (runtime->port.hotspot == nullptr || runtime->port.ops.set_callback == nullptr)
        {
            continue;
        }
        reach_result result = runtime->port.ops.set_callback(
            runtime->port.hotspot, reach_host_on_edge_reveal_event, runtime);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    return REACH_OK;
}

void reach_host_destroy_edge_reveals(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_host_edge_reveal_runtime *runtime = &host->edge_reveals[index];
        if (runtime->port.hotspot != nullptr && runtime->port.ops.hide != nullptr)
        {
            (void)runtime->port.ops.hide(runtime->port.hotspot);
        }
        if (runtime->port.hotspot != nullptr && runtime->port.ops.destroy != nullptr)
        {
            runtime->port.ops.destroy(runtime->port.hotspot);
        }
        *runtime = {};
    }
}

void reach_host_sync_edge_reveals(reach_host *host, reach_rect_f32 monitor_bounds)
{
    if (host == nullptr)
    {
        return;
    }

    float dpi_scale = reach_host_layout_dpi_scale(host);
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_host_edge_reveal_runtime *runtime = &host->edge_reveals[index];
        const reach_edge_reveal_geometry *geometry =
            &host->feature_runtimes[index].definition->surface.edge_reveal.geometry;
        reach_rect_f32 bounds = {};
        if (runtime->port.hotspot == nullptr ||
            !reach_host_resolve_edge_reveal_bounds(geometry, monitor_bounds, dpi_scale, &bounds))
        {
            continue;
        }
        if (geometry->visible)
        {
            reach_host_set_edge_reveal_bounds(runtime, bounds);
        }
        reach_host_set_edge_reveal_visible(host, runtime, geometry->visible);
    }
}

void reach_host_set_edge_reveal_bounds(reach_host_edge_reveal_runtime *runtime,
                                       reach_rect_f32 bounds)
{
    if (runtime == nullptr || runtime->port.hotspot == nullptr)
    {
        return;
    }
    if (runtime->bounds_valid && reach_rect_equal(runtime->bounds, bounds))
    {
        return;
    }
    if (runtime->port.ops.set_bounds != nullptr &&
        runtime->port.ops.set_bounds(runtime->port.hotspot, bounds) == REACH_OK)
    {
        runtime->bounds = bounds;
        runtime->bounds_valid = 1;
    }
}

void reach_host_set_edge_reveal_visible(reach_host *host, reach_host_edge_reveal_runtime *runtime,
                                        int32_t visible)
{
    if (host == nullptr || runtime == nullptr || runtime->port.hotspot == nullptr)
    {
        return;
    }
    reach_layout_set_visible(&host->layout_manager, runtime->participant,
                             visible && !host->window_manipulation.relevant);
}

void reach_host_on_surface_edge_reveal(reach_host *host, reach_screen_hotspot_event event)
{
    if (host == nullptr || event != REACH_SCREEN_HOTSPOT_ENTER)
    {
        return;
    }
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *runtime = &host->feature_runtimes[index];
        if (runtime->definition->surface.edge_reveal.handle_event ==
            reach_host_on_surface_edge_reveal)
        {
            reach_host_set_registered_surface_open(host, runtime->definition->id, 1);
        }
    }
}
