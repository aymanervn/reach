#include "host_internal.h"

void reach_host_set_tray_popup_open(reach_host *host, int32_t open)
{
    if (host == nullptr)
    {
        return;
    }

    int32_t next_open = open ? 1 : 0;
    if (!reach_top_bar_set_tray_popup_open(host->top_bar_capsule, next_open))
    {
        return;
    }

    reach_host_surface_transition_set(host, &host->tray_transition, next_open);
    if (next_open)
    {
        reach_host_surface_opening(host, REACH_SURFACE_ID_TRAY, REACH_SURFACE_ID_TOP_BAR);
        (void)reach_host_refresh_tray_items(host);
    }
    else
    {
        reach_host_request_bar_visibility_update(host);
    }
    reach_host_sync_popup_mouse_hook(host);
    host->top_bar.dirty_flags = 1;
    host->tray.dirty_flags = 1;
}

void reach_host_toggle_tray_popup(reach_host *host)
{
    if (host != nullptr)
    {
        reach_host_set_tray_popup_open(host,
                                       !reach_top_bar_tray_popup_is_open(host->top_bar_capsule));
    }
}

reach_result reach_host_refresh_tray_items(reach_host *host)
{
    return host != nullptr ? reach_top_bar_refresh_tray(host->top_bar_capsule) : REACH_OK;
}

reach_result reach_host_activate_tray_item(reach_host *host, uint32_t item_id,
                                           reach_tray_action action)
{
    if (host == nullptr)
    {
        return REACH_OK;
    }

    reach_host_set_tray_popup_open(host, 0);
    return reach_tray_service_activate(host->tray_service, item_id, action);
}

reach_result reach_host_apply_tray_pointer_action(reach_host *host, const reach_ui_event *event,
                                                  const reach_capsule_pointer_result *result)
{
    (void)event;
    if (host == nullptr || result == nullptr)
    {
        return REACH_OK;
    }

    reach_tray_action provider_action = REACH_TRAY_ACTION_LEFT_CLICK;
    if (result->action.kind == REACH_TOP_BAR_POINTER_ACTION_ACTIVATE_TRAY_RIGHT)
    {
        provider_action = REACH_TRAY_ACTION_RIGHT_CLICK;
    }
    else if (result->action.kind != REACH_TOP_BAR_POINTER_ACTION_ACTIVATE_TRAY_LEFT)
    {
        return REACH_OK;
    }

    return reach_host_activate_tray_item(host, static_cast<uint32_t>(result->action.id),
                                         provider_action);
}
