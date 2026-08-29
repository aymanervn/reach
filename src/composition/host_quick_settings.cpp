#include "host_internal.h"

static reach_quick_settings_layout_context
reach_host_quick_settings_layout_context(reach_host *host)
{
    reach_quick_settings_layout_context ctx = {};
    ctx.theme = host->theme;
    ctx.dpi_scale = reach_host_layout_dpi_scale(host);
    const reach_top_bar_layout *top_bar_layout =
        &reach_top_bar_state_ptr(
             reach_host_feature_capsule<reach_top_bar>(host, REACH_SURFACE_ID_TOP_BAR))
             ->layout;
    ctx.anchor_button =
        reach_top_bar_rect_to_screen(top_bar_layout, top_bar_layout->quick_settings_button);
    reach_rect_f32 monitor = {};
    if (reach_host_primary_monitor_bounds(host, &monitor))
    {
        ctx.monitor = monitor;
    }
    ctx.bar_edge_y = top_bar_layout->bounds.y + top_bar_layout->bounds.height;
    ctx.drop_direction = REACH_POPUP_DROP_DOWN;
    return ctx;
}

void reach_host_relayout_quick_settings(reach_host *host, int32_t animate_height)
{
    if (host == nullptr || !host->has_layout)
    {
        return;
    }

    reach_quick_settings_layout_context ctx = reach_host_quick_settings_layout_context(host);
    reach_quick_settings_relayout(
        reach_host_feature_capsule<reach_quick_settings>(host, REACH_SURFACE_ID_QUICK_SETTINGS),
        &ctx, animate_height);
}

void reach_host_set_quick_settings_open(reach_host *host, int32_t open)
{
    if (host == nullptr)
    {
        return;
    }

    int32_t next_open = open ? 1 : 0;
    if (next_open == reach_quick_settings_is_open(reach_host_feature_capsule<reach_quick_settings>(
                         host, REACH_SURFACE_ID_QUICK_SETTINGS)))
    {
        return;
    }

    if (next_open)
    {
        reach_host_surface_opening(host, REACH_SURFACE_ID_QUICK_SETTINGS, REACH_SURFACE_ID_TOP_BAR);
    }

    (void)reach_quick_settings_set_open(
        reach_host_feature_capsule<reach_quick_settings>(host, REACH_SURFACE_ID_QUICK_SETTINGS),
        next_open);
    reach_host_surface_transition_set(host, &host->quick_settings_transition, next_open);
    reach_host_sync_pointer_move_subscriptions(host);

    reach_quick_settings_set_bluetooth_pending(
        reach_host_feature_capsule<reach_quick_settings>(host, REACH_SURFACE_ID_QUICK_SETTINGS), 0,
        0);
    if (next_open)
    {
        reach_quick_settings_refresh_system(
            reach_host_feature_capsule<reach_quick_settings>(host, REACH_SURFACE_ID_QUICK_SETTINGS),
            0);
        reach_quick_settings_refresh_audio(reach_host_feature_capsule<reach_quick_settings>(
            host, REACH_SURFACE_ID_QUICK_SETTINGS));
        reach_quick_settings_reset_height_animation(
            reach_host_feature_capsule<reach_quick_settings>(host,
                                                             REACH_SURFACE_ID_QUICK_SETTINGS));
        reach_host_relayout_quick_settings(host, 0);
    }
    else
    {
        reach_host_request_bar_visibility_update(host);
    }

    reach_host_sync_popup_mouse_hook(host);

    host->quick_settings.dirty_flags = 1;
    host->dirty.render = 1;
}

void reach_host_toggle_quick_settings(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_host_set_quick_settings_open(
        host, reach_quick_settings_is_open(reach_host_feature_capsule<reach_quick_settings>(
                  host, REACH_SURFACE_ID_QUICK_SETTINGS))
                  ? 0
                  : 1);
}

void reach_host_on_system_controls_changed(void *user, uint32_t change_flags)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr || change_flags == 0)
    {
        return;
    }

    host->quick_settings_system_change_flags.fetch_or(change_flags);
}

void reach_host_on_audio_volume_changed(void *user)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host != nullptr)
    {
        host->audio_volume_changed.store(1);
    }
}

void reach_host_process_quick_settings_changes(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    uint32_t change_flags = host->quick_settings_system_change_flags.exchange(0);
    if (change_flags != 0)
    {
        reach_system_status_refresh_system(host->system_status, change_flags);
    }
    if (host->audio_volume_changed.exchange(0) != 0)
    {
        reach_system_status_refresh_audio(host->system_status);
    }
    reach_feature_tick_result changes = {};
    reach_quick_settings_process_changes(
        reach_host_feature_capsule<reach_quick_settings>(host, REACH_SURFACE_ID_QUICK_SETTINGS),
        &changes);

    uint64_t retired[REACH_AUDIO_VOLUME_MAX_SESSIONS + REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES];
    size_t retired_count = reach_quick_settings_take_retired_render_icons(
        reach_host_feature_capsule<reach_quick_settings>(host, REACH_SURFACE_ID_QUICK_SETTINGS),
        retired, REACH_AUDIO_VOLUME_MAX_SESSIONS + REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES);
    for (size_t index = 0; index < retired_count; ++index)
    {
        reach_host_release_render_icon(host, retired[index]);
    }

    if (changes.relayout)
    {
        reach_host_relayout_quick_settings(host, 1);
    }
    if (changes.redraw)
    {
        host->quick_settings.dirty_flags = 1;
        host->dirty.render = 1;
    }
    if (changes.request_update)
    {
        reach_host_request_update(host);
    }
}
