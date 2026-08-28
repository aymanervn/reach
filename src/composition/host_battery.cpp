#include "host_internal.h"

static reach_battery_open_context reach_host_battery_open_context(reach_host *host)
{
    reach_battery_open_context ctx = {};
    ctx.theme = host->theme != nullptr ? host->theme : reach_theme_default();
    ctx.dpi_scale = reach_host_layout_dpi_scale(host);
    const reach_top_bar_layout *top_bar_layout =
        &reach_top_bar_state_ptr(
             reach_host_feature_capsule<reach_top_bar>(host, REACH_SURFACE_ID_TOP_BAR))
             ->layout;
    ctx.anchor_button =
        reach_top_bar_rect_to_screen(top_bar_layout, top_bar_layout->battery_button);
    reach_rect_f32 monitor = {};
    if (reach_host_primary_monitor_bounds(host, &monitor))
    {
        ctx.monitor = monitor;
    }
    ctx.bar_edge_y = top_bar_layout->bounds.y + top_bar_layout->bounds.height;
    ctx.drop_direction = REACH_POPUP_DROP_DOWN;
    return ctx;
}

void reach_host_refresh_battery_power(reach_host *host)
{
    if (host == nullptr ||
        reach_host_feature_capsule<reach_battery>(host, REACH_SURFACE_ID_BATTERY) == nullptr)
    {
        return;
    }

    if (reach_battery_refresh_power(
            reach_host_feature_capsule<reach_battery>(host, REACH_SURFACE_ID_BATTERY)))
    {
        reach_host_sync_battery_saver_pending(host);
        host->battery.dirty_flags = 1;
        host->dirty.render = 1;
    }
}

void reach_host_sync_battery_saver_pending(reach_host *host)
{
    if (host == nullptr ||
        reach_host_feature_capsule<reach_battery>(host, REACH_SURFACE_ID_BATTERY) == nullptr ||
        reach_host_feature_capsule<reach_top_bar>(host, REACH_SURFACE_ID_TOP_BAR) == nullptr)
    {
        return;
    }

    const reach_battery_model *model =
        &reach_battery_state_ptr(
             reach_host_feature_capsule<reach_battery>(host, REACH_SURFACE_ID_BATTERY))
             ->model;
    const reach_top_bar_state *top_bar = reach_top_bar_state_ptr(
        reach_host_feature_capsule<reach_top_bar>(host, REACH_SURFACE_ID_TOP_BAR));
    if (top_bar->battery_saver_pending == model->saver_pending &&
        top_bar->battery_saver_pending_enabled == model->saver_pending_enabled)
    {
        return;
    }

    reach_top_bar_set_battery_saver_pending(
        reach_host_feature_capsule<reach_top_bar>(host, REACH_SURFACE_ID_TOP_BAR),
        model->saver_pending, model->saver_pending_enabled);
    host->top_bar.dirty_flags = 1;
    host->dirty.render = 1;
}

void reach_host_set_battery_open(reach_host *host, int32_t open)
{
    if (host == nullptr ||
        reach_host_feature_capsule<reach_battery>(host, REACH_SURFACE_ID_BATTERY) == nullptr)
    {
        return;
    }

    int32_t next_open = open ? 1 : 0;
    if (next_open == reach_battery_is_open(
                         reach_host_feature_capsule<reach_battery>(host, REACH_SURFACE_ID_BATTERY)))
    {
        return;
    }

    if (next_open)
    {
        reach_host_surface_opening(host, REACH_SURFACE_ID_BATTERY, REACH_SURFACE_ID_TOP_BAR);
        reach_host_refresh_battery_power(host);

        reach_battery_open_context ctx = reach_host_battery_open_context(host);
        reach_battery_open(
            reach_host_feature_capsule<reach_battery>(host, REACH_SURFACE_ID_BATTERY), &ctx);
    }
    else
    {
        reach_battery_force_close(
            reach_host_feature_capsule<reach_battery>(host, REACH_SURFACE_ID_BATTERY));
        reach_host_request_bar_visibility_update(host);
    }

    reach_host_surface_transition_set(host, &host->battery_transition, next_open);
    reach_host_sync_pointer_move_subscriptions(host);
    reach_host_sync_popup_mouse_hook(host);

    host->battery.dirty_flags = 1;
    host->dirty.render = 1;
}

void reach_host_toggle_battery(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }
    reach_host_set_battery_open(
        host, reach_battery_is_open(
                  reach_host_feature_capsule<reach_battery>(host, REACH_SURFACE_ID_BATTERY))
                  ? 0
                  : 1);
}
