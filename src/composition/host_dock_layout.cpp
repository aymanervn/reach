#include "host_internal.h"

#include <math.h>

static int32_t reach_host_dock_key_equal(int32_t a_pinned, uint32_t a_pin_id, uintptr_t a_window,
                                          int32_t b_pinned, uint32_t b_pin_id, uintptr_t b_window)
{
    reach_dock_order_key a = {a_pinned, a_pin_id, a_window};
    reach_dock_order_key b = {b_pinned, b_pin_id, b_window};
    return reach_dock_key_equal(&a, &b);
}

static int32_t reach_host_transient_open(const reach_host *host)
{

    return reach_host_any_surface_open(
        const_cast<reach_host *>(host), reach_surface_class_bit(REACH_SURFACE_CLASS_TRANSIENT) |
                                            reach_surface_class_bit(REACH_SURFACE_CLASS_POPUP));
}

int32_t reach_host_dock_can_hide(const reach_host *host)
{
    REACH_ASSERT(host != nullptr);
    if (host == nullptr)
    {
        return 0;
    }
    if (!host->dock_config.auto_hide)
    {
        return 0;
    }
    if (host->window_manager.ops.foreground_is_maximized_on_primary != nullptr)
    {
        return host->window_manager.ops.foreground_is_maximized_on_primary(host->window_manager.manager);
    }
    return 0;
}

void reach_host_request_dock_visibility_update(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_host_request_update(host);
}

static void reach_host_sync_pointer_move_enabled(reach_platform_window_port *window,
                                                  int32_t desired, int32_t *current, int32_t force)
{
    if (window == nullptr || current == nullptr)
    {
        return;
    }
    desired = desired ? 1 : 0;
    if (!force && *current == desired)
    {
        return;
    }
    if (window->ops.set_pointer_move_enabled != nullptr &&
        window->ops.set_pointer_move_enabled(window->window, desired) == REACH_OK)
    {
        *current = desired;
    }
}

void reach_host_sync_pointer_move_subscriptions(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    int32_t enabled = !reach_host_game_mode_enabled(host);
    int32_t force = !host->dock_reveal.subscriptions_initialized;

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->surface == nullptr)
        {
            continue;
        }
        int32_t wants = desc->capsule_ops != nullptr &&
                                desc->capsule_ops->wants_pointer_move != nullptr
                            ? desc->capsule_ops->wants_pointer_move(desc->capsule)
                            : 0;
        if (desc->id == REACH_SURFACE_ID_DOCK)
        {
            wants = wants || reach_host_dock_can_hide(host);
        }
        reach_host_sync_pointer_move_enabled(&desc->surface->window, enabled && wants,
                                             &host->dock_reveal.move_enabled[desc->id], force);
    }
    if (force)
    {
        host->dock_reveal.subscriptions_initialized = 1;
    }
}

static int32_t reach_host_get_pointer_position(reach_host *host, reach_point_i32 *out_pointer)
{
    return host != nullptr && out_pointer != nullptr &&
           host->input_source.ops.get_pointer_position != nullptr &&
           host->input_source.ops.get_pointer_position(host->input_source.source, out_pointer) ==
               REACH_OK;
}

static int32_t reach_host_reveal_edge_rect_equal(reach_rect_f32 a, reach_rect_f32 b)
{
    return fabsf(a.x - b.x) < 0.5f && fabsf(a.y - b.y) < 0.5f && fabsf(a.width - b.width) < 0.5f &&
           fabsf(a.height - b.height) < 0.5f;
}

static void reach_host_apply_reveal_edge(reach_host *host, int32_t mode,
                                          reach_rect_f32 shown_dock_bounds,
                                          reach_rect_f32 monitor_bounds)
{
    if (host == nullptr || host->dock_reveal_edge.edge == nullptr)
    {
        return;
    }

    if (mode == REACH_DOCK_REVEAL_EDGE_DISABLED)
    {
        if (host->dock_reveal.edge_visible && host->dock_reveal_edge.ops.hide != nullptr &&
            host->dock_reveal_edge.ops.hide(host->dock_reveal_edge.edge) == REACH_OK)
        {
            host->dock_reveal.edge_visible = 0;
        }
        return;
    }

    reach_rect_f32 edge_bounds =
        reach_dock_reveal_edge_bounds(mode, shown_dock_bounds, monitor_bounds);
    if (!host->dock_reveal.edge_bounds_valid ||
        !reach_host_reveal_edge_rect_equal(host->dock_reveal.edge_bounds, edge_bounds))
    {
        if (host->dock_reveal_edge.ops.set_bounds != nullptr &&
            host->dock_reveal_edge.ops.set_bounds(host->dock_reveal_edge.edge, edge_bounds) ==
                REACH_OK)
        {
            host->dock_reveal.edge_bounds = edge_bounds;
            host->dock_reveal.edge_bounds_valid = 1;
        }
    }

    if (!host->dock_reveal.edge_visible && host->dock_reveal_edge.ops.show != nullptr &&
        host->dock_reveal_edge.ops.show(host->dock_reveal_edge.edge) == REACH_OK)
    {
        host->dock_reveal.edge_visible = 1;
    }
    if (host->dock.window.ops.native_id != nullptr &&
        host->dock_reveal_edge.ops.place_behind != nullptr)
    {
        reach_window_id dock_id = host->dock.window.ops.native_id(host->dock.window.window);
        if (dock_id != 0)
        {
            (void)host->dock_reveal_edge.ops.place_behind(host->dock_reveal_edge.edge, dock_id);
        }
    }
}

reach_rect_f32 reach_host_reconcile_dock_visibility(reach_host *host,
                                                     reach_rect_f32 shown_bounds,
                                                     reach_rect_f32 monitor_bounds)
{
    REACH_ASSERT(host != nullptr);

    reach_dock_visibility_request request = {};
    request.shown_bounds = shown_bounds;
    request.monitor_bounds = monitor_bounds;
    request.pointer_valid = reach_host_get_pointer_position(host, &request.pointer);
    request.game_mode = reach_host_game_mode_enabled(host);
    request.can_hide = reach_host_dock_can_hide(host);
    request.transient_open = reach_host_transient_open(host);
    request.dock_sticky_feedback = reach_dock_state_ptr(host->dock_capsule)->feedback_sticky;

    reach_dock_visibility_result result =
        reach_dock_update_visibility(host->dock_capsule, &request);

    if (result.clear_sticky_feedback)
    {
        reach_host_clear_sticky_dock_feedback(host);
    }

    reach_host_apply_reveal_edge(host, result.edge_mode, shown_bounds, monitor_bounds);
    reach_host_sync_pointer_move_subscriptions(host);

    return result.animated_bounds;
}

reach_dock_build_context reach_host_dock_build_context(reach_host *host)
{

    reach_dock_build_context ctx = {};
    if (host == nullptr)
    {
        return ctx;
    }
    ctx.theme = host->theme != nullptr ? host->theme : reach_theme_default();
    ctx.dpi_scale = reach_host_layout_dpi_scale(host);
    ctx.icon_size = host->dock_config.icon_size;
    ctx.gap = host->dock_config.gap;
    ctx.pinned_apps = host->pinned_apps;
    ctx.pinned_app_count = host->pinned_app_count;
    return ctx;
}

reach_result reach_host_refresh_monitor_layout(reach_host *host)
{
    if (host == nullptr || !host->dirty.monitors || host->monitors.list == nullptr ||
        host->wallpaper_surface.ops.set_bounds == nullptr)
    {
        return REACH_OK;
    }

    if (host->monitors.list == nullptr || host->monitors.ops.refresh == nullptr ||
        host->monitors.ops.count == nullptr || host->monitors.ops.get == nullptr)
    {
        return REACH_OK;
    }

    (void)host->monitors.ops.refresh(host->monitors.list);
    size_t monitor_count = host->monitors.ops.count(host->monitors.list);
    if (monitor_count == 0)
    {
        host->dirty.monitors = 0;
        return REACH_OK;
    }

    const reach_monitor_info *monitor = host->monitors.ops.get(host->monitors.list, 0);
    if (monitor == nullptr)
    {
        host->dirty.monitors = 0;
        return REACH_OK;
    }

    int32_t left = monitor->bounds.left;
    int32_t top = monitor->bounds.top;
    int32_t right = monitor->bounds.right;
    int32_t bottom = monitor->bounds.bottom;
    for (size_t index = 1; index < monitor_count; ++index)
    {
        monitor = host->monitors.ops.get(host->monitors.list, index);
        if (monitor == nullptr)
        {
            continue;
        }
        if (monitor->bounds.left < left)
            left = monitor->bounds.left;
        if (monitor->bounds.top < top)
            top = monitor->bounds.top;
        if (monitor->bounds.right > right)
            right = monitor->bounds.right;
        if (monitor->bounds.bottom > bottom)
            bottom = monitor->bounds.bottom;
    }

    reach_rect_f32 wallpaper_bounds = {};
    wallpaper_bounds.x = (float)left;
    wallpaper_bounds.y = (float)top;
    wallpaper_bounds.width = (float)(right - left);
    wallpaper_bounds.height = (float)(bottom - top);
    reach_result wallpaper_bounds_result =
        reach_wallpaper_set_bounds(host->wallpaper, wallpaper_bounds);
    if (wallpaper_bounds_result != REACH_OK)
    {
        return wallpaper_bounds_result;
    }

    host->dirty.monitors = 0;
    return REACH_OK;
}

int32_t reach_host_can_move_dock_without_redraw(const reach_host *host)
{
    if (host == nullptr)
    {
        return 0;
    }
    if (reach_host_game_mode_enabled(host))
    {
        return 0;
    }
    if (reach_host_surface_transition_active(host, &host->launcher_transition) ||
        reach_host_surface_transition_active(host, &host->tray_transition) ||
        reach_host_surface_transition_active(host, &host->quick_settings_transition) ||
        reach_host_surface_transition_active(host, &host->switcher_transition) ||
        reach_host_surface_transition_active(host, &host->context_menu_transition))
    {
        return 0;
    }

    return reach_animation_manager_active(reach_dock_manager(host->dock_capsule), REACH_DOCK_ANIM_Y) &&
           host->has_layout && !host->dirty.update_requested && !host->dirty.layout &&
           !host->dirty.render && !host->dock.dirty_flags && !host->launcher.dirty_flags &&
           !host->tray.dirty_flags && !host->switcher.dirty_flags &&
           !host->context_menu.dirty_flags && !host->quick_settings.dirty_flags &&
           !reach_dock_slots_animating(host->dock_capsule) &&
           !reach_dock_state_ptr(host->dock_capsule)->drag.active &&
           !reach_animation_manager_active(reach_dock_manager(host->dock_capsule),
                                           REACH_DOCK_ANIM_DRAG_SNAP) &&
           !reach_animation_manager_active(reach_dock_manager(host->dock_capsule),
                                           REACH_DOCK_ANIM_FEEDBACK_OPACITY) &&
           !reach_animation_manager_active(reach_tray_animation_manager(host->tray_capsule),
                                           REACH_TRAY_ANIM_FEEDBACK_OPACITY) &&
           !reach_quick_settings_height_animation_active(host->quick_settings_capsule);
}

reach_result reach_host_move_dock_animation_frame(reach_host *host)
{
    if (host == nullptr || !host->has_layout)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_rect_f32 dock_bounds = host->layout.dock.bounds;
    dock_bounds.y = reach_animation_manager_value(reach_dock_manager(host->dock_capsule), REACH_DOCK_ANIM_Y);

    int32_t dock_window_changed = 0;
    reach_result result = reach_host_apply_window_state(
        &host->dock.window, dock_bounds, 1.0f, &host->dock.last_bounds, &host->dock.last_opacity,
        &host->dock.bounds_valid, &host->dock.opacity_valid, &dock_window_changed);
    if (result != REACH_OK)
    {
        return result;
    }

    host->layout.dock.bounds = dock_bounds;

    host->dirty.update_requested = 0;
    return REACH_OK;
}
