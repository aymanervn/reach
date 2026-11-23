#include "host_internal.h"
#include "reach/core/frame_clock.h"
#include "reach/support/workload.h"

#include <math.h>
#include <stdio.h>
#include <time.h>

static const float REACH_HOST_ICON_BASE_SIZE_PX = 154.0f;

int32_t reach_host_icon_size_px_for_scale(float dpi_scale)
{
    int32_t requested =
        (int32_t)ceilf(REACH_HOST_ICON_BASE_SIZE_PX * (dpi_scale > 0.0f ? dpi_scale : 1.0f));
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

int32_t reach_host_icon_size_px(const reach_host *host)
{
    return reach_host_icon_size_px_for_scale(reach_host_layout_dpi_scale(host));
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
        host->system_change_flags.fetch_or(change_flags);
        reach_host_post_feature_work_ready(host);
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
    uint32_t system_flags = host->system_change_flags.exchange(0);
    if (system_flags != 0)
    {
        reach_system_status_refresh_system(host->system_status, system_flags);
    }
    if (host->audio_volume_changed.exchange(0) != 0)
    {
        reach_system_status_refresh_audio(host->system_status);
    }
}

void reach_host_finish_surface_presentations(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    int32_t was_visible[REACH_HOST_SURFACE_CAPACITY] = {};
    for (size_t index = 0; index < host->surface_count; ++index)
    {
        reach_feature_runtime *runtime = &host->feature_runtimes[index];
        was_visible[index] = runtime->presentation_visible;
    }
    for (size_t index = 0; index < host->surface_count; ++index)
    {
        reach_feature_runtime *runtime = &host->feature_runtimes[index];
        int32_t visible = 0;
        if (runtime->definition != nullptr)
        {
            visible = visible || reach_host_surface_presented(runtime);
        }
        runtime->presentation_visible = visible;
        if (!was_visible[index] || runtime->definition == nullptr || visible)
        {
            continue;
        }

        const reach_feature_control_ops *control = runtime->definition->control_ops;
        if (control != nullptr && control->surface_hidden != nullptr && runtime->capsule != nullptr)
        {
            reach_feature_tick_result result = {};
            control->surface_hidden(runtime->capsule, &result);
            reach_host_apply_feature_tick_result(host, runtime, &result);
        }
        reach_host_flush_focus_restore(host, runtime->definition->id);
    }
}

static int32_t reach_host_presentation_ready(reach_feature_runtime *runtime)
{
    if (runtime->surface == nullptr ||
        runtime->definition->capsule_ops->surface_geometry == nullptr ||
        runtime->surface->renderer.ops.presentation_complete == nullptr)
        return 1;
    reach_feature_surface_geometry geometry = {};
    runtime->definition->capsule_ops->surface_geometry(runtime->capsule, &geometry);
    const reach_motion *opacity = &geometry.presentation.group.opacity;
    if (!geometry.wait_for_presentation || geometry.deadline_seconds <= 0.0 ||
        !geometry.presentation.retained || opacity->to != 0.0f ||
        opacity->duration_seconds <= 0.0 ||
        reach_motion_wait_ms(reach_motion_deadline(opacity)) != 0)
        return 1;
    reach_render_backend_port *renderer = &runtime->surface->renderer;
    int32_t complete = 0;
    reach_result result =
        renderer->ops.presentation_complete(renderer->backend, opacity->generation, &complete);
    if (result == REACH_DEVICE_LOST)
        return 1;
    if (result != REACH_OK)
    {
        if (renderer->ops.suspend != nullptr)
            renderer->ops.suspend(renderer->backend);
        return 1;
    }
    return complete;
}

static void reach_host_tick_features(reach_host *host, double delta_seconds)
{
    for (size_t index = 0; index < host->surface_count; ++index)
    {
        reach_feature_runtime *desc = &host->feature_runtimes[index];
        if (desc->definition->capsule_ops == nullptr ||
            desc->definition->capsule_ops->tick == nullptr || !reach_host_presentation_ready(desc))
        {
            continue;
        }
        reach_feature_tick_result tick = {};
        reach_workload_add(REACH_WORKLOAD_CAPSULE_TICKS, 1);
        desc->definition->capsule_ops->tick(desc->capsule, delta_seconds, &tick);
        reach_host_apply_feature_tick_result(host, desc, &tick);
    }
    reach_host_finish_surface_presentations(host);
}

static reach_result reach_host_finish_update(reach_host *host)
{
    reach_host_apply_layout(host);

    host->dirty.layout = 0;
    host->dirty.render = 0;
    host->dirty.update_requested = 0;
    host->dirty.bar_visibility = 0;
    for (size_t index = 0; index < host->surface_count; ++index)
    {
        reach_surface_runtime *surface = host->feature_runtimes[index].surface;
        if (surface != nullptr)
        {
            surface->dirty_flags = 0;
        }
    }

    return REACH_OK;
}

static reach_result reach_host_frame_surfaces(reach_host *host, const reach_monitor_info *monitor,
                                              int32_t game_mode)
{
    reach_host_frame_context frame = {};
    frame.monitor_bounds = {(float)monitor->bounds.left, (float)monitor->bounds.top,
                            (float)(monitor->bounds.right - monitor->bounds.left),
                            (float)(monitor->bounds.bottom - monitor->bounds.top)};
    host->layout_dpi_scale = reach_host_monitor_dpi_scale(monitor);
    if (!game_mode)
    {
        host->has_layout = 1;
        reach_host_sync_edge_reveals(host, frame.monitor_bounds);
    }

    for (size_t index = 0; index < host->surface_count; ++index)
    {
        auto *runtime = &host->feature_runtimes[host->feature_layout_order[index]];
        if (game_mode && (runtime->definition->surface.behavior_flags &
                          REACH_SURFACE_BEHAVIOR_GAME_MODE_VISIBLE) == 0)
        {
            continue;
        }
        const reach_result result = reach_host_frame_registered_surface(host, runtime, &frame);
        if (result != REACH_OK)
        {
            return result;
        }
        reach_host_sync_surface_input_regions(host, runtime);
    }
    return REACH_OK;
}

static reach_result reach_host_update_game_mode_surfaces(reach_host *host, double delta_seconds)
{
    for (size_t index = 0; index < host->surface_count; ++index)
    {
        reach_feature_runtime *desc = &host->feature_runtimes[index];
        if ((desc->definition->surface.behavior_flags & REACH_SURFACE_BEHAVIOR_GAME_MODE_VISIBLE) ==
                0 ||
            desc->definition->capsule_ops == nullptr ||
            desc->definition->capsule_ops->tick == nullptr || !reach_host_presentation_ready(desc))
        {
            continue;
        }
        reach_feature_tick_result tick = {};
        reach_workload_add(REACH_WORKLOAD_CAPSULE_TICKS, 1);
        desc->definition->capsule_ops->tick(desc->capsule, delta_seconds, &tick);
        reach_host_apply_feature_tick_result(host, desc, &tick);
    }

    if (host->monitors.ops.primary != nullptr)
    {
        const reach_monitor_info *monitor = host->monitors.ops.primary(host->monitors.list);
        if (monitor != nullptr)
        {
            const reach_result result = reach_host_frame_surfaces(host, monitor, 1);
            if (result != REACH_OK)
            {
                return result;
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
    reach_workload_add(REACH_WORKLOAD_APP_UPDATES, 1);
    if (host->dirty.events_dispatched_this_cycle)
    {
        host->dirty.events_dispatched_this_cycle = 0;
    }
    else
    {
        (void)reach_host_dispatch_events(host);
        host->dirty.events_dispatched_this_cycle = 0;
    }

    reach_host_tick_pending_unpins(host, delta_seconds);
    reach_host_apply_app_launch_results(host);
    reach_host_apply_window_control_result(host);

    int32_t window_manager_dirty =
        host->window_manager.ops.needs_refresh != nullptr &&
        host->window_manager.ops.needs_refresh(host->window_manager.manager);
    if (window_manager_dirty)
    {
        reach_host_refresh_window_world(host);
        reach_host_apply_foreground_change(host);
    }

    reach_result monitor_result = reach_host_refresh_monitor_layout(host);
    if (monitor_result != REACH_OK)
    {
        return monitor_result;
    }

    reach_host_process_system_status_requests(host);
    (void)reach_host_update_game_mode(host);
    if (reach_host_game_mode_enabled(host))
    {
        return reach_host_update_game_mode_surfaces(host, delta_seconds);
    }

    reach_host_tick_features(host, delta_seconds);
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
        reach_host_invalidate_surface_content(host, REACH_SURFACE_REDRAW_CLOCK);
    }
    if (reach_input_language_service_tick_settle(host->input_language, delta_seconds,
                                                 reach_host_foreground_window(host)))
    {
        reach_host_invalidate_surface_content(host, REACH_SURFACE_REDRAW_INPUT_LANGUAGE);
        host->dirty.layout = 1;
    }

    if (host->monitors.list != nullptr && host->monitors.ops.count != nullptr &&
        host->monitors.ops.primary != nullptr && host->monitors.ops.count(host->monitors.list) > 0)
    {
        const reach_monitor_info *monitor = host->monitors.ops.primary(host->monitors.list);
        if (monitor == nullptr)
        {
            return REACH_ERROR;
        }
        REACH_ASSERT(monitor->primary || host->monitors.ops.count(host->monitors.list) == 1);
        const reach_result result = reach_host_frame_surfaces(host, monitor, 0);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    reach_host_sync_pointer_move_subscriptions(host);
    return reach_host_finish_update(host);
}

int32_t reach_host_frame_rate_hz(const reach_host *host)
{
    if (host == nullptr)
    {
        return 60;
    }
    reach_system_status_system_snapshot power = {};
    reach_system_status_read_system(host->system_status, &power);
    const reach_monitor_info *primary =
        host->monitors.list != nullptr && host->monitors.ops.primary != nullptr
            ? host->monitors.ops.primary(host->monitors.list)
            : nullptr;
    return reach_frame_rate_resolve(host->high_refresh_rate,
                                    primary != nullptr ? primary->refresh_rate_hz : 0,
                                    power.power_valid && power.power.battery_saver_on);
}

reach_theme_mode reach_host_theme_mode(const reach_host *host)
{
    const reach_theme *theme =
        host != nullptr && host->theme != nullptr ? host->theme : reach_theme_default();
    return theme->mode;
}

void reach_host_set_theme(reach_host *host, reach_theme_mode mode, const reach_accent_pair *accent)
{
    if (host == nullptr)
    {
        return;
    }

    reach_theme resolved = {};
    reach_theme_resolve(mode, accent, &resolved);
    const reach_theme *current = host->theme;
    if (current != nullptr)
        resolved.glass.intensity = current->glass.intensity;
    if (current == &host->theme_storage && current->mode == resolved.mode &&
        current->accent_fill.r == resolved.accent_fill.r &&
        current->accent_fill.g == resolved.accent_fill.g &&
        current->accent_fill.b == resolved.accent_fill.b)
    {
        return;
    }

    host->theme_storage = resolved;
    host->theme = &host->theme_storage;
    ++host->theme_revision;
    host->dirty.layout = 1;
    host->dirty.render = 1;
}

int32_t reach_host_needs_frame(const reach_host *host)
{
    if (host == nullptr)
    {
        return 0;
    }

    for (size_t index = 0; index < host->surface_count; ++index)
    {
        const reach_feature_runtime *runtime = &host->feature_runtimes[index];
        if (runtime->definition == nullptr || runtime->definition->capsule_ops == nullptr ||
            runtime->definition->capsule_ops->surface_geometry == nullptr ||
            (reach_host_game_mode_enabled(host) && (runtime->definition->surface.behavior_flags &
                                                    REACH_SURFACE_BEHAVIOR_GAME_MODE_VISIBLE) == 0))
        {
            continue;
        }
        reach_feature_surface_geometry geometry = {};
        runtime->definition->capsule_ops->surface_geometry(runtime->capsule, &geometry);
        if ((runtime->native_overlay_motion_pending && reach_host_surface_presented(runtime)) ||
            (runtime->surface != nullptr && runtime->surface->fallback_animation_active) ||
            (reach_motion_wait_ms(geometry.deadline_seconds) == 0 &&
             reach_host_presentation_ready(const_cast<reach_feature_runtime *>(runtime))))
        {
            return 1;
        }
    }

    if (reach_monitor_refresh_retry_due(&host->monitor_refresh_retry))
    {
        return 1;
    }
    if (reach_monitor_topology_confirmation_due(&host->monitor_topology))
    {
        return 1;
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
        for (size_t index = 0; index < host->surface_count; ++index)
        {
            const reach_feature_runtime *desc = &host->feature_runtimes[index];
            if ((desc->definition->surface.behavior_flags &
                 REACH_SURFACE_BEHAVIOR_GAME_MODE_VISIBLE) != 0 &&
                desc->definition->capsule_ops != nullptr &&
                desc->definition->capsule_ops->needs_frame != nullptr &&
                desc->definition->capsule_ops->needs_frame(desc->capsule) &&
                reach_host_presentation_ready(const_cast<reach_feature_runtime *>(desc)))
            {
                return 1;
            }
        }
        return 0;
    }

    if (host->dirty.render || reach_host_any_surface_dirty(host) ||
        reach_icon_service_work_pending(host->icon_service) ||
        reach_clock_minute_elapsed(host->clock) ||
        reach_input_language_service_settling(host->input_language))
    {
        return 1;
    }

    for (size_t index = 0; index < host->surface_count; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[index];
        if (desc->definition->capsule_ops != nullptr &&
            desc->definition->capsule_ops->needs_frame != nullptr &&
            desc->definition->capsule_ops->needs_frame(desc->capsule) &&
            reach_host_presentation_ready(const_cast<reach_feature_runtime *>(desc)))
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

    uint32_t feature_wait = REACH_CLOCK_WAIT_FOREVER;
    for (size_t index = 0; index < host->surface_count; ++index)
    {
        const reach_feature_runtime *runtime = &host->feature_runtimes[index];
        if (runtime->definition == nullptr || runtime->definition->capsule_ops == nullptr ||
            runtime->definition->capsule_ops->surface_geometry == nullptr ||
            (reach_host_game_mode_enabled(host) && (runtime->definition->surface.behavior_flags &
                                                    REACH_SURFACE_BEHAVIOR_GAME_MODE_VISIBLE) == 0))
        {
            continue;
        }
        reach_feature_surface_geometry geometry = {};
        runtime->definition->capsule_ops->surface_geometry(runtime->capsule, &geometry);
        uint32_t wait = reach_motion_wait_ms(geometry.deadline_seconds);
        if (wait == 0 &&
            !reach_host_presentation_ready(const_cast<reach_feature_runtime *>(runtime)))
            wait = REACH_CLOCK_WAIT_FOREVER;
        if (wait < feature_wait)
        {
            feature_wait = wait;
        }
    }

    if (host->monitor_refresh_retry.scheduled || host->monitor_topology.confirmation_scheduled)
    {
        uint32_t retry_wait = REACH_CLOCK_WAIT_FOREVER;
        if (host->monitor_refresh_retry.scheduled)
        {
            retry_wait = reach_monitor_refresh_retry_wait_ms(&host->monitor_refresh_retry);
        }
        if (host->monitor_topology.confirmation_scheduled)
        {
            uint32_t confirmation_wait =
                reach_monitor_topology_confirmation_wait_ms(&host->monitor_topology);
            if (retry_wait == REACH_CLOCK_WAIT_FOREVER || confirmation_wait < retry_wait)
            {
                retry_wait = confirmation_wait;
            }
        }
        if (reach_host_game_mode_enabled(host))
        {
            return retry_wait < feature_wait ? retry_wait : feature_wait;
        }
        uint32_t clock_wait = reach_clock_next_minute_delay_ms(host->clock);
        uint32_t wait = clock_wait == REACH_CLOCK_WAIT_FOREVER || retry_wait < clock_wait
                            ? retry_wait
                            : clock_wait;
        return wait < feature_wait ? wait : feature_wait;
    }
    if (reach_host_game_mode_enabled(host))
    {
        return reach_host_needs_frame(host)
                   ? (uint32_t)ceil(1000.0 / reach_host_frame_rate_hz(host))
                   : feature_wait;
    }
    uint32_t clock_wait = reach_clock_next_minute_delay_ms(host->clock);
    return clock_wait < feature_wait ? clock_wait : feature_wait;
}
