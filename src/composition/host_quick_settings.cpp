#include "host_internal.h"

#include <math.h>

static float reach_host_quick_settings_clamp01(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    if (value > 1.0f)
    {
        return 1.0f;
    }
    return value;
}

/*
 * The quick-settings layout + open/close height animation live in the capsule
 * (reach_quick_settings_refresh_layout / _relayout / _update_open_animation);
 * composition only computes the borrowed anchor inputs from its dock layout and
 * routes the redraw result onto its surface flags.
 */
static reach_quick_settings_layout_context
reach_host_quick_settings_layout_context(reach_host *host)
{
    reach_quick_settings_layout_context ctx = {};
    ctx.theme = host->theme;
    ctx.dpi_scale = reach_host_layout_dpi_scale(host);
    reach_rect_f32 button = reach_dock_rect_to_screen(
        &host->layout.dock, host->layout.dock.quick_settings_button);
    ctx.anchor_x = button.x + button.width * 0.5f;
    ctx.dock_top = host->layout.dock.bounds.y;
    return ctx;
}

void reach_host_relayout_quick_settings(reach_host *host, int32_t animate_height)
{
    if (host == nullptr || !host->has_layout)
    {
        return;
    }

    reach_quick_settings_layout_context ctx = reach_host_quick_settings_layout_context(host);
    reach_quick_settings_relayout(host->quick_settings_capsule, &ctx, animate_height);
}

void reach_host_refresh_quick_settings_layout(reach_host *host)
{
    if (host == nullptr || !host->has_layout)
    {
        return;
    }

    reach_quick_settings_layout_context ctx = reach_host_quick_settings_layout_context(host);
    reach_quick_settings_refresh_layout(host->quick_settings_capsule, &ctx);
}

void reach_host_update_quick_settings_animation(reach_host *host)
{
    if (host == nullptr || !host->has_layout)
    {
        return;
    }

    reach_quick_settings_layout_context ctx = reach_host_quick_settings_layout_context(host);
    if (reach_quick_settings_update_open_animation(host->quick_settings_capsule, &ctx))
    {
        host->quick_settings.dirty_flags = 1;
        host->dirty.render = 1;
    }
}

void reach_host_set_quick_settings_open(reach_host *host, int32_t open)
{
    if (host == nullptr)
    {
        return;
    }

    int32_t next_open = open ? 1 : 0;
    if (next_open == reach_quick_settings_is_open(host->quick_settings_capsule))
    {
        return;
    }

    if (next_open)
    {
        reach_host_close_other_popups(host, REACH_SURFACE_ID_QUICK_SETTINGS);
    }

    (void)reach_quick_settings_set_open(host->quick_settings_capsule, next_open);
    reach_host_surface_transition_set(host, &host->quick_settings_transition, next_open);
    reach_host_sync_pointer_move_subscriptions(host);

    reach_quick_settings_set_bluetooth_pending(host->quick_settings_capsule, 0, 0);
    if (next_open)
    {
        reach_quick_settings_refresh_system(host->quick_settings_capsule, 0);
        reach_quick_settings_refresh_audio(host->quick_settings_capsule);
        reach_quick_settings_reset_height_animation(host->quick_settings_capsule);
        reach_host_relayout_quick_settings(host, 0);
    }
    else
    {
        reach_host_request_dock_visibility_update(host);
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
        host, reach_quick_settings_is_open(host->quick_settings_capsule) ? 0 : 1);
}

reach_result reach_host_apply_quick_settings_pointer_action(
    reach_host *host, const reach_ui_event *event, const reach_capsule_pointer_result *result)
{
    (void)event;
    if (host == nullptr || result == nullptr)
    {
        return REACH_OK;
    }
    reach_quick_settings_pointer_action_kind action =
        (reach_quick_settings_pointer_action_kind)result->action.kind;

    if (action == REACH_QUICK_SETTINGS_POINTER_ACTION_SET_MAIN_VOLUME)
    {
        float level = reach_host_quick_settings_clamp01(result->action.value);

        reach_quick_settings_apply_main_volume(
            host->quick_settings_capsule, level,
            reach_quick_settings_state_ptr(host->quick_settings_capsule)->model.main_muted);

        if (host->audio_volume.set_level != nullptr)
        {
            (void)host->audio_volume.set_level(host->audio_volume.userdata, level);
        }

        host->quick_settings.dirty_flags = 1;
        host->dirty.render = 1;
        return REACH_OK;
    }

    if (action == REACH_QUICK_SETTINGS_POINTER_ACTION_SET_SESSION_VOLUME)
    {
        float level = reach_host_quick_settings_clamp01(result->action.value);

        const uint16_t *session_instance_id = reach_quick_settings_set_session_level(
            host->quick_settings_capsule, result->action.index, level);
        if (session_instance_id != nullptr && host->audio_volume.set_session_level != nullptr)
        {
            (void)host->audio_volume.set_session_level(host->audio_volume.userdata,
                                                       session_instance_id, level);
        }

        host->quick_settings.dirty_flags = 1;
        host->dirty.render = 1;
        return REACH_OK;
    }

    if (action == REACH_QUICK_SETTINGS_POINTER_ACTION_SET_BRIGHTNESS)
    {
        if (host->system_controls.set_brightness_level != nullptr)
        {
            (void)host->system_controls.set_brightness_level(
                host->system_controls.userdata,
                reach_host_quick_settings_clamp01(result->action.value));
        }
        reach_quick_settings_refresh_system(host->quick_settings_capsule, 0);
        host->quick_settings.dirty_flags = 1;
        host->dirty.render = 1;
        return REACH_OK;
    }

    if (action == REACH_QUICK_SETTINGS_POINTER_ACTION_NETWORK_TILE)
    {
        reach_host_set_quick_settings_open(host, 0);
        if (host->system_controls.open_system_quick_settings != nullptr)
        {
            (void)host->system_controls.open_system_quick_settings(
                host->system_controls.userdata);
        }
        return REACH_OK;
    }

    if (action == REACH_QUICK_SETTINGS_POINTER_ACTION_TOGGLE_BLUETOOTH)
    {
        if (reach_quick_settings_bluetooth_pending(host->quick_settings_capsule))
        {
            return REACH_OK;
        }

        if (!reach_quick_settings_bluetooth_available(host->quick_settings_capsule))
        {
            reach_quick_settings_refresh_system(host->quick_settings_capsule, 0);
            host->quick_settings.dirty_flags = 1;
            host->dirty.render = 1;
            return REACH_OK;
        }

        int32_t target_enabled =
            reach_quick_settings_bluetooth_enabled(host->quick_settings_capsule) ? 0 : 1;
        if (host->system_controls.request_bluetooth_enabled != nullptr)
        {
            reach_quick_settings_set_bluetooth_pending(host->quick_settings_capsule, 1,
                                                       target_enabled);
            host->quick_settings.dirty_flags = 1;
            host->dirty.render = 1;
            if (host->system_controls.request_bluetooth_enabled(host->system_controls.userdata,
                                                                 target_enabled) != REACH_OK)
            {
                reach_quick_settings_set_bluetooth_pending(host->quick_settings_capsule, 0, 0);
                reach_quick_settings_refresh_system(host->quick_settings_capsule, 0);
            }
            host->quick_settings.dirty_flags = 1;
            host->dirty.render = 1;
            return REACH_OK;
        }

        if (host->system_controls.set_bluetooth_enabled != nullptr)
        {
            reach_quick_settings_set_bluetooth_pending(host->quick_settings_capsule, 1,
                                                       target_enabled);
            host->quick_settings.dirty_flags = 1;
            host->dirty.render = 1;
            (void)host->system_controls.set_bluetooth_enabled(host->system_controls.userdata,
                                                               target_enabled);
        }
        reach_quick_settings_refresh_system(host->quick_settings_capsule, 0);
        reach_quick_settings_set_bluetooth_pending(host->quick_settings_capsule, 0, 0);
        host->quick_settings.dirty_flags = 1;
        host->dirty.render = 1;
        return REACH_OK;
    }

    if (action == REACH_QUICK_SETTINGS_POINTER_ACTION_TOGGLE_BATTERY_SAVER)
    {
        reach_quick_settings_refresh_system(host->quick_settings_capsule, 0);
        host->quick_settings.dirty_flags = 1;
        host->dirty.render = 1;
        return REACH_OK;
    }

    if (action == REACH_QUICK_SETTINGS_POINTER_ACTION_OPEN_PROJECT)
    {
        reach_host_set_quick_settings_open(host, 0);
        if (host->system_controls.open_project_menu != nullptr)
        {
            (void)host->system_controls.open_project_menu(host->system_controls.userdata);
        }
        return REACH_OK;
    }

    if (action == REACH_QUICK_SETTINGS_POINTER_ACTION_TOGGLE_OUTPUT_DEVICES)
    {
        (void)reach_quick_settings_toggle_output_devices(host->quick_settings_capsule);

        reach_quick_settings_refresh_system(host->quick_settings_capsule, 0);
        reach_quick_settings_refresh_audio(host->quick_settings_capsule);
        reach_host_relayout_quick_settings(host, 1);

        host->quick_settings.dirty_flags = 1;
        host->dirty.render = 1;
        return REACH_OK;
    }

    if (action == REACH_QUICK_SETTINGS_POINTER_ACTION_SET_OUTPUT_DEVICE)
    {
        int32_t changed = 0;
        const uint16_t *output_device_id = reach_quick_settings_output_device_id(
            host->quick_settings_capsule, result->action.index);
        if (output_device_id != nullptr &&
            host->audio_volume.set_default_output_device != nullptr)
        {
            changed = host->audio_volume.set_default_output_device(
                          host->audio_volume.userdata, output_device_id) == REACH_OK;
        }

        if (changed)
        {
            reach_quick_settings_collapse_output_devices(host->quick_settings_capsule);
        }
        reach_quick_settings_refresh_system(host->quick_settings_capsule, 0);
        reach_quick_settings_refresh_audio(host->quick_settings_capsule);
        if (changed)
        {
            reach_host_relayout_quick_settings(host, 1);
        }

        host->quick_settings.dirty_flags = 1;
        host->dirty.render = 1;
        return REACH_OK;
    }

    if (action == REACH_QUICK_SETTINGS_POINTER_ACTION_EXPAND)
    {
        (void)reach_quick_settings_toggle_expanded(host->quick_settings_capsule);

        reach_quick_settings_refresh_system(host->quick_settings_capsule, 0);
        reach_quick_settings_refresh_audio(host->quick_settings_capsule);
        reach_host_relayout_quick_settings(host, 1);

        host->quick_settings.dirty_flags = 1;
        host->dirty.render = 1;
        return REACH_OK;
    }
    return REACH_OK;
}

/* System-controls watcher callback: fires on a PORT thread. Capsule state is
   never written off-thread, so this only accumulates into the atomic; the
   update pass drains it into the capsule's service pump below. */
void reach_host_on_system_controls_changed(void *user, uint32_t change_flags)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr || change_flags == 0)
    {
        return;
    }

    host->quick_settings_system_change_flags.fetch_or(change_flags);
}

void reach_host_process_quick_settings_changes(reach_host *host, double delta_seconds)
{
    if (host == nullptr)
    {
        return;
    }

    uint32_t change_flags = host->quick_settings_system_change_flags.exchange(0);
    reach_feature_tick_result changes = {};
    reach_quick_settings_process_changes(host->quick_settings_capsule, change_flags,
                                         delta_seconds, &changes);

    /* GPU lifetime stays composition-owned: release the render icons an audio
       apply retired inside the capsule. */
    uint64_t retired[REACH_AUDIO_VOLUME_MAX_SESSIONS + REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES];
    size_t retired_count = reach_quick_settings_take_retired_render_icons(
        host->quick_settings_capsule, retired,
        REACH_AUDIO_VOLUME_MAX_SESSIONS + REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES);
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
