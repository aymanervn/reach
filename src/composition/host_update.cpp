#include "host_internal.h"

#include <math.h>
#include <stdio.h>
#include <time.h>

int32_t reach_host_dock_icon_size_px(const reach_host *host)
{
    const reach_theme *theme =
        host != nullptr && host->theme != nullptr ? host->theme : reach_theme_default();

    float dock_height = host != nullptr ? host->dock_config.height : 48.0f;
    float dpi_scale = reach_host_layout_dpi_scale(host);
    float icon_box_size = reach_theme_icon_box_size(theme, dock_height) * dpi_scale;

    int32_t requested = (int32_t)ceilf(icon_box_size * 4.0f);
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

static void reach_host_update_clock_text(reach_host *host)
{
    if (host != nullptr && reach_dock_update_clock(host->dock_capsule))
    {
        host->dock.dirty_flags = 1;
    }
}

static void reach_host_tick_animations(reach_host *host, double delta_seconds)
{
    reach_animation_manager_tick(&host->animations, delta_seconds);

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->capsule_ops == nullptr || desc->capsule_ops->tick == nullptr)
        {
            continue;
        }
        reach_feature_tick_result tick = {};
        desc->capsule_ops->tick(desc->capsule, delta_seconds, &tick);
        if (tick.redraw && desc->surface != nullptr)
        {
            desc->surface->dirty_flags = 1;
        }
        if (tick.relayout)
        {
            host->dirty.layout = 1;
        }
        if (tick.request_update)
        {
            reach_host_request_update(host);
        }
    }
    int32_t launcher_transition_was_visible =
        reach_host_surface_transition_visible(&host->launcher_transition);
    reach_host_surface_transition_finish(host, &host->launcher_transition);
    if (launcher_transition_was_visible &&
        !reach_host_surface_transition_visible(&host->launcher_transition) &&
        !reach_launcher_is_open(host->launcher_capsule))
    {
        reach_host_cleanup_closed_launcher(host);
        host->dirty.layout = 1;
        host->launcher.dirty_flags = 1;
    }
    reach_host_surface_transition_finish(host, &host->tray_transition);
    reach_host_surface_transition_finish(host, &host->quick_settings_transition);
    reach_host_surface_transition_finish(host, &host->switcher_transition);
    reach_host_surface_transition_finish(host, &host->context_menu_transition);
    reach_host_surface_transition_finish(host, &host->clipboard_transition);

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
    reach_host_tick_animations(host, delta_seconds);
    reach_host_drain_now_playing_retired_covers(host);
    reach_host_process_deferred_launcher_app_launch(host);
    reach_host_process_clipboard_refresh(host);
    if (reach_clipboard_tick_scroll(host->clipboard_capsule, delta_seconds))
    {
        host->dirty.layout = 1;
        host->clipboard_surface.dirty_flags = 1;
        reach_host_request_update(host);
    }

    if (reach_host_can_move_dock_without_redraw(host))
    {
        return reach_host_move_dock_animation_frame(host);
    }

    reach_host_process_quick_settings_changes(host, delta_seconds);

    host->popup_hook_reassert_seconds += delta_seconds;
    if (host->popup_hook_reassert_seconds >= 2.0)
    {
        host->popup_hook_reassert_seconds = 0.0;
        reach_host_sync_popup_mouse_hook(host);
    }
    reach_host_drain_icon_evictions(host);
    reach_host_update_clock_text(host);
    int32_t window_manager_dirty =
        host->window_manager.ops.needs_refresh != nullptr &&
        host->window_manager.ops.needs_refresh(host->window_manager.manager);
    if (window_manager_dirty && host->window_manager.ops.refresh != nullptr)
    {
        (void)host->window_manager.ops.refresh(host->window_manager.manager);

        int32_t open_windows_changed = 0;
        (void)reach_host_refresh_open_windows(host, &open_windows_changed);

        uintptr_t foreground_window =
            host->window_manager.ops.foreground != nullptr
                ? host->window_manager.ops.foreground(host->window_manager.manager)
                : 0;
        int32_t foreground_changed = reach_host_foreground_window(host) != foreground_window;
        reach_host_note_foreground_window(host, foreground_window);

        if (open_windows_changed || foreground_changed)
        {
            reach_host_refresh_switcher_windows(host);
            host->dock.dirty_flags = 1;
            host->switcher.dirty_flags = 1;
        }

        if (foreground_changed && foreground_window != 0 && reach_launcher_is_open(host->launcher_capsule))
        {
            reach_host_clear_launcher_restore_window(host);
            reach_host_close_launcher_without_focus_restore(host);
        }
    }
    (void)reach_host_update_game_mode(host);
    int32_t game_mode = reach_host_game_mode_enabled(host);
    if (!game_mode && reach_tray_state_ptr(host->tray_capsule)->popup_open &&
        host->tray_provider.ops.needs_refresh != nullptr &&
        host->tray_provider.ops.needs_refresh(host->tray_provider.provider))
    {
        (void)reach_host_refresh_tray_items(host);
        host->tray.dirty_flags = 1;
    }

    reach_result monitor_result = reach_host_refresh_monitor_layout(host);
    if (monitor_result != REACH_OK)
    {
        return monitor_result;
    }

    if (host->launcher.window.ops.set_bounds != nullptr && host->monitors.list != nullptr &&
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

        if (host->launcher.renderer.ops.begin_frame != nullptr)
        {
            reach_ui_layout_input input = {};
            input.monitor_bounds = bounds;
            input.work_area = bounds;
            input.dpi_scale = reach_host_monitor_dpi_scale(monitor);
            input.pinned_app_count = host->pinned_app_count;
            host->layout_dpi_scale = input.dpi_scale;

            reach_ui_layout layout = {};
            reach_result dock_layout_result =
                reach_dock_layout_compute(&host->dock_config, &input, &layout.dock);
            reach_result launcher_layout_result = reach_launcher_layout_compute(
                &reach_launcher_state_ptr(host->launcher_capsule)->model, &input, &layout.launcher);
            if (dock_layout_result == REACH_OK && launcher_layout_result == REACH_OK)
            {
                reach_dock_build_context build_ctx = reach_host_dock_build_context(host);
                if (reach_dock_take_items_changed(host->dock_capsule))
                {
                    reach_dock_rebuild_items(host->dock_capsule, &build_ctx,
                                             host->has_layout ? &host->layout.dock : nullptr,
                                             &layout.dock);
                }
                else
                {
                    reach_dock_build_layout(host->dock_capsule, &build_ctx, &layout.dock);
                }

                if (reach_dock_slots_animating(host->dock_capsule))
                {
                    host->dock.dirty_flags = 1;
                }
                reach_rect_f32 shown_dock_bounds = layout.dock.bounds;
                reach_rect_f32 animated_dock_bounds =
                    game_mode
                        ? shown_dock_bounds
                        : reach_host_reconcile_dock_visibility(host, shown_dock_bounds, bounds);
                if (game_mode)
                {
                    if (host->dock_reveal_edge.ops.hide != nullptr)
                    {
                        (void)host->dock_reveal_edge.ops.hide(host->dock_reveal_edge.edge);
                    }
                    host->dock_reveal.edge_visible = 0;
                }

                layout.dock.bounds = animated_dock_bounds;

                float dock_left_offset = bounds.x - layout.dock.bounds.x;
                float dock_right_offset =
                    bounds.x + bounds.width - (layout.dock.bounds.x + layout.dock.bounds.width);
                float dock_x_offset = 0.0f;
                if (dock_left_offset > 0.0f)
                {
                    dock_x_offset = dock_left_offset;
                }
                else if (dock_right_offset < 0.0f)
                {
                    dock_x_offset = dock_right_offset;
                }

                if (dock_x_offset != 0.0f)
                {
                    layout.dock.bounds.x += dock_x_offset;
                }

                int32_t dock_layout_changed =
                    !host->has_layout ||
                    !reach_host_rect_equal(host->layout.dock.bounds, layout.dock.bounds);
                int32_t launcher_layout_changed =
                    !host->has_layout ||
                    !reach_host_rect_equal(host->layout.launcher.bounds, layout.launcher.bounds);

                host->layout = layout;
                host->has_layout = 1;
                reach_launcher_set_pointer_context(host->launcher_capsule,
                                                   &host->layout.launcher,
                                                   host->pinned_apps,
                                                   host->pinned_app_count);

                reach_host_frame_context frame_ctx = {};
                frame_ctx.game_mode = game_mode;
                frame_ctx.monitor_bounds = bounds;
                frame_ctx.dock_layout_changed = dock_layout_changed;
                frame_ctx.launcher_layout_changed = launcher_layout_changed;
                reach_surface_id frame_order[REACH_HOST_SURFACE_COUNT];
                size_t frame_count = 0;
                for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
                {
                    const reach_surface_desc *desc = &host->surface_descs[index];
                    if (desc->frame == nullptr)
                    {
                        continue;
                    }
                    size_t at = frame_count;
                    while (at > 0 && host->surface_descs[frame_order[at - 1]].frame_priority >
                                         desc->frame_priority)
                    {
                        frame_order[at] = frame_order[at - 1];
                        --at;
                    }
                    frame_order[at] = desc->id;
                    ++frame_count;
                }
                for (size_t index = 0; index < frame_count; ++index)
                {
                    result = host->surface_descs[frame_order[index]].frame(host, &frame_ctx);
                    if (result != REACH_OK)
                    {
                        return result;
                    }
                }
            }
        }
    }
    host->dirty.layout = 0;
    host->dirty.render = 0;
    host->dirty.update_requested = 0;
    host->dock.dirty_flags = 0;
    host->launcher.dirty_flags = 0;
    host->tray.dirty_flags = 0;
    host->switcher.dirty_flags = 0;
    host->context_menu.dirty_flags = 0;
    host->quick_settings.dirty_flags = 0;
    host->clipboard_surface.dirty_flags = 0;

    return REACH_OK;
}

int32_t reach_host_needs_frame(const reach_host *host)
{
    int32_t window_manager_needs_refresh = 0;

    if (host != nullptr)
    {
        if (host->window_manager.manager != nullptr &&
            host->window_manager.ops.needs_refresh != nullptr)
        {
            window_manager_needs_refresh =
                host->window_manager.ops.needs_refresh(host->window_manager.manager);
        }
    }

    if (host == nullptr)
    {
        return 0;
    }

    if (host->dirty.update_requested || window_manager_needs_refresh || host->dirty.render ||
        reach_host_any_surface_dirty(host) ||
        reach_icon_service_work_pending(host->icon_service) ||
        reach_host_config_reload_work_pending(host) ||
        reach_animation_manager_any_active(&host->animations))
    {
        return 1;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->capsule_ops != nullptr && desc->capsule_ops->needs_frame != nullptr &&
            desc->capsule_ops->needs_frame(desc->capsule))
        {
            return 1;
        }
    }
    return 0;
}
