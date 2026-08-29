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
        host->surfaces[REACH_SURFACE_ID_DOCK].dirty_flags = 1;
        reach_feature_notification notification = {};
        notification.kind = REACH_FEATURE_NOTIFICATION_MEDIA_ACTION;
        notification.media_action = action;
        notification.present = host->top_bar_hidden;
        reach_host_notify_registered_features(host, &notification);
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

    reach_feature_notification notification = {};
    notification.kind = REACH_FEATURE_NOTIFICATION_MAIN_VOLUME;
    notification.volume = state;
    notification.volume.level = level;
    notification.present = host->top_bar_hidden;
    reach_host_notify_registered_features(host, &notification);

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

    reach_feature_notification notification = {};
    notification.kind = REACH_FEATURE_NOTIFICATION_MAIN_VOLUME;
    notification.volume = state;
    notification.volume.level = reach_host_clamp01(state.level);
    notification.volume.muted = muted;
    notification.present = host->top_bar_hidden;
    reach_host_notify_registered_features(host, &notification);

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
    if (host == nullptr || host->system_status == nullptr)
    {
        return REACH_OK;
    }

    reach_brightness_state state = {};
    reach_result result = reach_system_status_step_brightness(host->system_status, delta, &state);
    if (result == REACH_NOT_IMPLEMENTED)
    {
        return REACH_OK;
    }
    if (result != REACH_OK)
    {
        return result;
    }
    if (!state.available)
    {
        return REACH_OK;
    }

    reach_feature_notification notification = {};
    notification.kind = REACH_FEATURE_NOTIFICATION_BRIGHTNESS;
    notification.brightness = state;
    notification.present = host->top_bar_hidden;
    reach_host_notify_registered_features(host, &notification);

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

reach_result reach_host_execute_power_command(reach_host *host, uint32_t command)
{
    if (host == nullptr)
    {
        return REACH_OK;
    }

    switch (command)
    {
    case REACH_CONTEXT_MENU_COMMAND_POWER_LOCK:
        return host->power_session.ops.lock != nullptr
                   ? host->power_session.ops.lock(host->power_session.session)
                   : REACH_ERROR;
    case REACH_CONTEXT_MENU_COMMAND_POWER_SLEEP:
        return host->power_session.ops.sleep != nullptr
                   ? host->power_session.ops.sleep(host->power_session.session)
                   : REACH_ERROR;
    case REACH_CONTEXT_MENU_COMMAND_POWER_RESTART:
        return host->power_session.ops.restart != nullptr
                   ? host->power_session.ops.restart(host->power_session.session)
                   : REACH_ERROR;
    case REACH_CONTEXT_MENU_COMMAND_POWER_SHUTDOWN:
        return host->power_session.ops.shutdown != nullptr
                   ? host->power_session.ops.shutdown(host->power_session.session)
                   : REACH_ERROR;
    case REACH_CONTEXT_MENU_COMMAND_POWER_SIGN_OUT:
        return host->power_session.ops.sign_out != nullptr
                   ? host->power_session.ops.sign_out(host->power_session.session)
                   : REACH_ERROR;
    default:
        return REACH_OK;
    }
}
