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

reach_result reach_host_execute_media_action(reach_host *host, reach_now_playing_action action)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (reach_now_playing_service_try_action(host->now_playing_service, action))
    {
        host->dock.dirty_flags = 1;
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
    return host->audio_volume.set_level(host->audio_volume.userdata, level);
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

    return host->audio_volume.set_muted(host->audio_volume.userdata, state.muted ? 0 : 1);
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

    if (reach_quick_settings_state_ptr(host->quick_settings_capsule)->open)
    {
        reach_quick_settings_refresh_system(host->quick_settings_capsule,
                                            REACH_SYSTEM_CONTROLS_CHANGE_BRIGHTNESS);
        host->quick_settings.dirty_flags = 1;
        host->dirty.render = 1;
        reach_host_request_update(host);
    }

    return REACH_OK;
}
