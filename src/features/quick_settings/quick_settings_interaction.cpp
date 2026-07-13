#include "reach/features/quick_settings.h"

#include "quick_settings_common.h"

#include <math.h>

static int reach_quick_settings_point_in_rect(reach_rect_f32 rect, float x, float y)
{
    return x >= rect.x && x <= rect.x + rect.width && y >= rect.y && y <= rect.y + rect.height;
}

static float reach_quick_settings_level_for_slider_line(reach_rect_f32 slider_line, float x)
{
    if (slider_line.width <= 0.0f)
    {
        return 0.0f;
    }
    return reach_quick_settings_clamp01((x - slider_line.x) / slider_line.width);
}

static int
reach_quick_settings_point_in_app_slider(const reach_quick_settings_app_volume_row_layout *row,
                                         float x, float y)
{
    if (row == nullptr)
    {
        return 0;
    }

    reach_rect_f32 slider_hit = row->slider_full_range_line;
    slider_hit.y = row->bounds.y;
    slider_hit.height = row->bounds.height;
    if (reach_quick_settings_point_in_rect(slider_hit, x, y))
    {
        return 1;
    }

    return reach_quick_settings_point_in_rect(row->slider_thumb, x, y);
}

reach_quick_settings_hit_result
reach_quick_settings_hit_test(const reach_quick_settings_layout *layout,
                              const reach_quick_settings_model *model, float x, float y)
{
    reach_quick_settings_hit_result result = {};
    result.type = REACH_QUICK_SETTINGS_HIT_NONE;
    result.volume_level = 0.0f;

    if (layout == nullptr)
    {
        return result;
    }

    if (reach_quick_settings_point_in_rect(layout->network_tile.bounds, x, y))
    {
        result.type = REACH_QUICK_SETTINGS_HIT_NETWORK_TILE;
        return result;
    }

    if (reach_quick_settings_point_in_rect(layout->bluetooth_tile.bounds, x, y))
    {
        if (model != nullptr && model->bluetooth_pending)
        {
            result.type = REACH_QUICK_SETTINGS_HIT_NONE;
            return result;
        }
        result.type = REACH_QUICK_SETTINGS_HIT_BLUETOOTH_TILE;
        return result;
    }

    if (reach_quick_settings_point_in_rect(layout->battery_saver_tile.bounds, x, y))
    {
        result.type = REACH_QUICK_SETTINGS_HIT_BATTERY_SAVER_TILE;
        return result;
    }

    if (reach_quick_settings_point_in_rect(layout->project_tile.bounds, x, y))
    {
        result.type = REACH_QUICK_SETTINGS_HIT_PROJECT_TILE;
        return result;
    }

    if (model != nullptr && model->brightness.available &&
        reach_quick_settings_point_in_rect(layout->brightness_slider_track, x, y))
    {
        result.type = REACH_QUICK_SETTINGS_HIT_BRIGHTNESS_SLIDER;
        result.volume_level =
            reach_quick_settings_volume_pill_level_for_x(&layout->brightness_pill, x);
        return result;
    }

    if (reach_quick_settings_point_in_rect(layout->main_slider_track, x, y))
    {
        result.type = REACH_QUICK_SETTINGS_HIT_MAIN_SLIDER;
        result.volume_level =
            reach_quick_settings_volume_pill_level_for_x(&layout->main_volume_pill, x);
        return result;
    }

    if (reach_quick_settings_point_in_rect(layout->output_device_button, x, y) ||
        reach_quick_settings_point_in_rect(layout->output_devices_title_chevron, x, y))
    {
        result.type = REACH_QUICK_SETTINGS_HIT_OUTPUT_DEVICE_BUTTON;
        return result;
    }

    for (size_t index = 0; index < layout->output_device_row_count; ++index)
    {
        if (reach_quick_settings_point_in_rect(layout->output_device_rows[index].bounds, x, y))
        {
            result.type = REACH_QUICK_SETTINGS_HIT_OUTPUT_DEVICE_ROW;
            result.output_device_index = index;
            if (model != nullptr && index < model->output_devices.count)
            {
                reach_quick_settings_copy_utf16(result.output_device_id,
                                                REACH_AUDIO_VOLUME_DEVICE_ID_CAPACITY,
                                                model->output_devices.devices[index].device_id);
            }
            return result;
        }
    }

    for (size_t index = 0; index < layout->app_volume_row_count; ++index)
    {
        if (reach_quick_settings_point_in_app_slider(&layout->app_volume_rows[index], x, y))
        {
            result.type = REACH_QUICK_SETTINGS_HIT_SESSION_SLIDER;
            result.session_index = index;
            result.volume_level = reach_quick_settings_level_for_slider_line(
                layout->app_volume_rows[index].slider_full_range_line, x);
            if (model != nullptr && index < model->sessions.count)
            {
                reach_quick_settings_copy_utf16(
                    result.session_instance_id, REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY,
                    model->sessions.sessions[index].session_instance_id);
            }
            return result;
        }
    }

    if (reach_quick_settings_point_in_rect(layout->expand_button, x, y))
    {
        result.type = REACH_QUICK_SETTINGS_HIT_EXPAND_BUTTON;
        return result;
    }

    return result;
}

reach_quick_settings_action reach_quick_settings_action_for_hit(reach_quick_settings_hit_result hit)
{
    reach_quick_settings_action action = {};
    action.type = REACH_QUICK_SETTINGS_ACTION_NONE;
    action.volume_level = 0.0f;

    if (hit.type == REACH_QUICK_SETTINGS_HIT_MAIN_SLIDER)
    {
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME;
        action.volume_level = reach_quick_settings_clamp01(hit.volume_level);
        return action;
    }

    if (hit.type == REACH_QUICK_SETTINGS_HIT_SESSION_SLIDER)
    {
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_SESSION_VOLUME;
        action.volume_level = reach_quick_settings_clamp01(hit.volume_level);
        action.session_index = hit.session_index;
        reach_quick_settings_copy_utf16(action.session_instance_id,
                                        REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY,
                                        hit.session_instance_id);
        return action;
    }

    if (hit.type == REACH_QUICK_SETTINGS_HIT_BRIGHTNESS_SLIDER)
    {
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_BRIGHTNESS;
        action.volume_level = reach_quick_settings_clamp01(hit.volume_level);
        return action;
    }

    if (hit.type == REACH_QUICK_SETTINGS_HIT_NETWORK_TILE)
    {
        action.type = REACH_QUICK_SETTINGS_ACTION_NETWORK_TILE;
        return action;
    }

    if (hit.type == REACH_QUICK_SETTINGS_HIT_BLUETOOTH_TILE)
    {
        action.type = REACH_QUICK_SETTINGS_ACTION_TOGGLE_BLUETOOTH;
        return action;
    }

    if (hit.type == REACH_QUICK_SETTINGS_HIT_BATTERY_SAVER_TILE)
    {
        action.type = REACH_QUICK_SETTINGS_ACTION_TOGGLE_BATTERY_SAVER;
        return action;
    }

    if (hit.type == REACH_QUICK_SETTINGS_HIT_PROJECT_TILE)
    {
        action.type = REACH_QUICK_SETTINGS_ACTION_OPEN_PROJECT;
        return action;
    }

    if (hit.type == REACH_QUICK_SETTINGS_HIT_OUTPUT_DEVICE_BUTTON)
    {
        action.type = REACH_QUICK_SETTINGS_ACTION_TOGGLE_OUTPUT_DEVICES;
        return action;
    }

    if (hit.type == REACH_QUICK_SETTINGS_HIT_OUTPUT_DEVICE_ROW)
    {
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_OUTPUT_DEVICE;
        action.output_device_index = hit.output_device_index;
        reach_quick_settings_copy_utf16(
            action.output_device_id, REACH_AUDIO_VOLUME_DEVICE_ID_CAPACITY, hit.output_device_id);
        return action;
    }

    if (hit.type == REACH_QUICK_SETTINGS_HIT_EXPAND_BUTTON)
    {
        action.type = REACH_QUICK_SETTINGS_ACTION_EXPAND;
        return action;
    }

    return action;
}

reach_quick_settings_action
reach_quick_settings_begin_drag_if_hit(reach_quick_settings *quick_settings, int32_t x, int32_t y)
{
    reach_quick_settings_action action = {};
    if (quick_settings == nullptr)
    {
        return action;
    }
    reach_quick_settings_state *state = reach_quick_settings_state_mut(quick_settings);

    reach_quick_settings_hit_result hit =
        reach_quick_settings_hit_test(&state->layout, &state->model,
                                      (float)x - state->bounds.x, (float)y - state->bounds.y);
    action = reach_quick_settings_action_for_hit(hit);
    if (action.type == REACH_QUICK_SETTINGS_ACTION_NONE)
    {
        return action;
    }

    state->drag.active = action.type == REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME ||
                         action.type == REACH_QUICK_SETTINGS_ACTION_SET_SESSION_VOLUME ||
                         action.type == REACH_QUICK_SETTINGS_ACTION_SET_BRIGHTNESS;
    state->drag.type = hit.type;
    state->drag.last_level = action.volume_level;
    state->drag.level_valid = state->drag.active;
    state->drag.session_index = hit.session_index;
    reach_quick_settings_copy_utf16(state->drag.session_instance_id,
                                    REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY,
                                    hit.session_instance_id);
    return action;
}

reach_quick_settings_action reach_quick_settings_drag_move(reach_quick_settings *quick_settings,
                                                           int32_t x, int32_t y)
{
    (void)y;
    reach_quick_settings_action action = {};
    if (quick_settings == nullptr)
    {
        return action;
    }
    reach_quick_settings_state *state = reach_quick_settings_state_mut(quick_settings);

    reach_rect_f32 track = state->layout.main_slider_track;
    if (state->drag.type == REACH_QUICK_SETTINGS_HIT_SESSION_SLIDER &&
        state->drag.session_index < state->layout.app_volume_row_count)
    {
        track = state->layout.app_volume_rows[state->drag.session_index].slider_full_range_line;
    }
    else if (state->drag.type == REACH_QUICK_SETTINGS_HIT_BRIGHTNESS_SLIDER)
    {
        track = state->layout.brightness_slider_track;
    }

    if (track.width <= 0.0f)
    {
        return action;
    }

    float local_x = (float)x - state->bounds.x;
    float next_level = reach_quick_settings_clamp01((local_x - track.x) / track.width);

    if (state->drag.level_valid && fabsf(next_level - state->drag.last_level) < 0.005f)
    {
        return action;
    }

    state->drag.last_level = next_level;
    state->drag.level_valid = 1;

    if (state->drag.type == REACH_QUICK_SETTINGS_HIT_SESSION_SLIDER)
    {
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_SESSION_VOLUME;
    }
    else if (state->drag.type == REACH_QUICK_SETTINGS_HIT_BRIGHTNESS_SLIDER)
    {
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_BRIGHTNESS;
    }
    else
    {
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME;
    }

    action.volume_level = next_level;
    action.session_index = state->drag.session_index;
    reach_quick_settings_copy_utf16(action.session_instance_id,
                                    REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY,
                                    state->drag.session_instance_id);
    return action;
}

void reach_quick_settings_end_drag(reach_quick_settings *quick_settings)
{
    if (quick_settings == nullptr)
    {
        return;
    }
    reach_quick_settings_state *state = reach_quick_settings_state_mut(quick_settings);
    state->drag.active = 0;
    state->drag.type = REACH_QUICK_SETTINGS_HIT_NONE;
    state->drag.level_valid = 0;
    state->drag.session_index = 0;
    state->drag.session_instance_id[0] = 0;
}
