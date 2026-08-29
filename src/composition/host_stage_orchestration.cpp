#include "host_internal.h"

static const reach_monitor_info *
reach_host_stage_monitor_for(reach_host *host, reach_rect_f32 frame, uint32_t *out_rank)
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
        reach_result frame_result = host->window_manager.ops.frame_bounds(
            host->window_manager.manager, snapshot->id, &frame);
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
        reach_window_id desktop =
            host->wallpaper_surface.ops.desktop_window != nullptr
                ? host->wallpaper_surface.ops.desktop_window(host->wallpaper_surface.surface)
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
            collected++;
        }
    }

    return collected;
}

void reach_host_sync_stage_window_states(reach_host *host)
{
    if (host == nullptr ||
        reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE) == nullptr)
    {
        return;
    }
    if (!reach_stage_is_open(reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE)))
    {
        return;
    }

    reach_stage_open_window windows[REACH_STAGE_MAX_TILES] = {};
    size_t count = reach_host_collect_stage_windows(host, windows, REACH_STAGE_MAX_TILES);
    if (reach_stage_update_windows(
            reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE), windows, count))
    {
        host->stage.dirty_flags = 1;
        host->dirty.render = 1;
        reach_host_request_update(host);
    }
}

void reach_host_open_stage(reach_host *host)
{
    if (host == nullptr ||
        reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE) == nullptr)
    {
        return;
    }
    if (reach_stage_is_open(reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE)))
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
    if (reach_stage_open(reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE),
                         monitor_bounds, reach_host_layout_dpi_scale(host), windows,
                         count) != REACH_OK)
    {
        return;
    }

    reach_host_surface_opening(host, REACH_SURFACE_ID_STAGE, REACH_SURFACE_ORIGIN_NONE);
    reach_host_surface_transition_set(host, &host->stage_transition, 1);
    reach_host_request_update(host);
}

void reach_host_close_stage(reach_host *host)
{
    if (host == nullptr ||
        reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE) == nullptr)
    {
        return;
    }
    if (!reach_stage_is_open(reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE)))
    {
        return;
    }
    reach_stage_open_window windows[REACH_STAGE_MAX_TILES] = {};
    size_t count = reach_host_collect_stage_windows(host, windows, REACH_STAGE_MAX_TILES);
    reach_stage_refresh_tile_frames(
        reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE), windows, count);

    host->stage_transition.close_seconds = 0.0;
    reach_stage_begin_close(reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE));
    reach_host_request_update(host);
}

void reach_host_toggle_stage(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    if (reach_stage_is_open(reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE)))
    {
        reach_host_close_stage(host);
        return;
    }
    reach_host_open_stage(host);
}

void reach_host_on_stage_edge_reveal(reach_host *host, reach_screen_hotspot_event event)
{
    if (host == nullptr || event != REACH_SCREEN_HOTSPOT_ENTER)
    {
        return;
    }
    reach_host_open_stage(host);
}
