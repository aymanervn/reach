#include "host_internal.h"

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

    if (report.items_changed)
    {
        reach_dock_mark_items_changed(host->dock_capsule);
    }
    if (report.icon_identity_changed)
    {

        host->dock.dirty_flags = 1;
        host->switcher.dirty_flags = 1;
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
    reach_host_refresh_switcher_windows(host);
    host->dock.dirty_flags = 1;
    host->switcher.dirty_flags = 1;

    reach_host_close_activating_surfaces_on_focus_loss(host);
    reach_host_request_update(host);
}

int32_t reach_host_window_is_minimized(const reach_host *host, uintptr_t window_id)
{
    return host != nullptr &&
           reach_window_tracking_window_is_minimized(host->window_tracking, window_id);
}
