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
        reach_host_surface_opening(host, REACH_SURFACE_ID_TRAY, REACH_SURFACE_ID_TOP_BAR);
        (void)reach_host_refresh_tray_items(host);
    }
    else
    {
        reach_host_request_dock_visibility_update(host);
    }
    reach_host_sync_popup_mouse_hook(host);
    host->top_bar.dirty_flags = 1;
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

void reach_host_compute_tray_popup_layout(reach_host *host, reach_rect_f32 *out_bounds)
{
    if (host == nullptr || out_bounds == nullptr)
    {
        return;
    }

    const reach_theme *theme = host->theme != nullptr ? host->theme : reach_theme_default();
    const reach_top_bar_layout *top_bar_layout =
        &reach_top_bar_state_ptr(host->top_bar_capsule)->layout;

    reach_popup_anchor anchor = {};
    anchor.button =
        reach_top_bar_rect_to_screen(top_bar_layout, top_bar_layout->tray_overflow_button);
    anchor.bar_edge_y = top_bar_layout->bounds.y + top_bar_layout->bounds.height;
    anchor.bar_height = host->layout.dock.bounds.height;
    anchor.direction = REACH_POPUP_DROP_DOWN;

    reach_tray_layout_popup(host->tray_capsule, theme, &anchor,
                            reach_host_layout_dpi_scale(host), out_bounds);
}

reach_result reach_host_activate_tray_item(reach_host *host, uint32_t item_id,
                                           reach_tray_action action)
{
    if (host == nullptr || host->tray_provider.ops.activate == nullptr)
    {
        return REACH_OK;
    }

    if (host->tray.window.ops.set_topmost != nullptr)
    {
        (void)host->tray.window.ops.set_topmost(host->tray.window.window, 0);
    }
    return host->tray_provider.ops.activate(host->tray_provider.provider, item_id, action);
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

    return reach_host_activate_tray_item(host, static_cast<uint32_t>(result->action.id),
                                        provider_action);
}
