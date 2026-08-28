#include "host_internal.h"

static float reach_host_clamp01(float value)
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

static void reach_host_mark_quick_settings_changed(reach_host *host)
{
    host->quick_settings.dirty_flags = 1;
    host->dirty.render = 1;
    reach_host_request_update(host);
}

static void reach_host_mark_system_hud_changed(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }
    host->system_hud.dirty_flags = 1;
    host->dirty.render = 1;
    reach_host_request_update(host);
}

reach_result reach_host_launch_settings_app(reach_host *host)
{
    if (host == nullptr || host->settings_launcher.ops.resolve == nullptr)
    {
        return REACH_ERROR;
    }

    reach_app_launch_request request = {};
    reach_result resolved = host->settings_launcher.ops.resolve(
        host->settings_launcher.launcher, request.path, 260, request.arguments, 260);
    if (resolved != REACH_OK)
    {
        return resolved;
    }
    return reach_host_schedule_app_launch(host, &request);
}

reach_result reach_host_execute_media_action(reach_host *host, reach_now_playing_action action)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (reach_now_playing_service_try_action(host->now_playing_service, action))
    {
        host->dock.dirty_flags = 1;
        if (host->top_bar_hidden)
        {
            reach_system_hud_show_media(
                reach_host_feature_capsule<reach_system_hud>(host, REACH_SURFACE_ID_SYSTEM_HUD),
                action);
            reach_host_mark_system_hud_changed(host);
        }
        reach_host_request_update(host);
    }
    return REACH_OK;
}

reach_result reach_host_step_main_volume(reach_host *host, float delta)
{
    if (host == nullptr || host->audio_volume.get_state == nullptr ||
        host->audio_volume.set_level == nullptr)
    {
        return REACH_OK;
    }

    reach_audio_volume_state state = {};
    if (host->audio_volume.get_state(host->audio_volume.userdata, &state) != REACH_OK)
    {
        return REACH_ERROR;
    }

    float level = reach_host_clamp01(state.level + delta);
    reach_result result = host->audio_volume.set_level(host->audio_volume.userdata, level);
    if (result != REACH_OK)
    {
        return result;
    }

    if (reach_quick_settings_state_ptr(
            reach_host_feature_capsule<reach_quick_settings>(host, REACH_SURFACE_ID_QUICK_SETTINGS))
            ->open)
    {
        reach_quick_settings_apply_main_volume(
            reach_host_feature_capsule<reach_quick_settings>(host, REACH_SURFACE_ID_QUICK_SETTINGS),
            level, state.muted ? 1 : 0);
        reach_host_mark_quick_settings_changed(host);
    }
    if (host->top_bar_hidden)
    {
        reach_audio_volume_state next = state;
        next.level = level;
        reach_system_hud_show_volume(
            reach_host_feature_capsule<reach_system_hud>(host, REACH_SURFACE_ID_SYSTEM_HUD), &next);
        reach_host_mark_system_hud_changed(host);
    }

    return REACH_OK;
}

reach_result reach_host_toggle_main_volume_mute(reach_host *host)
{
    if (host == nullptr || host->audio_volume.get_state == nullptr ||
        host->audio_volume.set_muted == nullptr)
    {
        return REACH_OK;
    }

    reach_audio_volume_state state = {};
    if (host->audio_volume.get_state(host->audio_volume.userdata, &state) != REACH_OK)
    {
        return REACH_ERROR;
    }

    int32_t muted = state.muted ? 0 : 1;
    reach_result result = host->audio_volume.set_muted(host->audio_volume.userdata, muted);
    if (result != REACH_OK)
    {
        return result;
    }

    if (reach_quick_settings_state_ptr(
            reach_host_feature_capsule<reach_quick_settings>(host, REACH_SURFACE_ID_QUICK_SETTINGS))
            ->open)
    {
        reach_quick_settings_apply_main_volume(
            reach_host_feature_capsule<reach_quick_settings>(host, REACH_SURFACE_ID_QUICK_SETTINGS),
            reach_host_clamp01(state.level), muted);
        reach_host_mark_quick_settings_changed(host);
    }
    if (host->top_bar_hidden)
    {
        reach_audio_volume_state next = state;
        next.muted = muted;
        reach_system_hud_show_volume(
            reach_host_feature_capsule<reach_system_hud>(host, REACH_SURFACE_ID_SYSTEM_HUD), &next);
        reach_host_mark_system_hud_changed(host);
    }

    return REACH_OK;
}

reach_result reach_host_snap_foreground_window(reach_host *host, reach_split_mode mode)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    uintptr_t foreground = reach_host_foreground_window(host);
    if (foreground == 0)
    {
        return REACH_OK;
    }

    reach_result result = reach_app_control_schedule_snap(host->app_control, foreground, mode);
    if (result == REACH_OK)
    {
        reach_host_begin_programmatic_window_manipulation(host, foreground);
        reach_host_request_update(host);
    }
    return result;
}

reach_result reach_host_step_brightness(reach_host *host, float delta)
{
    if (host == nullptr || host->system_controls.get_brightness_state == nullptr ||
        host->system_controls.set_brightness_level == nullptr)
    {
        return REACH_OK;
    }

    reach_brightness_state state = {};
    if (host->system_controls.get_brightness_state(host->system_controls.userdata, &state) !=
        REACH_OK)
    {
        return REACH_ERROR;
    }
    if (!state.available)
    {
        return REACH_OK;
    }

    float level = reach_host_clamp01(state.level + delta);
    reach_result result =
        host->system_controls.set_brightness_level(host->system_controls.userdata, level);
    if (result != REACH_OK)
    {
        return result;
    }

    if (reach_quick_settings_state_ptr(
            reach_host_feature_capsule<reach_quick_settings>(host, REACH_SURFACE_ID_QUICK_SETTINGS))
            ->open)
    {
        reach_brightness_state brightness = state;
        brightness.level = level;
        const reach_quick_settings_model *model =
            &reach_quick_settings_state_ptr(reach_host_feature_capsule<reach_quick_settings>(
                                                host, REACH_SURFACE_ID_QUICK_SETTINGS))
                 ->model;
        reach_quick_settings_system_apply_result apply_result = {};
        reach_quick_settings_apply_system_states(
            reach_host_feature_capsule<reach_quick_settings>(host, REACH_SURFACE_ID_QUICK_SETTINGS),
            &model->network, &model->bluetooth, &brightness, 0, &apply_result);
        if (apply_result.relayout)
        {
            reach_host_relayout_quick_settings(host, 1);
        }
        reach_quick_settings_refresh_system(
            reach_host_feature_capsule<reach_quick_settings>(host, REACH_SURFACE_ID_QUICK_SETTINGS),
            REACH_SYSTEM_CONTROLS_CHANGE_BRIGHTNESS);
        reach_host_mark_quick_settings_changed(host);
    }
    if (host->top_bar_hidden)
    {
        reach_brightness_state brightness = state;
        brightness.level = level;
        reach_system_hud_show_brightness(
            reach_host_feature_capsule<reach_system_hud>(host, REACH_SURFACE_ID_SYSTEM_HUD),
            &brightness);
        reach_host_mark_system_hud_changed(host);
    }

    return REACH_OK;
}

reach_result reach_host_cycle_input_language(reach_host *host)
{
    if (host == nullptr)
    {
        return REACH_OK;
    }
    return reach_input_language_service_cycle_next(host->input_language,
                                                   reach_host_foreground_window(host));
}
