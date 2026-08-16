#include "host_internal.h"

float reach_host_stage_reveal_corner_size(reach_host *host)
{
    float scale = reach_host_layout_dpi_scale(host);
    return 4.0f * (scale > 0.0f ? scale : 1.0f);
}

void reach_host_sync_stage_reveal_corner(reach_host *host, reach_rect_f32 monitor_bounds)
{
    if (host == nullptr || host->stage_reveal_corner.hotspot == nullptr)
    {
        return;
    }

    reach_rect_f32 corner_bounds = {};
    corner_bounds.x = monitor_bounds.x;
    corner_bounds.y = monitor_bounds.y;
    corner_bounds.width = reach_host_stage_reveal_corner_size(host);
    corner_bounds.height = reach_host_stage_reveal_corner_size(host);

    if (!host->stage_reveal.corner_bounds_valid ||
        !reach_host_rect_equal(host->stage_reveal.corner_bounds, corner_bounds))
    {
        if (host->stage_reveal_corner.ops.set_bounds != nullptr &&
            host->stage_reveal_corner.ops.set_bounds(host->stage_reveal_corner.hotspot,
                                                     corner_bounds) == REACH_OK)
        {
            host->stage_reveal.corner_bounds = corner_bounds;
            host->stage_reveal.corner_bounds_valid = 1;
        }
    }

    if (!host->stage_reveal.corner_visible && host->stage_reveal_corner.ops.show != nullptr &&
        host->stage_reveal_corner.ops.show(host->stage_reveal_corner.hotspot) == REACH_OK)
    {
        host->stage_reveal.corner_visible = 1;
    }
    if (host->dock.window.ops.native_id != nullptr &&
        host->stage_reveal_corner.ops.place_behind != nullptr)
    {
        reach_window_id dock_id = host->dock.window.ops.native_id(host->dock.window.window);
        if (dock_id != 0)
        {
            (void)host->stage_reveal_corner.ops.place_behind(host->stage_reveal_corner.hotspot,
                                                             dock_id);
        }
    }
}

static const reach_monitor_info *reach_host_stage_monitor_for(reach_host *host,
                                                              reach_rect_f32 frame,
                                                              uint32_t *out_rank)
{
    REACH_ASSERT(out_rank != nullptr);
    *out_rank = 0;

    if (host == nullptr || host->monitors.list == nullptr || host->monitors.ops.count == nullptr ||
        host->monitors.ops.get == nullptr)
    {
        return nullptr;
    }

    size_t count = host->monitors.ops.count(host->monitors.list);
    if (count == 0)
    {
        return nullptr;
    }

    float center_x = frame.x + frame.width * 0.5f;
    float center_y = frame.y + frame.height * 0.5f;

    const reach_monitor_info *match = nullptr;
    for (size_t index = 0; index < count && match == nullptr; ++index)
    {
        const reach_monitor_info *monitor = host->monitors.ops.get(host->monitors.list, index);
        if (monitor == nullptr)
        {
            continue;
        }
        if (center_x >= (float)monitor->bounds.left && center_x < (float)monitor->bounds.right &&
            center_y >= (float)monitor->bounds.top && center_y < (float)monitor->bounds.bottom)
        {
            match = monitor;
        }
    }

    if (match == nullptr)
    {
        match = host->monitors.ops.primary != nullptr
                    ? host->monitors.ops.primary(host->monitors.list)
                    : host->monitors.ops.get(host->monitors.list, 0);
    }
    if (match == nullptr)
    {
        return nullptr;
    }

    uint32_t rank = 0;
    for (size_t index = 0; index < count; ++index)
    {
        const reach_monitor_info *monitor = host->monitors.ops.get(host->monitors.list, index);
        if (monitor == nullptr || monitor == match)
        {
            continue;
        }
        if (monitor->bounds.left < match->bounds.left ||
            (monitor->bounds.left == match->bounds.left && monitor->bounds.top < match->bounds.top))
        {
            rank++;
        }
    }

    *out_rank = rank;
    return match;
}

static int32_t reach_host_stage_monitor_is_portrait(const reach_monitor_info *monitor)
{
    if (monitor == nullptr)
    {
        return 0;
    }
    return (monitor->bounds.bottom - monitor->bounds.top) >
                   (monitor->bounds.right - monitor->bounds.left)
               ? 1
               : 0;
}

static size_t reach_host_collect_stage_windows(reach_host *host,
                                               reach_stage_open_window *out_windows,
                                               size_t capacity)
{
    if (host == nullptr || out_windows == nullptr || capacity == 0)
    {
        return 0;
    }

    const reach_window_snapshot *windows = reach_window_tracking_windows(host->window_tracking);
    size_t window_count = reach_window_tracking_window_count(host->window_tracking);
    if (windows == nullptr)
    {
        return 0;
    }

    size_t collected = 0;
    for (size_t index = 0; index < window_count && collected < capacity; ++index)
    {
        const reach_window_snapshot *snapshot = &windows[index];
        if (!snapshot->visible || snapshot->id == 0)
        {
            continue;
        }

        reach_rect_f32 frame = {};
        if (host->window_manager.ops.frame_bounds == nullptr)
        {
            continue;
        }
        reach_result frame_result =
            host->window_manager.ops.frame_bounds(host->window_manager.manager, snapshot->id, &frame);
        if (frame_result != REACH_OK || frame.width <= 0.0f || frame.height <= 0.0f)
        {
            continue;
        }

        reach_stage_open_window *entry = &out_windows[collected];
        *entry = {};
        entry->window = snapshot->id;
        entry->label = snapshot->title;
        entry->minimized = snapshot->minimized;
        entry->frame = frame;

        const reach_monitor_info *monitor =
            reach_host_stage_monitor_for(host, frame, &entry->monitor_index);
        entry->monitor_portrait = reach_host_stage_monitor_is_portrait(monitor);
        entry->icon_id = reach_icon_service_get(host->icon_service, snapshot->icon_ref,
                                                reach_host_icon_size_px(host));
        collected++;
    }

    if (collected < capacity)
    {
        reach_window_id desktop = host->wallpaper_surface.ops.desktop_window != nullptr
                                      ? host->wallpaper_surface.ops.desktop_window(
                                            host->wallpaper_surface.surface)
                                      : 0;
        if (desktop != 0)
        {
            static const uint16_t desktop_label[] = {'D', 'e', 's', 'k', 't', 'o', 'p', 0};
            reach_rect_f32 monitor_bounds = {};
            (void)reach_host_primary_monitor_bounds(host, &monitor_bounds);

            reach_stage_open_window *entry = &out_windows[collected];
            *entry = {};
            entry->window = desktop;
            entry->label = desktop_label;
            entry->desktop = 1;
            entry->frame = monitor_bounds;

            const reach_monitor_info *monitor =
                reach_host_stage_monitor_for(host, monitor_bounds, &entry->monitor_index);
            entry->monitor_portrait = reach_host_stage_monitor_is_portrait(monitor);
            entry->icon_id = host->wallpaper_image_id;
            collected++;
        }
    }

    return collected;
}

static void reach_host_release_stage_thumbnails(reach_host *host)
{
    if (host == nullptr || !host->stage_thumbnails_registered)
    {
        return;
    }

    for (size_t index = 0; index < REACH_STAGE_MAX_TILES; ++index)
    {
        host->stage_thumbnail_ids[index] = REACH_WINDOW_THUMBNAIL_NONE;
    }
    host->stage_thumbnails_registered = 0;

    if (host->window_thumbnails.ops.destroy_all == nullptr)
    {
        return;
    }
    (void)host->window_thumbnails.ops.destroy_all(host->window_thumbnails.thumbnails);
}

static void reach_host_register_stage_thumbnails(reach_host *host)
{
    if (host == nullptr || host->window_thumbnails.ops.create == nullptr ||
        host->window_thumbnails.ops.set_target == nullptr)
    {
        return;
    }

    reach_host_release_stage_thumbnails(host);
    host->stage_thumbnail_generation = reach_stage_tile_generation(host->stage_capsule);

    reach_window_id target = 0;
    if (host->stage.window.ops.native_id != nullptr)
    {
        target = host->stage.window.ops.native_id(host->stage.window.window);
    }
    if (target == 0)
    {
        return;
    }

    (void)host->window_thumbnails.ops.set_target(host->window_thumbnails.thumbnails, target);

    size_t count = reach_stage_thumbnail_count(host->stage_capsule);
    if (count > REACH_STAGE_MAX_TILES)
    {
        count = REACH_STAGE_MAX_TILES;
    }
    for (size_t index = count; index > 0; --index)
    {
        size_t tile_index = index - 1;
        reach_stage_thumbnail_placement placement = {};
        if (reach_stage_thumbnail_at(host->stage_capsule, tile_index, &placement) != REACH_OK ||
            placement.desktop)
        {
            continue;
        }

        reach_window_thumbnail_id id = REACH_WINDOW_THUMBNAIL_NONE;
        if (host->window_thumbnails.ops.create(host->window_thumbnails.thumbnails, placement.window,
                                               &id) == REACH_OK)
        {
            host->stage_thumbnail_ids[tile_index] = id;
            host->stage_thumbnails_registered = 1;
        }
    }
}

void reach_host_sync_stage_window_states(reach_host *host)
{
    if (host == nullptr || host->stage_capsule == nullptr)
    {
        return;
    }
    if (!reach_stage_is_open(host->stage_capsule))
    {
        return;
    }

    reach_stage_open_window windows[REACH_STAGE_MAX_TILES] = {};
    size_t count = reach_host_collect_stage_windows(host, windows, REACH_STAGE_MAX_TILES);
    if (reach_stage_update_windows(host->stage_capsule, windows, count))
    {
        reach_host_sync_stage_thumbnails(host);
        host->stage.dirty_flags = 1;
        host->dirty.render = 1;
        reach_host_request_update(host);
    }
}

void reach_host_sync_stage_thumbnails(reach_host *host)
{
    if (host == nullptr || host->window_thumbnails.ops.set_placement == nullptr)
    {
        return;
    }

    const reach_stage_state *state = reach_stage_state_ptr(host->stage_capsule);
    if (state == nullptr)
    {
        return;
    }

    if (host->stage_thumbnail_generation != reach_stage_tile_generation(host->stage_capsule))
    {
        reach_host_register_stage_thumbnails(host);
    }

    reach_rect_f32 stage_bounds = state->bounds;

    size_t count = reach_stage_thumbnail_count(host->stage_capsule);
    for (size_t index = 0; index < count && index < REACH_STAGE_MAX_TILES; ++index)
    {
        reach_window_thumbnail_id id = host->stage_thumbnail_ids[index];
        if (id == REACH_WINDOW_THUMBNAIL_NONE)
        {
            continue;
        }

        reach_stage_thumbnail_placement placement = {};
        if (reach_stage_thumbnail_at(host->stage_capsule, index, &placement) != REACH_OK)
        {
            continue;
        }

        reach_rect_f32 destination = placement.destination;
        destination.x -= stage_bounds.x;
        destination.y -= stage_bounds.y;

        (void)host->window_thumbnails.ops.set_placement(host->window_thumbnails.thumbnails, id,
                                                        destination, placement.opacity,
                                                        placement.visible);
    }
}

void reach_host_open_stage(reach_host *host)
{
    if (host == nullptr || host->stage_capsule == nullptr)
    {
        return;
    }
    if (reach_stage_is_open(host->stage_capsule))
    {
        return;
    }

    reach_rect_f32 monitor_bounds = {};
    if (!reach_host_primary_monitor_bounds(host, &monitor_bounds))
    {
        return;
    }

    if (host->monitors.ops.refresh != nullptr)
    {
        (void)host->monitors.ops.refresh(host->monitors.list);
    }

    if (host->window_manager.ops.refresh != nullptr)
    {
        (void)host->window_manager.ops.refresh(host->window_manager.manager);
        (void)reach_host_refresh_open_windows(host, nullptr);
    }

    reach_stage_open_window windows[REACH_STAGE_MAX_TILES] = {};
    size_t count = reach_host_collect_stage_windows(host, windows, REACH_STAGE_MAX_TILES);
    if (count == 0)
    {
        return;
    }
    if (reach_stage_open(host->stage_capsule, monitor_bounds, reach_host_layout_dpi_scale(host),
                         windows, count) != REACH_OK)
    {
        return;
    }

    reach_host_surface_opening(host, REACH_SURFACE_ID_STAGE,
                               REACH_SURFACE_ORIGIN_NONE);
    reach_host_surface_transition_set(host, &host->stage_transition, 1);
    reach_host_register_stage_thumbnails(host);
    reach_host_sync_stage_thumbnails(host);
    reach_host_request_update(host);
}

void reach_host_close_stage(reach_host *host)
{
    if (host == nullptr || host->stage_capsule == nullptr)
    {
        return;
    }
    if (!reach_stage_is_open(host->stage_capsule))
    {
        return;
    }
    if (!host->stage_topmost && host->stage.window.ops.set_topmost != nullptr &&
        host->stage.window.ops.set_topmost(host->stage.window.window, 1) == REACH_OK)
    {
        host->stage_topmost = 1;
    }
    host->stage_transition.close_seconds = 0.0;
    reach_stage_begin_close(host->stage_capsule);
    reach_host_request_update(host);
}

void reach_host_toggle_stage(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    if (reach_stage_is_open(host->stage_capsule))
    {
        reach_host_close_stage(host);
        return;
    }
    reach_host_open_stage(host);
}

void reach_host_cleanup_closed_stage(reach_host *host)
{
    if (host == nullptr || host->stage_capsule == nullptr)
    {
        return;
    }
    if (reach_stage_is_open(host->stage_capsule))
    {
        return;
    }

    reach_host_release_stage_thumbnails(host);
}

reach_result reach_host_apply_stage_pointer_action(reach_host *host, const reach_ui_event *event,
                                                   const reach_capsule_pointer_result *result)
{
    (void)event;
    if (host == nullptr || result == nullptr)
    {
        return REACH_OK;
    }

    if (result->action.kind == REACH_STAGE_ACTION_ACTIVATE_WINDOW)
    {
        reach_result activate_result = REACH_OK;
        if (result->action.window != 0)
        {
            activate_result = reach_host_schedule_window_control(
                host, REACH_WINDOW_CONTROL_ACTIVATE, result->action.window);
        }
        reach_host_close_stage(host);
        return activate_result;
    }

    if (result->action.kind == REACH_STAGE_ACTION_SHOW_DESKTOP)
    {
        reach_host_close_stage(host);
        return reach_host_schedule_minimize_open_windows(host);
    }

    if (result->action.kind == REACH_STAGE_ACTION_CLOSE_WINDOW)
    {
        host->stage.dirty_flags = 1;
        host->dirty.render = 1;
        reach_host_sync_stage_thumbnails(host);
        reach_host_request_update(host);
        return result->action.window != 0
                   ? reach_host_schedule_window_control(host, REACH_WINDOW_CONTROL_CLOSE,
                                                        result->action.window)
                   : REACH_OK;
    }

    return REACH_OK;
}

void reach_host_on_stage_reveal_corner(void *user, reach_screen_hotspot_event event)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr || event != REACH_SCREEN_HOTSPOT_ENTER)
    {
        return;
    }
    reach_host_open_stage(host);
}
