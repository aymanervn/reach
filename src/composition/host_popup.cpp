#include "host_internal.h"

static int32_t reach_host_surface_contains_point(const reach_feature_runtime *desc,
                                                 reach_point_i32 point)
{
    reach_rect_f32 bounds = {};
    if (desc->resolved_bounds_valid && desc->resolved_bounds.width > 0.0f &&
        desc->resolved_bounds.height > 0.0f)
    {
        bounds = desc->resolved_bounds;
    }
    else
    {
        const reach_surface_runtime *surface = desc->surface;
        if (surface == nullptr || !surface->bounds_valid)
        {
            return 1;
        }
        bounds = surface->last_bounds;
    }

    return (float)point.x >= bounds.x && (float)point.x <= bounds.x + bounds.width &&
           (float)point.y >= bounds.y && (float)point.y <= bounds.y + bounds.height;
}

/* A press on the control a surface hangs off belongs to that control, not to the dismissal
   hook: the control's own handler decides whether to toggle, replace or leave the surface
   alone. The control is named by the surface's layout anchor, or declared outright by a
   surface that has no anchor. */
static int32_t reach_host_press_holds_surface_open(reach_host *host,
                                                   const reach_feature_runtime *desc,
                                                   reach_point_i32 point)
{
    const reach_surface_spec *spec = &desc->definition->surface;
    reach_feature_layout_anchor anchor = {};
    int32_t match_index = 0;

    if (spec->dismiss_guard_surface < REACH_HOST_SURFACE_COUNT)
    {
        anchor.surface = spec->dismiss_guard_surface;
        anchor.slot = spec->dismiss_guard_slot;
    }
    else
    {
        anchor.surface = desc->definition->layout.anchor;
        anchor.slot = desc->definition->layout.anchor_slot;
        if (desc->definition->surface_ops != nullptr &&
            desc->definition->surface_ops->layout_anchor != nullptr)
        {
            if (!desc->definition->surface_ops->layout_anchor(desc->capsule, &anchor))
            {
                return 0;
            }
            match_index = 1;
        }
    }

    if (anchor.surface >= REACH_HOST_SURFACE_COUNT)
    {
        return 0;
    }

    const reach_feature_runtime *owner = &host->feature_runtimes[anchor.surface];
    if (owner->capsule == nullptr || owner->definition->capsule_ops == nullptr ||
        owner->definition->capsule_ops->control_at_point == nullptr)
    {
        return 0;
    }

    reach_feature_control control = {};
    if (!owner->definition->capsule_ops->control_at_point(owner->capsule, point.x, point.y,
                                                          &control) ||
        !control.valid)
    {
        return 0;
    }

    if (spec->dismiss_guard_any_control)
    {
        return 1;
    }
    return control.slot == anchor.slot && (!match_index || control.index == anchor.index);
}

static void reach_host_handle_global_mouse_down(reach_host *host, reach_point_i32 point)
{
    if (host == nullptr)
    {
        return;
    }

    int32_t closed_any = 0;
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[index];
        if (desc->definition->surface.cls != REACH_SURFACE_CLASS_TRANSIENT &&
            desc->definition->surface.cls != REACH_SURFACE_CLASS_POPUP)
        {
            continue;
        }
        if (!reach_host_surface_closable(desc) || !reach_host_surface_is_open(desc) ||
            reach_host_surface_contains_point(desc, point) ||
            reach_host_press_holds_surface_open(host, desc, point))
        {
            continue;
        }
        reach_host_close_registered_surface(host, desc->definition->id,
                                            REACH_SURFACE_CLOSE_SUPERSEDED);
        closed_any = 1;
    }

    if (closed_any)
    {
        reach_host_notify_popups_closed(host);
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

void reach_host_close_surface_classes(reach_host *host, uint32_t class_mask, int32_t restore_focus)
{
    if (host == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[index];
        if ((class_mask & reach_surface_class_bit(desc->definition->surface.cls)) == 0)
        {
            continue;
        }
        reach_host_close_registered_surface(host, desc->definition->id,
                                            restore_focus ? REACH_SURFACE_CLOSE_DISMISS
                                                          : REACH_SURFACE_CLOSE_SUPERSEDED);
    }
    reach_host_notify_popups_closed(host);
}

void reach_host_close_transient_surfaces(reach_host *host, int32_t restore_focus)
{
    reach_host_close_surface_classes(host,
                                     reach_surface_class_bit(REACH_SURFACE_CLASS_TRANSIENT) |
                                         reach_surface_class_bit(REACH_SURFACE_CLASS_POPUP),
                                     restore_focus);
}

reach_result reach_host_close_window(reach_host *host, uintptr_t window_id)
{
    if (host == nullptr || window_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    return reach_host_schedule_window_control(host, REACH_WINDOW_CONTROL_CLOSE, window_id);
}
