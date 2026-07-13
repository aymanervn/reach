#include "host_internal.h"

/*
 * Window-tracking shims (read model only). The snapshot cache and focus
 * history live in the window-tracking SERVICE (services/window_tracking);
 * composition translates its refresh reports into dock rebuilds and
 * render-icon redraws. Window ACTIONS (activate/minimize/close) go through
 * the app_control service (host_app_control.cpp).
 */

/* Case-insensitive (ASCII) text equality — paths AND app-user-model ids. */
void reach_host_note_foreground_window(reach_host *host, uintptr_t foreground_window)
{
    if (host != nullptr)
    {
        reach_window_tracking_note_foreground(host->window_tracking, foreground_window);
    }
}

/*
 * Refresh translator: the service reports what changed; composition applies
 * the cross-feature effects (dock item rebuild, render-icon slot migration
 * against the pre-refresh identity snapshot).
 */
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
        /* An icon ref changed under a window: the pull-model gets pick up the
           new path on the next draw; just redraw the icon surfaces. */
        host->dock.dirty_flags = 1;
        host->switcher.dirty_flags = 1;
    }

    if (out_changed != nullptr)
    {
        *out_changed = report.changed;
    }
    return result;
}

int32_t reach_host_window_is_minimized(const reach_host *host, uintptr_t window_id)
{
    return host != nullptr &&
           reach_window_tracking_window_is_minimized(host->window_tracking, window_id);
}
