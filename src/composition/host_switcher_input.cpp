#include "host_internal.h"

/*
 * Switcher orchestration: composition is wiring-only here. The alt-tab ring rebuild,
 * selection, and visible-start now live in the switcher capsule
 * (reach_switcher_handle_event / reach_switcher_sync_windows). This file only borrows the
 * window context to the capsule and translates the returned action into surface visibility
 * + the window-control port call. OS-watching (the window-manager refresh) stays here.
 */

static void reach_host_apply_switcher_action(reach_host *host, reach_switcher_action action)
{
    switch (action.type)
    {
    case REACH_SWITCHER_ACTION_OPENED:
        reach_host_surface_transition_set(host, &host->switcher_transition, 1);
        host->switcher.dirty_flags = 1;
        break;
    case REACH_SWITCHER_ACTION_CHANGED:
        host->switcher.dirty_flags = 1;
        break;
    case REACH_SWITCHER_ACTION_CLOSED:
        reach_host_surface_transition_set(host, &host->switcher_transition, 0);
        host->switcher.dirty_flags = 1;
        break;
    case REACH_SWITCHER_ACTION_COMMITTED:
        reach_host_surface_transition_set(host, &host->switcher_transition, 0);
        host->switcher.dirty_flags = 1;
        if (action.window != 0)
        {
            (void)reach_host_schedule_window_control(host, REACH_WINDOW_CONTROL_ACTIVATE,
                                                      action.window);
        }
        break;
    case REACH_SWITCHER_ACTION_NONE:
    default:
        break;
    }
}

void reach_host_refresh_switcher_windows(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_switcher_action action = reach_switcher_sync_windows(host->switcher_capsule);
    reach_host_apply_switcher_action(host, action);
}

reach_result reach_host_handle_switcher_event(reach_host *host, const reach_ui_event *event)
{
    if (host == nullptr || event == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    /* OS-watching stays in composition: on switch-begin, close transients and refresh the
     * window list + foreground so the capsule rebuilds its ring from current data. */
    if (event->type == REACH_UI_EVENT_APP_SWITCH_BEGIN)
    {
        reach_host_close_transient_surfaces(host, 0);

        if (host->window_manager.ops.refresh != nullptr)
        {
            (void)host->window_manager.ops.refresh(host->window_manager.manager);
            (void)reach_host_refresh_open_windows(host, nullptr);
            if (host->window_manager.ops.foreground != nullptr)
            {
                reach_host_note_foreground_window(
                    host, host->window_manager.ops.foreground(host->window_manager.manager));
            }
        }
    }

    reach_switcher_action action = reach_switcher_handle_event(host->switcher_capsule, event);
    reach_host_apply_switcher_action(host, action);
    return REACH_OK;
}
