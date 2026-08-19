#include "host_internal.h"

static int32_t reach_host_bar_forced_shown(const reach_host *host)
{
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->bar_shown_while_open && desc->capsule_ops != nullptr &&
            desc->capsule_ops->is_open != nullptr && desc->capsule_ops->is_open(desc->capsule))
        {
            return 1;
        }
    }
    return 0;
}

static int32_t reach_host_popup_open(const reach_host *host)
{
    return reach_host_any_surface_open(const_cast<reach_host *>(host),
                                       reach_surface_class_bit(REACH_SURFACE_CLASS_POPUP));
}

static const reach_host_window_facts *reach_host_current_window_facts(reach_host *host)
{
    reach_host_window_facts *facts = &host->window_facts;
    if (facts->valid)
    {
        return facts;
    }

    facts->any_window_maximized =
        host->window_manager.ops.any_window_maximized_on_primary != nullptr &&
        host->window_manager.ops.any_window_maximized_on_primary(host->window_manager.manager);
    facts->foreground_snapped =
        host->window_manager.ops.window_is_snapped_on_primary != nullptr &&
        host->window_manager.ops.window_is_snapped_on_primary(host->window_manager.manager,
                                                              reach_host_foreground_window(host));
    facts->valid = 1;
    return facts;
}

void reach_host_invalidate_window_facts(reach_host *host)
{
    if (host != nullptr)
    {
        host->window_facts.valid = 0;
        reach_top_bar_invalidate_occlusion(host->top_bar_capsule);
    }
}

void reach_host_request_bar_visibility_update(reach_host *host)
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

static void reach_host_apply_pointer_move_subscriptions(reach_host *host, int32_t enabled)
{
    if (host == nullptr)
    {
        return;
    }

    int32_t force = !host->pointer_move.subscriptions_initialized;

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->surface == nullptr)
        {
            continue;
        }
        int32_t wants =
            desc->capsule_ops != nullptr && desc->capsule_ops->wants_pointer_move != nullptr
                ? desc->capsule_ops->wants_pointer_move(desc->capsule)
                : 0;
        if (desc->update_visibility != nullptr)
        {
            wants = 1;
        }
        reach_host_sync_pointer_move_enabled(&desc->surface->window, enabled && wants,
                                             &host->pointer_move.move_enabled[desc->id], force);
    }
    if (force)
    {
        host->pointer_move.subscriptions_initialized = 1;
    }
}

void reach_host_sync_pointer_move_subscriptions(reach_host *host)
{
    reach_host_apply_pointer_move_subscriptions(host, 1);
}

void reach_host_suspend_pointer_move_subscriptions(reach_host *host)
{
    reach_host_apply_pointer_move_subscriptions(host, 0);
}

static int32_t reach_host_get_pointer_position(reach_host *host, reach_point_i32 *out_pointer)
{
    return host != nullptr && out_pointer != nullptr &&
           host->input_source.ops.get_pointer_position != nullptr &&
           host->input_source.ops.get_pointer_position(host->input_source.source, out_pointer) ==
               REACH_OK;
}

static void reach_host_apply_reveal_edge(reach_host *host, reach_screen_hotspot_port *hotspot,
                                         reach_host_bar_reveal_state *reveal, int32_t shown,
                                         reach_rect_f32 edge_bounds)
{
    if (hotspot == nullptr || hotspot->hotspot == nullptr || reveal == nullptr)
    {
        return;
    }

    if (shown &&
        (!reveal->edge_bounds_valid || !reach_host_rect_equal(reveal->edge_bounds, edge_bounds)))
    {
        if (hotspot->ops.set_bounds != nullptr &&
            hotspot->ops.set_bounds(hotspot->hotspot, edge_bounds) == REACH_OK)
        {
            reveal->edge_bounds = edge_bounds;
            reveal->edge_bounds_valid = 1;
        }
    }

    reach_layout_set_visible(&host->layout_manager, reach_host_hotspot_participant(host, hotspot),
                             shown);
}

reach_rect_f32 reach_host_reconcile_bar_visibility(reach_host *host, reach_surface_id id,
                                                   reach_rect_f32 shown_bounds,
                                                   reach_rect_f32 monitor_bounds)
{
    REACH_ASSERT(host != nullptr);

    const reach_surface_desc *desc = &host->surface_descs[id];
    if (desc->update_visibility == nullptr)
    {
        return shown_bounds;
    }

    const reach_host_window_facts *facts = reach_host_current_window_facts(host);

    reach_bar_visibility_request request = {};
    request.shown_bounds = shown_bounds;
    request.monitor_bounds = monitor_bounds;
    request.pointer_valid = reach_host_get_pointer_position(host, &request.pointer);
    request.any_window_maximized = facts->any_window_maximized;
    request.foreground_snapped = facts->foreground_snapped;
    request.force_shown = reach_host_bar_forced_shown(host);
    request.hold_open = reach_host_popup_open(host);
    request.reveal_seconds =
        (host->theme != nullptr ? host->theme : reach_theme_default())->bar_reveal_seconds;
    request.reveal_span_inset =
        id == REACH_SURFACE_ID_TOP_BAR ? reach_host_stage_reveal_corner_size(host) : 0.0f;

    reach_bar_visibility_result result = desc->update_visibility(desc->capsule, &request);

    reach_layout_set_condition(&host->layout_manager, REACH_LAYOUT_CONDITION_BARS_FORCED,
                               request.force_shown);
    reach_layout_set_condition(&host->layout_manager, REACH_LAYOUT_CONDITION_BARS_HELD,
                               request.hold_open);
    if (id == REACH_SURFACE_ID_TOP_BAR)
    {
        reach_layout_set_condition(&host->layout_manager,
                                   REACH_LAYOUT_CONDITION_TOP_BAR_REVEALED,
                                   result.reveal_transition_active);
    }

    if (result.redraw && desc->surface != nullptr)
    {
        desc->surface->dirty_flags = 1;
    }

    reach_host_apply_reveal_edge(host, desc->reveal_edge, desc->reveal,
                                 result.reveal_edge_shown, result.reveal_bounds);

    return result.animated_bounds;
}

void reach_host_build_top_bar_layout(reach_host *host, reach_rect_f32 monitor_bounds)
{
    if (host == nullptr)
    {
        return;
    }

    reach_top_bar_build_context ctx = {};
    ctx.theme = host->theme != nullptr ? host->theme : reach_theme_default();
    ctx.monitor_bounds = monitor_bounds;
    ctx.dpi_scale = reach_host_layout_dpi_scale(host);

    reach_top_bar_tray_item tray_items[REACH_TOP_BAR_MAX_TRAY_ICONS] = {};
    size_t tray_item_count = reach_tray_item_count(host->tray_capsule);
    size_t copied_count = tray_item_count < REACH_TOP_BAR_MAX_TRAY_ICONS
                              ? tray_item_count
                              : REACH_TOP_BAR_MAX_TRAY_ICONS;
    for (size_t index = 0; index < copied_count; ++index)
    {
        tray_items[index].id = reach_tray_item_id(host->tray_capsule, index);
        tray_items[index].icon_id = reach_tray_item_icon_id(host->tray_capsule, index);
    }
    ctx.tray_items = tray_items;
    ctx.tray_item_count = tray_item_count;
    ctx.tray_popup_open = reach_tray_popup_is_open(host->tray_capsule);

    reach_top_bar_build_layout(host->top_bar_capsule, &ctx);
    reach_tray_set_overflow_start(host->tray_capsule,
                                  reach_top_bar_tray_overflow_start(host->top_bar_capsule));
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

int32_t reach_host_can_move_bars_without_redraw(const reach_host *host)
{
    if (host == nullptr)
    {
        return 0;
    }
    int32_t position_animating = 0;
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->transition != nullptr &&
            reach_host_surface_transition_active(host, desc->transition))
        {
            return 0;
        }
        if (desc->reveal_animation == nullptr)
        {
            continue;
        }
        reach_bar_reveal_animation animation = desc->reveal_animation(desc->capsule);
        if (animation.content_animating)
        {
            return 0;
        }
        position_animating = position_animating || animation.position_animating;
    }

    return position_animating && host->has_layout && !host->dirty.update_requested &&
           !host->dirty.layout && !host->dirty.render && !reach_host_any_surface_dirty(host) &&
           !reach_animation_manager_active(reach_tray_animation_manager(host->tray_capsule),
                                           REACH_TRAY_ANIM_FEEDBACK_OPACITY) &&
           !reach_quick_settings_height_animation_active(host->quick_settings_capsule);
}

reach_result reach_host_move_dock_reveal_frame(reach_host *host)
{
    reach_rect_f32 bounds = host->layout.dock.bounds;
    bounds.y =
        reach_animation_manager_value(reach_dock_manager(host->dock_capsule), REACH_DOCK_ANIM_Y);

    int32_t window_changed = 0;
    reach_result result = reach_host_apply_window_state(
        &host->dock.window, bounds, 1.0f, &host->dock.last_bounds, &host->dock.last_opacity,
        &host->dock.bounds_valid, &host->dock.opacity_valid, &window_changed);
    if (result != REACH_OK)
    {
        return result;
    }

    host->layout.dock.bounds = bounds;
    return REACH_OK;
}

reach_result reach_host_move_top_bar_reveal_frame(reach_host *host)
{
    reach_rect_f32 bounds = reach_top_bar_state_ptr(host->top_bar_capsule)->layout.bounds;
    bounds.y = reach_animation_manager_value(reach_top_bar_manager(host->top_bar_capsule),
                                             REACH_TOP_BAR_ANIM_Y);

    int32_t window_changed = 0;
    reach_result result = reach_host_apply_window_state(
        &host->top_bar.window, bounds, 1.0f, &host->top_bar.last_bounds,
        &host->top_bar.last_opacity, &host->top_bar.bounds_valid, &host->top_bar.opacity_valid,
        &window_changed);
    reach_top_bar_move_window_push_frame(host->top_bar_capsule);
    return result;
}

reach_result reach_host_move_bar_animation_frame(reach_host *host)
{
    if (host == nullptr || !host->has_layout)
    {
        return REACH_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->reveal_animation == nullptr || desc->reveal_frame == nullptr ||
            !desc->reveal_animation(desc->capsule).position_animating)
        {
            continue;
        }
        reach_result result = desc->reveal_frame(host);
        if (result != REACH_OK)
        {
            return result;
        }
    }

    host->dirty.update_requested = 0;
    return REACH_OK;
}
