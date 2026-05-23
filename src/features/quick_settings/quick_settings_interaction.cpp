#include "reach/features/quick_settings.h"

static int reach_quick_settings_point_in_rect(
    reach_rect_f32 rect,
    float x,
    float y
)
{
    return x >= rect.x &&
        x <= rect.x + rect.width &&
        y >= rect.y &&
        y <= rect.y + rect.height;
}

static float reach_quick_settings_clamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static void reach_quick_settings_copy_utf16(
    uint16_t *dst,
    size_t dst_count,
    const uint16_t *src
)
{
    if (dst == nullptr || dst_count == 0) {
        return;
    }

    size_t index = 0;
    if (src != nullptr) {
        while (index + 1 < dst_count && src[index] != 0) {
            dst[index] = src[index];
            ++index;
        }
    }
    dst[index] = 0;
}

reach_quick_settings_hit_result reach_quick_settings_hit_test(
    const reach_quick_settings_layout *layout,
    const reach_quick_settings_model *model,
    float x,
    float y
)
{
    reach_quick_settings_hit_result result = {};
    result.type = REACH_QUICK_SETTINGS_HIT_NONE;
    result.volume_level = 0.0f;

    if (layout == nullptr) {
        return result;
    }

    if (reach_quick_settings_point_in_rect(layout->main_slider_track, x, y)) {
        result.type = REACH_QUICK_SETTINGS_HIT_MAIN_SLIDER;
        result.volume_level = reach_quick_settings_volume_pill_level_for_x(
            &layout->main_volume_pill,
            x);
        return result;
    }

    for (size_t index = 0; index < layout->session_pill_count; ++index) {
        if (reach_quick_settings_point_in_rect(
            layout->session_volume_pills[index].slider_track,
            x,
            y)) {
            result.type = REACH_QUICK_SETTINGS_HIT_SESSION_SLIDER;
            result.session_index = index;
            result.volume_level = reach_quick_settings_volume_pill_level_for_x(
                &layout->session_volume_pills[index],
                x);
            if (model != nullptr && index < model->sessions.count) {
                reach_quick_settings_copy_utf16(
                    result.session_instance_id,
                    REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY,
                    model->sessions.sessions[index].session_instance_id);
            }
            return result;
        }
    }

    if (reach_quick_settings_point_in_rect(layout->expand_button, x, y)) {
        result.type = REACH_QUICK_SETTINGS_HIT_EXPAND_BUTTON;
        return result;
    }

    return result;
}

reach_quick_settings_action reach_quick_settings_action_for_hit(
    reach_quick_settings_hit_result hit
)
{
    reach_quick_settings_action action = {};
    action.type = REACH_QUICK_SETTINGS_ACTION_NONE;
    action.volume_level = 0.0f;

    if (hit.type == REACH_QUICK_SETTINGS_HIT_MAIN_SLIDER) {
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME;
        action.volume_level = reach_quick_settings_clamp01(hit.volume_level);
        return action;
    }

    if (hit.type == REACH_QUICK_SETTINGS_HIT_SESSION_SLIDER) {
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_SESSION_VOLUME;
        action.volume_level = reach_quick_settings_clamp01(hit.volume_level);
        action.session_index = hit.session_index;
        reach_quick_settings_copy_utf16(
            action.session_instance_id,
            REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY,
            hit.session_instance_id);
        return action;
    }

    if (hit.type == REACH_QUICK_SETTINGS_HIT_EXPAND_BUTTON) {
        action.type = REACH_QUICK_SETTINGS_ACTION_EXPAND;
        return action;
    }

    return action;
}
