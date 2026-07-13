#include "host_internal.h"

void reach_host_set_tray_popup_open(reach_host *host, int32_t open)
{
    if (host == nullptr)
    {
        return;
    }

    int32_t next_open = open ? 1 : 0;
    if (!reach_tray_set_popup_open(host->tray_capsule, next_open))
    {
        return;
    }

    reach_host_surface_transition_set(host, &host->tray_transition, next_open);
    if (next_open)
    {
        if (host->tray.window.ops.set_topmost != nullptr)
        {
            (void)host->tray.window.ops.set_topmost(host->tray.window.window, 1);
        }
        reach_host_close_other_popups(host, REACH_SURFACE_ID_TRAY);
        (void)reach_host_refresh_tray_items(host);
    }
    else
    {
        reach_host_request_dock_visibility_update(host);
    }
    reach_host_sync_popup_mouse_hook(host);
    host->dock.dirty_flags = 1;
    host->tray.dirty_flags = 1;
}

void reach_host_toggle_tray_popup(reach_host *host)
{
    if (host != nullptr)
    {
        reach_host_set_tray_popup_open(host, !reach_tray_popup_is_open(host->tray_capsule));
    }
}

reach_result reach_host_refresh_tray_items(reach_host *host)
{
    return host != nullptr ? reach_tray_refresh(host->tray_capsule, &host->tray_provider)
                           : REACH_OK;
}

void reach_host_compute_tray_popup_layout(reach_host *host, const reach_dock_layout *dock_layout,
                                          reach_rect_f32 *out_bounds)
{
    if (host == nullptr || dock_layout == nullptr || out_bounds == nullptr)
    {
        return;
    }

    const reach_theme *theme = host->theme != nullptr ? host->theme : reach_theme_default();
    reach_tray_layout_popup(host->tray_capsule, theme, dock_layout,
                            reach_host_layout_dpi_scale(host), out_bounds);
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
    if (result->action.kind == REACH_TRAY_POINTER_ACTION_ACTIVATE_RIGHT)
    {
        provider_action = REACH_TRAY_ACTION_RIGHT_CLICK;
    }
    else if (result->action.kind != REACH_TRAY_POINTER_ACTION_ACTIVATE_LEFT)
    {
        return REACH_OK;
    }

    if (host->tray_provider.ops.activate == nullptr)
    {
        return REACH_OK;
    }

    if (host->tray.window.ops.set_topmost != nullptr)
    {
        (void)host->tray.window.ops.set_topmost(host->tray.window.window, 0);
    }
    return host->tray_provider.ops.activate(
        host->tray_provider.provider, static_cast<uint32_t>(result->action.id), provider_action);
}
