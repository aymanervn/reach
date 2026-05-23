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

static float reach_quick_settings_volume_from_slider_x(
    reach_rect_f32 slider_track,
    float x
)
{
    if (slider_track.width <= 0.0f) {
        return 0.0f;
    }

    return reach_quick_settings_clamp01(
        (x - slider_track.x) / slider_track.width);
}

reach_quick_settings_hit_result reach_quick_settings_hit_test(
    const reach_quick_settings_layout *layout,
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
        result.volume_level = reach_quick_settings_volume_from_slider_x(
            layout->main_slider_track,
            x);
        return result;
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

    if (hit.type == REACH_QUICK_SETTINGS_HIT_EXPAND_BUTTON) {
        action.type = REACH_QUICK_SETTINGS_ACTION_EXPAND;
        return action;
    }

    return action;
}
