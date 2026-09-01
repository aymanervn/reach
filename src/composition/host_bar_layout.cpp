#include "host_internal.h"

static int32_t reach_host_bar_forced_shown(const reach_host *host)
{
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[index];
        if (desc->definition->surface.bar_shown_while_open &&
            desc->definition->capsule_ops != nullptr && reach_host_surface_presented(desc))
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

void reach_host_sync_bar_layout_conditions(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }
    reach_layout_set_condition(&host->layout_manager, REACH_LAYOUT_CONDITION_BARS_FORCED,
                               reach_host_bar_forced_shown(host));
    reach_layout_set_condition(&host->layout_manager, REACH_LAYOUT_CONDITION_BARS_HELD,
                               reach_host_popup_open(host));
}

void reach_host_invalidate_bar_coverage(reach_host *host)
{
    if (host != nullptr)
    {
        for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
        {
            const reach_feature_runtime *desc = &host->feature_runtimes[index];
            if (desc->definition->surface.bar_reveal.ops != nullptr &&
                desc->definition->surface.bar_reveal.ops->invalidate_coverage != nullptr)
            {
                desc->definition->surface.bar_reveal.ops->invalidate_coverage(desc->capsule);
            }
        }
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
        reach_feature_runtime *desc = &host->feature_runtimes[index];
        if (desc->surface == nullptr)
        {
            continue;
        }
        int32_t wants = desc->definition->capsule_ops != nullptr &&
                                desc->definition->capsule_ops->wants_pointer_move != nullptr
                            ? desc->definition->capsule_ops->wants_pointer_move(desc->capsule)
                            : 0;
        if (desc->definition->surface.bar_reveal.ops != nullptr)
        {
            wants = 1;
        }
        int32_t game_mode_visible =
            reach_host_game_mode_enabled(host) && (desc->definition->surface.behavior_flags &
                                                   REACH_SURFACE_BEHAVIOR_GAME_MODE_VISIBLE) != 0;
        reach_host_sync_pointer_move_enabled(
            &desc->surface->window, (enabled || game_mode_visible) && wants,
            &host->pointer_move.move_enabled[desc->definition->id], force);
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

static void reach_host_apply_edge_reveal(reach_host *host, reach_host_edge_reveal_runtime *runtime,
                                         int32_t shown, reach_rect_f32 edge_bounds)
{
    if (runtime == nullptr || runtime->port.hotspot == nullptr)
    {
        return;
    }

    if (shown)
    {
        reach_host_set_edge_reveal_bounds(runtime, edge_bounds);
    }
    reach_host_set_edge_reveal_visible(host, runtime, shown);
}

void reach_host_set_pointer_observation(reach_host *host, reach_surface_id id,
                                        reach_rect_f32 bounds, int32_t enabled)
{
    if (host == nullptr || host->input_source.ops.set_pointer_region == nullptr)
    {
        return;
    }
    (void)host->input_source.ops.set_pointer_region(host->input_source.source,
                                                    static_cast<uint32_t>(id), bounds, enabled);
}

static void reach_host_apply_bar_pointer_observation(reach_host *host, reach_surface_id id,
                                                     const reach_bar_visibility_result *result)
{
    if (result == nullptr)
    {
        return;
    }
    reach_host_set_pointer_observation(host, id, result->pointer_observation_bounds,
                                       result->pointer_observation_active);
}

reach_rect_f32 reach_host_reconcile_bar_visibility(reach_host *host, reach_surface_id id,
                                                   reach_rect_f32 shown_bounds,
                                                   reach_rect_f32 monitor_bounds)
{
    REACH_ASSERT(host != nullptr);

    const reach_feature_runtime *desc = &host->feature_runtimes[id];
    if (desc->definition->surface.bar_reveal.ops == nullptr ||
        desc->definition->surface.bar_reveal.ops->update_visibility == nullptr)
    {
        return shown_bounds;
    }

    reach_bar_visibility_request request = {};
    request.shown_bounds = shown_bounds;
    request.monitor_bounds = monitor_bounds;
    request.pointer_valid = reach_host_get_pointer_position(host, &request.pointer);
    request.force_shown = reach_host_bar_forced_shown(host);
    request.force_hidden = host->window_manipulation.relevant;
    request.hold_open = reach_host_popup_open(host);
    request.excluded_window = host->window_manipulation.active_window;
    request.reveal_seconds =
        (host->theme != nullptr ? host->theme : reach_theme_default())->bar_reveal_seconds;
    request.reveal_span_inset = desc->definition->surface.bar_reveal.span_start_inset_dp *
                                reach_host_layout_dpi_scale(host);
    request.shadow_clearance = reach_theme_shadow_extent(reach_host_surface_shadow(host, id),
                                                         reach_host_layout_dpi_scale(host));

    reach_bar_visibility_result result =
        desc->definition->surface.bar_reveal.ops->update_visibility(desc->capsule, &request);

    if (id == REACH_SURFACE_ID_TOP_BAR)
    {
        int32_t was_hidden = host->top_bar_hidden;
        host->top_bar_hidden = result.visible ? 0 : 1;
        if (was_hidden && !host->top_bar_hidden)
        {
            reach_feature_notification notification = {};
            notification.kind = REACH_FEATURE_NOTIFICATION_TOP_BAR_VISIBLE;
            notification.present = 1;
            reach_host_notify_registered_features(host, &notification);
        }
    }

    reach_host_apply_bar_pointer_observation(host, id, &result);

    if (desc->definition->surface.bar_reveal.active_layer > 0)
    {
        reach_layout_set_layer_intent(&host->layout_manager, host->surface_participants[id],
                                      result.reveal_transition_active,
                                      desc->definition->surface.bar_reveal.active_layer);
    }

    if (result.redraw && desc->surface != nullptr)
    {
        desc->surface->dirty_flags = 1;
    }

    reach_host_apply_edge_reveal(host, reach_host_edge_reveal_for_surface(host, id),
                                 result.reveal_edge_shown, result.reveal_bounds);

    return result.animated_bounds;
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
    reach_host_notify_display_changed(host);
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
        const reach_feature_runtime *desc = &host->feature_runtimes[index];
        if (desc->definition->surface.bar_reveal.ops == nullptr ||
            desc->definition->surface.bar_reveal.ops->animation == nullptr)
        {
            continue;
        }
        reach_bar_reveal_animation animation =
            desc->definition->surface.bar_reveal.ops->animation(desc->capsule);
        if (animation.content_animating)
        {
            return 0;
        }
        position_animating = position_animating || animation.position_animating;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *runtime = &host->feature_runtimes[index];
        const reach_feature_control_ops *control = runtime->definition->control_ops;
        if (control != nullptr && control->blocks_position_only_frame != nullptr &&
            control->blocks_position_only_frame(runtime->capsule))
        {
            return 0;
        }
    }

    return position_animating && host->has_layout && !host->dirty.update_requested &&
           !host->dirty.layout && !host->dirty.render && !reach_host_any_surface_dirty(host);
}

reach_result reach_host_move_bar_animation_frame(reach_host *host)
{
    if (host == nullptr || !host->has_layout)
    {
        return REACH_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[index];
        if (desc->definition->surface.bar_reveal.ops == nullptr ||
            desc->definition->surface.bar_reveal.ops->animation == nullptr ||
            desc->surface == nullptr)
        {
            continue;
        }
        reach_bar_reveal_animation animation =
            desc->definition->surface.bar_reveal.ops->animation(desc->capsule);
        if (!animation.position_animating)
        {
            continue;
        }

        reach_rect_f32 bounds = desc->surface->last_bounds;
        bounds.y = animation.animated_y;
        int32_t window_changed = 0;
        reach_result result = reach_host_apply_window_state(
            &desc->surface->window, bounds,
            reach_host_surface_shadow_pad(host, desc->definition->id),
            &desc->surface->last_bounds, &desc->surface->bounds_valid, &window_changed);
        if (result != REACH_OK)
        {
            return result;
        }
        if (desc->definition->surface.bar_reveal.ops->position_frame != nullptr)
        {
            desc->definition->surface.bar_reveal.ops->position_frame(desc->capsule);
        }
    }

    host->dirty.update_requested = 0;
    return REACH_OK;
}
