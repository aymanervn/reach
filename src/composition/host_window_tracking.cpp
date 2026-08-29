#include "host_internal.h"

static int32_t reach_host_window_center_on_primary_monitor(reach_host *host, reach_window_id window)
{
    if (host == nullptr || window == 0)
    {
        return 0;
    }

    const reach_window_snapshot *snapshot =
        reach_window_tracking_window_by_id(host->window_tracking, window);
    if (snapshot == nullptr || !snapshot->visible || snapshot->minimized)
    {
        return 0;
    }

    reach_rect_f32 monitor = {};
    reach_rect_f32 bounds = {};
    if (!reach_host_primary_monitor_bounds(host, &monitor) ||
        reach_app_control_window_bounds(host->app_control, window, &bounds) != REACH_OK)
    {
        return 0;
    }

    float center_x = bounds.x + bounds.width * 0.5f;
    float center_y = bounds.y + bounds.height * 0.5f;
    return center_x >= monitor.x && center_x < monitor.x + monitor.width && center_y >= monitor.y &&
           center_y < monitor.y + monitor.height;
}

static int32_t reach_host_pointer_primary_membership(reach_host *host, int32_t *out_inside)
{
    if (out_inside == nullptr)
    {
        return 0;
    }
    *out_inside = 0;

    reach_rect_f32 monitor = {};
    reach_point_i32 pointer = {};
    if (!reach_host_primary_monitor_bounds(host, &monitor) ||
        !reach_host_get_pointer_position(host, &pointer))
    {
        return 0;
    }

    *out_inside = (float)pointer.x >= monitor.x && (float)pointer.x < monitor.x + monitor.width &&
                  (float)pointer.y >= monitor.y && (float)pointer.y < monitor.y + monitor.height;
    return 1;
}

static void reach_host_update_window_manipulation(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_window_id active_window = 0;
    if (host->window_manipulation.programmatic.active &&
        host->window_manipulation.programmatic_relevant)
    {
        active_window = host->window_manipulation.programmatic.window;
    }
    else if (host->window_manipulation.manual.active && host->window_manipulation.manual_relevant)
    {
        active_window = host->window_manipulation.manual.window;
    }
    int32_t relevant = active_window != 0;
    int32_t began = relevant && !host->window_manipulation.relevant;
    int32_t changed = relevant != host->window_manipulation.relevant ||
                      active_window != host->window_manipulation.active_window;

    host->window_manipulation.active_window = active_window;
    host->window_manipulation.relevant = relevant;
    if (!changed)
    {
        return;
    }

    if (began)
    {
        reach_host_close_surface_classes(host, reach_surface_class_bit(REACH_SURFACE_CLASS_POPUP),
                                         0);
    }
    reach_host_invalidate_bar_coverage(host);
    reach_host_request_bar_visibility_update(host);
}

void reach_host_sync_window_manipulation(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_window_manipulation manipulation = {};
    if (host->input_source.ops.get_window_manipulation != nullptr)
    {
        (void)host->input_source.ops.get_window_manipulation(host->input_source.source,
                                                             &manipulation);
    }
    int32_t started =
        manipulation.active && (!host->window_manipulation.manual.active ||
                                host->window_manipulation.manual.window != manipulation.window);
    int32_t pointer_inside = 0;
    int32_t pointer_valid =
        manipulation.active && reach_host_pointer_primary_membership(host, &pointer_inside);
    host->window_manipulation.manual = manipulation;
    if (started)
    {
        host->window_manipulation.manual_relevant =
            pointer_valid ? pointer_inside
                          : reach_host_window_center_on_primary_monitor(host, manipulation.window);
    }
    else if (!manipulation.active)
    {
        host->window_manipulation.manual_relevant = 0;
    }
    else if (pointer_valid)
    {
        host->window_manipulation.manual_relevant = pointer_inside;
    }
    reach_host_update_window_manipulation(host);
}

void reach_host_begin_programmatic_window_manipulation(reach_host *host, reach_window_id window)
{
    if (host == nullptr || window == 0)
    {
        return;
    }
    host->window_manipulation.programmatic.window = window;
    host->window_manipulation.programmatic.active = 1;
    host->window_manipulation.programmatic_relevant =
        reach_host_window_center_on_primary_monitor(host, window);
    reach_host_update_window_manipulation(host);
}

void reach_host_end_programmatic_window_manipulation(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }
    host->window_manipulation.programmatic = {};
    host->window_manipulation.programmatic_relevant = 0;
    reach_host_update_window_manipulation(host);
}

void reach_host_refresh_window_world(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_host_invalidate_bar_coverage(host);
    if (host->window_manager.ops.refresh != nullptr)
    {
        (void)host->window_manager.ops.refresh(host->window_manager.manager);
    }

    int32_t open_windows_changed = 0;
    (void)reach_host_refresh_open_windows(host, &open_windows_changed);
    if (open_windows_changed)
    {
        reach_host_notify_windows_changed(host);
        host->dock.dirty_flags = 1;
        host->top_bar.dirty_flags = 1;
    }
    reach_host_request_update(host);
}

void reach_host_notify_windows_changed(reach_host *host)
{
    reach_feature_notification notification = {};
    notification.kind = REACH_FEATURE_NOTIFICATION_WINDOWS_CHANGED;
    notification.windows.changed = 1;
    notification.present = 1;
    reach_host_notify_registered_features(host, &notification);
}

void reach_host_notify_windows_refreshed(reach_host *host,
                                         const reach_window_tracking_refresh_report *report)
{
    if (report == nullptr)
    {
        return;
    }
    reach_feature_notification notification = {};
    notification.kind = REACH_FEATURE_NOTIFICATION_WINDOWS_CHANGED;
    notification.windows = *report;
    notification.windows.changed = 0;
    notification.present = 1;
    reach_host_notify_registered_features(host, &notification);
}

void reach_host_notify_display_changed(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_feature_notification notification = {};
    notification.kind = REACH_FEATURE_NOTIFICATION_DISPLAY_CHANGED;
    notification.present = 1;
    notification.display.icon_size_px = reach_host_icon_size_px(host);
    notification.display.dpi_scale = reach_host_layout_dpi_scale(host);
    notification.display.desktop_window =
        host->wallpaper_surface.ops.desktop_window != nullptr
            ? host->wallpaper_surface.ops.desktop_window(host->wallpaper_surface.surface)
            : 0;
    (void)reach_host_primary_monitor_bounds(host, &notification.display.primary_bounds);

    if (host->monitors.list != nullptr && host->monitors.ops.count != nullptr &&
        host->monitors.ops.get != nullptr)
    {
        size_t count = host->monitors.ops.count(host->monitors.list);
        for (size_t index = 0; index < count && index < REACH_DISPLAY_MAX_MONITORS; ++index)
        {
            const reach_monitor_info *monitor = host->monitors.ops.get(host->monitors.list, index);
            if (monitor == nullptr)
            {
                continue;
            }
            reach_rect_f32 *bounds =
                &notification.display.monitors[notification.display.monitor_count++];
            bounds->x = (float)monitor->bounds.left;
            bounds->y = (float)monitor->bounds.top;
            bounds->width = (float)(monitor->bounds.right - monitor->bounds.left);
            bounds->height = (float)(monitor->bounds.bottom - monitor->bounds.top);
        }
    }

    reach_host_notify_registered_features(host, &notification);
}

void reach_host_notify_config_changed(reach_host *host, const reach_config_snapshot *snapshot)
{
    if (host == nullptr || snapshot == nullptr)
    {
        return;
    }
    reach_feature_notification notification = {};
    notification.kind = REACH_FEATURE_NOTIFICATION_CONFIG_CHANGED;
    notification.config = snapshot;
    notification.present = 1;
    reach_host_notify_registered_features(host, &notification);
}

void reach_host_notify_popups_closed(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }
    reach_feature_notification notification = {};
    notification.kind = REACH_FEATURE_NOTIFICATION_POPUPS_CLOSED;
    notification.present = 1;
    reach_host_notify_registered_features(host, &notification);
}

void reach_host_notify_pinned_apps_changed(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }
    reach_feature_notification notification = {};
    notification.kind = REACH_FEATURE_NOTIFICATION_PINNED_APPS_CHANGED;
    notification.pinned_apps = host->pinned_apps;
    notification.pinned_app_count = host->pinned_app_count;
    notification.present = 1;
    reach_host_notify_registered_features(host, &notification);
}

void reach_host_note_foreground_window(reach_host *host, uintptr_t foreground_window)
{
    if (host != nullptr)
    {
        reach_window_tracking_note_foreground(host->window_tracking, foreground_window);
    }
}

reach_result reach_host_refresh_open_windows(reach_host *host, int32_t *out_changed)
{
    if (host == nullptr)
    {
        if (out_changed != nullptr)
        {
            *out_changed = 0;
        }
        return REACH_OK;
    }

    reach_window_tracking_refresh_report report = {};
    reach_result result = reach_window_tracking_refresh(host->window_tracking, &report);

    if (report.items_changed || report.icon_identity_changed)
    {
        reach_host_notify_windows_refreshed(host, &report);
    }

    if (out_changed != nullptr)
    {
        *out_changed = report.changed;
    }
    return result;
}

void reach_host_apply_foreground_change(reach_host *host)
{
    if (host == nullptr || host->foreground_watcher.ops.foreground == nullptr)
    {
        return;
    }

    uintptr_t foreground =
        host->foreground_watcher.ops.foreground(host->foreground_watcher.watcher);

    if (foreground == 0 || foreground == reach_host_foreground_window(host) ||
        reach_window_tracking_window_by_id(host->window_tracking, foreground) == nullptr)
    {
        return;
    }

    reach_host_note_foreground_window(host, foreground);
    reach_host_invalidate_bar_coverage(host);
    reach_host_notify_windows_changed(host);
    if (reach_input_language_service_refresh(host->input_language, foreground))
    {
        host->top_bar.dirty_flags = 1;
        host->dirty.layout = 1;
    }
    host->dock.dirty_flags = 1;
    host->top_bar.dirty_flags = 1;
    host->switcher.dirty_flags = 1;

    reach_host_close_activating_surfaces_on_focus_loss(host);
    reach_host_request_update(host);
}

int32_t reach_host_window_is_minimized(const reach_host *host, uintptr_t window_id)
{
    return host != nullptr &&
           reach_window_tracking_window_is_minimized(host->window_tracking, window_id);
}
