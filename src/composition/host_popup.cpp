#include "host_internal.h"

static int32_t reach_host_surface_contains_point(const reach_surface_desc *desc,
                                                 reach_point_i32 point)
{
    const reach_surface_runtime *surface = desc->surface;
    if (surface == nullptr || !surface->bounds_valid)
    {
        return 1;
    }

    const reach_rect_f32 *bounds = &surface->last_bounds;
    return (float)point.x >= bounds->x && (float)point.x <= bounds->x + bounds->width &&
           (float)point.y >= bounds->y && (float)point.y <= bounds->y + bounds->height;
}

static int32_t reach_host_dock_cluster_holds_surface_open(reach_host *host,
                                                          const reach_surface_desc *desc,
                                                          reach_dock_pointer_region dock_region)
{
    switch (desc->id)
    {
    case REACH_SURFACE_ID_TRAY:
        return dock_region == REACH_DOCK_POINTER_REGION_TRAY_BUTTON;
    case REACH_SURFACE_ID_QUICK_SETTINGS:
        return dock_region == REACH_DOCK_POINTER_REGION_QUICK_SETTINGS_BUTTON;
    case REACH_SURFACE_ID_CONTEXT_MENU:
        return reach_context_menu_state_ptr(host->context_menu_capsule)->power_open &&
               dock_region == REACH_DOCK_POINTER_REGION_POWER_BUTTON;
    case REACH_SURFACE_ID_LAUNCHER:
        return dock_region != REACH_DOCK_POINTER_REGION_NONE;
    default:
        return 0;
    }
}

static void reach_host_handle_global_mouse_down(reach_host *host, reach_point_i32 point)
{
    if (host == nullptr)
    {
        return;
    }

    reach_point_i32 dock_point = host->has_layout
                                     ? reach_dock_local_point(&host->layout.dock, point.x, point.y)
                                     : reach_point_i32{};
    reach_dock_pointer_region dock_region =
        host->has_layout
            ? reach_dock_pointer_region_at(host->dock_capsule, dock_point.x, dock_point.y)
            : REACH_DOCK_POINTER_REGION_NONE;

    int32_t closed_any = 0;
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->cls != REACH_SURFACE_CLASS_TRANSIENT && desc->cls != REACH_SURFACE_CLASS_POPUP)
        {
            continue;
        }
        if (desc->force_close == nullptr || !reach_host_surface_is_open(desc) ||
            reach_host_surface_contains_point(desc, point) ||
            reach_host_dock_cluster_holds_surface_open(host, desc, dock_region))
        {
            continue;
        }
        if (desc->transition != nullptr && desc->transition->target_open &&
            reach_host_surface_transition_active(host, desc->transition))
        {
            continue;
        }

        desc->force_close(host);
        closed_any = 1;
    }

    if (closed_any)
    {
        reach_host_clear_sticky_dock_feedback(host);
    }
}

static void reach_host_on_popup_mouse_down(void *user, int32_t x, int32_t y)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr)
    {
        return;
    }
    reach_point_i32 point = {x, y};
    reach_host_handle_global_mouse_down(host, point);
}

void reach_host_sync_popup_mouse_hook(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    if (host->popup_capture.sync_mouse_hook != nullptr)
    {

        int32_t should_hook = reach_host_any_surface_open(
            host, reach_surface_class_bit(REACH_SURFACE_CLASS_TRANSIENT) |
                      reach_surface_class_bit(REACH_SURFACE_CLASS_POPUP));
        (void)host->popup_capture.sync_mouse_hook(host->popup_capture.userdata, should_hook,
                                                  reach_host_on_popup_mouse_down, host);
    }
}

void reach_host_close_transient_surfaces(reach_host *host, int32_t restore_focus)
{
    if (host == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->cls != REACH_SURFACE_CLASS_TRANSIENT && desc->cls != REACH_SURFACE_CLASS_POPUP)
        {
            continue;
        }
        if (restore_focus && desc->dismiss != nullptr)
        {
            desc->dismiss(host);
        }
        else if (desc->force_close != nullptr)
        {
            desc->force_close(host);
        }
    }
    reach_host_clear_sticky_dock_feedback(host);
}
