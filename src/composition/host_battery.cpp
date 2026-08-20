#include "host_internal.h"

static reach_battery_open_context reach_host_battery_open_context(reach_host *host)
{
    reach_battery_open_context ctx = {};
    ctx.dpi_scale = reach_host_layout_dpi_scale(host);
    const reach_top_bar_layout *top_bar_layout =
        &reach_top_bar_state_ptr(host->top_bar_capsule)->layout;
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
    if (host == nullptr || host->battery_capsule == nullptr)
    {
        return;
    }

    reach_system_stats_snapshot snapshot = {};
    reach_system_stats_snapshot_take(host->system_stats, &snapshot);

    int32_t valid = snapshot.power_valid && snapshot.power.has_battery &&
                    snapshot.power.battery_percent >= 0;
    int32_t percent = valid ? snapshot.power.battery_percent : 0;
    int32_t saver_on = snapshot.power_valid && snapshot.power.battery_saver_on ? 1 : 0;

    if (reach_battery_set_power(host->battery_capsule, valid, percent, saver_on))
    {
        reach_top_bar_set_battery_saver_pending(
            host->top_bar_capsule, reach_battery_saver_pending(host->battery_capsule),
            reach_battery_state_ptr(host->battery_capsule)->model.saver_pending_enabled);
        host->battery.dirty_flags = 1;
        host->dirty.render = 1;
    }
}

void reach_host_sync_battery_saver_pending(reach_host *host)
{
    if (host == nullptr || host->battery_capsule == nullptr || host->top_bar_capsule == nullptr)
    {
        return;
    }

    const reach_battery_model *model = &reach_battery_state_ptr(host->battery_capsule)->model;
    const reach_top_bar_state *top_bar = reach_top_bar_state_ptr(host->top_bar_capsule);
    if (top_bar->battery_saver_pending == model->saver_pending &&
        top_bar->battery_saver_pending_enabled == model->saver_pending_enabled)
    {
        return;
    }

    reach_top_bar_set_battery_saver_pending(host->top_bar_capsule, model->saver_pending,
                                            model->saver_pending_enabled);
    host->top_bar.dirty_flags = 1;
    host->dirty.render = 1;
}

void reach_host_relayout_battery(reach_host *host)
{
    if (host == nullptr || !host->has_layout || host->battery_capsule == nullptr)
    {
        return;
    }

    reach_battery_open_context ctx = reach_host_battery_open_context(host);
    reach_battery_relayout(host->battery_capsule, &ctx);
}

void reach_host_set_battery_open(reach_host *host, int32_t open)
{
    if (host == nullptr || host->battery_capsule == nullptr)
    {
        return;
    }

    int32_t next_open = open ? 1 : 0;
    if (next_open == reach_battery_is_open(host->battery_capsule))
    {
        return;
    }

    if (next_open)
    {
        reach_host_surface_opening(host, REACH_SURFACE_ID_BATTERY, REACH_SURFACE_ID_TOP_BAR);
        reach_host_refresh_battery_power(host);

        reach_battery_open_context ctx = reach_host_battery_open_context(host);
        reach_battery_open(host->battery_capsule, &ctx);
    }
    else
    {
        reach_battery_force_close(host->battery_capsule);
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
    reach_host_set_battery_open(host, reach_battery_is_open(host->battery_capsule) ? 0 : 1);
}

reach_result reach_host_apply_battery_pointer_action(reach_host *host,
                                                     const reach_ui_event *event,
                                                     const reach_capsule_pointer_result *result)
{
    (void)event;
    if (host == nullptr || result == nullptr)
    {
        return REACH_OK;
    }

    if (result->action.kind == REACH_BATTERY_POINTER_ACTION_DISMISS)
    {
        reach_host_set_battery_open(host, 0);
        return REACH_OK;
    }

    if (result->action.kind != REACH_BATTERY_POINTER_ACTION_TOGGLE_SAVER)
    {
        return REACH_OK;
    }

    const reach_battery_state *state = reach_battery_state_ptr(host->battery_capsule);
    int32_t target_enabled = reach_battery_model_saver_effective(&state->model) ? 0 : 1;

    reach_battery_set_saver_pending(host->battery_capsule, 1, target_enabled);
    reach_top_bar_set_battery_saver_pending(host->top_bar_capsule, 1, target_enabled);

    if (host->system_controls.set_battery_saver_enabled != nullptr)
    {
        (void)host->system_controls.set_battery_saver_enabled(host->system_controls.userdata,
                                                              target_enabled);
    }

    reach_host_refresh_battery_power(host);

    host->battery.dirty_flags = 1;
    host->top_bar.dirty_flags = 1;
    host->dirty.render = 1;
    return REACH_OK;
}
