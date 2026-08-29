#include "host_internal.h"

#include <math.h>
#include <stdio.h>
#include <time.h>

static const float REACH_HOST_ICON_BASE_SIZE_PX = 154.0f;

int32_t reach_host_icon_size_px(const reach_host *host)
{
    int32_t requested =
        (int32_t)ceilf(REACH_HOST_ICON_BASE_SIZE_PX * reach_host_layout_dpi_scale(host));
    if (requested < 128)
    {
        requested = 128;
    }
    if (requested > 256)
    {
        requested = 256;
    }

    return requested;
}

void reach_host_request_update(reach_host *host)
{
    if (host != nullptr)
    {
        host->dirty.update_requested = 1;
    }
}

void reach_host_on_system_controls_changed(void *user, uint32_t change_flags)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host != nullptr && change_flags != 0)
    {
        host->quick_settings_system_change_flags.fetch_or(change_flags);
        reach_host_request_update(host);
    }
}

void reach_host_on_audio_volume_changed(void *user)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host != nullptr)
    {
        host->audio_volume_changed.store(1);
        reach_host_request_update(host);
    }
}

static void reach_host_process_system_status_requests(reach_host *host)
{
    uint32_t system_flags = host->quick_settings_system_change_flags.exchange(0);
    if (system_flags != 0)
    {
        reach_system_status_refresh_system(host->system_status, system_flags);
    }
    if (host->audio_volume_changed.exchange(0) != 0)
    {
        reach_system_status_refresh_audio(host->system_status);
    }
}

void reach_host_finish_surface_transitions(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    int32_t was_visible[REACH_HOST_SURFACE_COUNT] = {};
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        was_visible[index] =
            reach_host_surface_transition_visible(host->feature_runtimes[index].transition);
    }
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_host_surface_transition_finish(host, host->feature_runtimes[index].transition);
    }
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_feature_runtime *runtime = &host->feature_runtimes[index];
        if (!was_visible[index] || runtime->definition == nullptr ||
            reach_host_surface_transition_visible(runtime->transition) ||
            reach_host_surface_is_open(runtime))
        {
            continue;
        }

        const reach_feature_control_ops *control = runtime->definition->control_ops;
        if (control != nullptr && control->surface_hidden != nullptr &&
            runtime->capsule != nullptr)
        {
            reach_feature_tick_result result = {};
            control->surface_hidden(runtime->capsule, &result);
            reach_host_apply_feature_tick_result(host, runtime, &result);
        }
        reach_host_flush_focus_restore(host, runtime->definition->id);
    }
}

static void reach_host_tick_animations(reach_host *host, double delta_seconds)
{
    reach_animation_manager_tick(&host->animations, delta_seconds);

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_feature_runtime *desc = &host->feature_runtimes[index];
        if (desc->definition->capsule_ops == nullptr ||
            desc->definition->capsule_ops->tick == nullptr)
        {
            continue;
        }
        reach_feature_tick_result tick = {};
        desc->definition->capsule_ops->tick(desc->capsule, delta_seconds, &tick);
        reach_host_apply_feature_tick_result(host, desc, &tick);
    }
    reach_host_finish_surface_transitions(host);
}

static reach_result reach_host_finish_update(reach_host *host)
{
    reach_host_apply_layout(host);

    host->dirty.layout = 0;
    host->dirty.render = 0;
    host->dirty.update_requested = 0;
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_surface_runtime *surface = host->feature_runtimes[index].surface;
        if (surface != nullptr)
        {
            surface->dirty_flags = 0;
        }
    }

    return REACH_OK;
}

static reach_result reach_host_update_game_mode_surfaces(reach_host *host, double delta_seconds)
{
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_feature_runtime *desc = &host->feature_runtimes[index];
        if ((desc->definition->surface.behavior_flags & REACH_SURFACE_BEHAVIOR_GAME_MODE_VISIBLE) ==
                0 ||
            desc->definition->capsule_ops == nullptr ||
            desc->definition->capsule_ops->tick == nullptr)
        {
            continue;
        }
        reach_feature_tick_result tick = {};
        desc->definition->capsule_ops->tick(desc->capsule, delta_seconds, &tick);
        reach_host_apply_feature_tick_result(host, desc, &tick);
    }

    if (host->monitors.ops.primary != nullptr)
    {
        const reach_monitor_info *monitor = host->monitors.ops.primary(host->monitors.list);
        if (monitor != nullptr)
        {
            reach_rect_f32 bounds = {(float)monitor->bounds.left, (float)monitor->bounds.top,
                                     (float)(monitor->bounds.right - monitor->bounds.left),
                                     (float)(monitor->bounds.bottom - monitor->bounds.top)};
            host->layout_dpi_scale = reach_host_monitor_dpi_scale(monitor);
            reach_feature_runtime *dock_desc = &host->feature_runtimes[REACH_SURFACE_ID_DOCK];
            if (!dock_desc->resolved_bounds_valid && dock_desc->capsule != nullptr &&
                dock_desc->definition->surface_ops != nullptr &&
                dock_desc->definition->surface_ops->arrange != nullptr)
            {
                reach_feature_surface_context arrange = {};
                arrange.theme = host->theme != nullptr ? host->theme : reach_theme_default();
                arrange.monitor_bounds = bounds;
                arrange.dpi_scale = host->layout_dpi_scale;
                (void)dock_desc->definition->surface_ops->arrange(dock_desc->capsule, &arrange);

                reach_feature_surface_geometry geometry = {};
                dock_desc->definition->capsule_ops->surface_geometry(dock_desc->capsule, &geometry);
                dock_desc->resolved_bounds = geometry.visible_bounds;
                dock_desc->resolved_bounds_valid = 1;
            }
            reach_host_frame_context frame = {};
            frame.monitor_bounds = bounds;
            for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
            {
                reach_feature_runtime *desc = &host->feature_runtimes[index];
                if ((desc->definition->surface.behavior_flags &
                     REACH_SURFACE_BEHAVIOR_GAME_MODE_VISIBLE) == 0)
                {
                    continue;
                }
                reach_result result = reach_host_frame_registered_surface(host, desc, &frame);
                if (result != REACH_OK)
                {
                    return result;
                }
                reach_host_sync_surface_input_regions(host, desc);
            }
        }
    }
    reach_host_sync_pointer_move_subscriptions(host);
    return reach_host_finish_update(host);
}

reach_result reach_host_update(reach_host *host, double delta_seconds)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (host->dirty.events_dispatched_this_cycle)
    {
        host->dirty.events_dispatched_this_cycle = 0;
    }
    else
    {
        (void)reach_host_dispatch_events(host);
        host->dirty.events_dispatched_this_cycle = 0;
    }

    reach_host_apply_window_control_result(host);

    int32_t window_manager_dirty =
        host->window_manager.ops.needs_refresh != nullptr &&
        host->window_manager.ops.needs_refresh(host->window_manager.manager);
    if (window_manager_dirty)
    {
        reach_host_refresh_window_world(host);
        reach_host_apply_foreground_change(host);
    }

    (void)reach_host_update_game_mode(host);
    if (reach_host_game_mode_enabled(host))
    {
        return reach_host_update_game_mode_surfaces(host, delta_seconds);
    }

    reach_host_process_system_status_requests(host);
    reach_host_tick_animations(host, delta_seconds);
    reach_host_drain_registered_render_resources(host);
    reach_host_drain_now_playing_retired_covers(host);
    reach_host_process_deferred_launch(host);
    reach_host_sync_bar_layout_conditions(host);
    if (reach_host_can_move_bars_without_redraw(host))
    {
        return reach_host_move_bar_animation_frame(host);
    }

    host->popup_hook_reassert_seconds += delta_seconds;
    if (host->popup_hook_reassert_seconds >= 2.0)
    {
        host->popup_hook_reassert_seconds = 0.0;
        reach_host_sync_popup_mouse_hook(host);
    }
    reach_host_drain_icon_evictions(host);
    if (reach_clock_tick(host->clock))
    {
        host->surfaces[REACH_SURFACE_ID_TOP_BAR].dirty_flags = 1;
    }
    if (reach_input_language_service_tick_settle(host->input_language, delta_seconds,
                                                 reach_host_foreground_window(host)))
    {
        host->surfaces[REACH_SURFACE_ID_TOP_BAR].dirty_flags = 1;
        host->dirty.layout = 1;
    }
    if (reach_system_stats_take_changed(host->system_stats))
    {
        host->surfaces[REACH_SURFACE_ID_TOP_BAR].dirty_flags = 1;
        host->dirty.layout = 1;
    }

    reach_result monitor_result = reach_host_refresh_monitor_layout(host);
    if (monitor_result != REACH_OK)
    {
        return monitor_result;
    }

    if (host->surfaces[REACH_SURFACE_ID_LAUNCHER].window.ops.set_bounds != nullptr && host->monitors.list != nullptr &&
        host->monitors.ops.count != nullptr && host->monitors.ops.primary != nullptr &&
        host->monitors.ops.count(host->monitors.list) > 0)
    {
        const reach_monitor_info *monitor = host->monitors.ops.primary(host->monitors.list);
        REACH_ASSERT(monitor != nullptr);
        REACH_ASSERT(monitor->primary || host->monitors.ops.count(host->monitors.list) == 1);
        if (monitor == nullptr)
        {
            return REACH_ERROR;
        }

        reach_rect_f32 bounds = {};
        bounds.x = (float)monitor->bounds.left;
        bounds.y = (float)monitor->bounds.top;
        bounds.width = (float)(monitor->bounds.right - monitor->bounds.left);
        bounds.height = (float)(monitor->bounds.bottom - monitor->bounds.top);

        reach_result result = REACH_OK;

        if (host->surfaces[REACH_SURFACE_ID_LAUNCHER].renderer.ops.begin_frame != nullptr)
        {
            reach_ui_layout_input input = {};
            input.monitor_bounds = bounds;
            input.work_area = bounds;
            input.dpi_scale = reach_host_monitor_dpi_scale(monitor);
            input.border_thickness = reach_theme_border_thickness(
                host->theme != nullptr ? host->theme : reach_theme_default(), input.dpi_scale);
            host->layout_dpi_scale = input.dpi_scale;

            {
                host->has_layout = 1;

                reach_host_frame_context frame_ctx = {};
                frame_ctx.monitor_bounds = bounds;
                reach_host_sync_edge_reveals(host, bounds);
                reach_surface_id frame_order[REACH_HOST_SURFACE_COUNT];
                size_t frame_count = 0;
                for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
                {
                    const reach_feature_runtime *desc = &host->feature_runtimes[index];
                    size_t at = frame_count;
                    while (at > 0 &&
                           host->feature_runtimes[frame_order[at - 1]].definition->layout.priority >
                               desc->definition->layout.priority)
                    {
                        frame_order[at] = frame_order[at - 1];
                        --at;
                    }
                    frame_order[at] = desc->definition->id;
                    ++frame_count;
                }
                for (size_t index = 0; index < frame_count; ++index)
                {
                    reach_feature_runtime *desc = &host->feature_runtimes[frame_order[index]];
                    result = reach_host_frame_registered_surface(host, desc, &frame_ctx);
                    if (result != REACH_OK)
                    {
                        return result;
                    }
                    reach_host_sync_surface_input_regions(host, desc);
                }
            }
        }
    }
    reach_host_sync_pointer_move_subscriptions(host);
    return reach_host_finish_update(host);
}

int32_t reach_host_frame_interval_ms(const reach_host *host)
{
    if (host == nullptr || !host->high_refresh_rate)
    {
        return 16;
    }

    const reach_monitor_info *primary = host->monitors.ops.primary(host->monitors.list);
    if (primary == nullptr || primary->refresh_rate_hz == 0)
    {
        return 16;
    }

    const int32_t cap_fps = primary->refresh_rate_hz < 120 ? primary->refresh_rate_hz : 120;
    return 1000 / cap_fps;
}

reach_theme_mode reach_host_theme_mode(const reach_host *host)
{
    const reach_theme *theme =
        host != nullptr && host->theme != nullptr ? host->theme : reach_theme_default();
    return theme->mode;
}

void reach_host_set_theme_mode(reach_host *host, reach_theme_mode mode)
{
    if (host == nullptr)
    {
        return;
    }

    const reach_theme *theme = reach_theme_for_mode(mode);
    if (host->theme == theme)
    {
        return;
    }

    host->theme = theme;
    host->dirty.layout = 1;
    host->dirty.render = 1;
}

int32_t reach_host_needs_frame(const reach_host *host)
{
    if (host == nullptr)
    {
        return 0;
    }

    int32_t window_manager_needs_refresh =
        host->window_manager.manager != nullptr &&
        host->window_manager.ops.needs_refresh != nullptr &&
        host->window_manager.ops.needs_refresh(host->window_manager.manager);

    if (host->dirty.update_requested || window_manager_needs_refresh)
    {
        return 1;
    }

    if (reach_host_game_mode_enabled(host))
    {
        for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
        {
            const reach_feature_runtime *desc = &host->feature_runtimes[index];
            if ((desc->definition->surface.behavior_flags &
                 REACH_SURFACE_BEHAVIOR_GAME_MODE_VISIBLE) != 0 &&
                desc->definition->capsule_ops != nullptr &&
                desc->definition->capsule_ops->needs_frame != nullptr &&
                desc->definition->capsule_ops->needs_frame(desc->capsule))
            {
                return 1;
            }
        }
        return 0;
    }

    if (host->dirty.render || reach_host_any_surface_dirty(host) ||
        reach_icon_service_work_pending(host->icon_service) ||
        reach_animation_manager_any_active(&host->animations) ||
        reach_clock_minute_elapsed(host->clock) ||
        reach_input_language_service_settling(host->input_language))
    {
        return 1;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[index];
        if (desc->definition->capsule_ops != nullptr &&
            desc->definition->capsule_ops->needs_frame != nullptr &&
            desc->definition->capsule_ops->needs_frame(desc->capsule))
        {
            return 1;
        }
    }
    return 0;
}

uint32_t reach_host_idle_wait_ms(const reach_host *host)
{
    if (host == nullptr)
    {
        return REACH_CLOCK_WAIT_FOREVER;
    }
    if (reach_host_game_mode_enabled(host))
    {
        return reach_host_needs_frame(host) ? (uint32_t)reach_host_frame_interval_ms(host)
                                            : REACH_CLOCK_WAIT_FOREVER;
    }
    return reach_clock_next_minute_delay_ms(host->clock);
}
