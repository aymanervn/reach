#include "reach/features/quick_settings.h"

#include "quick_settings_common.h"

#include "reach/features/common/wifi_session.h"

#include <math.h>

static int reach_quick_settings_point_in_rect(reach_rect_f32 rect, float x, float y)
{
    return rect.width > 0.0f && rect.height > 0.0f && x >= rect.x && x <= rect.x + rect.width &&
           y >= rect.y && y <= rect.y + rect.height;
}

static float reach_quick_settings_level_for_track(reach_rect_f32 track, float x)
{
    if (track.width <= 0.0f)
    {
        return 0.0f;
    }
    return reach_quick_settings_clamp01((x - track.x) / track.width);
}

static int32_t reach_quick_settings_hit_pill(const reach_quick_settings_level_pill_layout *pill,
                                             float x, float y, reach_quick_settings_hit_type slider,
                                             size_t mute_target, int32_t mutable_pill,
                                             reach_quick_settings_hit_result *out)
{
    if (!reach_quick_settings_point_in_rect(pill->bounds, x, y))
    {
        return 0;
    }
    if (mutable_pill && x <= pill->bounds.x + pill->bounds.height)
    {
        out->type = REACH_QUICK_SETTINGS_HIT_MUTE;
        out->item_index = mute_target;
        return 1;
    }
    out->type = slider;
    out->volume_level = reach_quick_settings_level_for_track(pill->bounds, x);
    return 1;
}

static reach_quick_settings_hit_result
reach_quick_settings_hit_test_overview(const reach_quick_settings_layout *layout,
                                       const reach_quick_settings_model *model, float x, float y)
{
    reach_quick_settings_hit_result result = {};
    if (reach_quick_settings_hit_pill(&layout->overview_volume, x, y,
                                      REACH_QUICK_SETTINGS_HIT_OVERVIEW_VOLUME_SLIDER,
                                      REACH_QUICK_SETTINGS_MUTE_OVERVIEW, 1, &result))
    {
        return result;
    }
    if (reach_quick_settings_point_in_rect(layout->sound_page_button, x, y))
    {
        result.type = REACH_QUICK_SETTINGS_HIT_SOUND_PAGE_BUTTON;
        return result;
    }
    if (model->brightness.available &&
        reach_quick_settings_hit_pill(&layout->overview_brightness, x, y,
                                      REACH_QUICK_SETTINGS_HIT_BRIGHTNESS_SLIDER, 0, 0, &result))
    {
        return result;
    }
    for (size_t index = 0; index < REACH_QUICK_SETTINGS_TOGGLE_COUNT; ++index)
    {
        if (!reach_quick_settings_point_in_rect(layout->toggles[index].bounds, x, y))
        {
            continue;
        }
        int32_t blocked =
            (index == REACH_QUICK_SETTINGS_TOGGLE_BLUETOOTH && model->bluetooth_pending) ||
            (index == REACH_QUICK_SETTINGS_TOGGLE_WIFI &&
             model->wifi_radio == REACH_WIFI_RADIO_UNAVAILABLE);
        if (!blocked)
        {
            result.type = REACH_QUICK_SETTINGS_HIT_TOGGLE;
            result.item_index = index;
        }
        return result;
    }
    return result;
}

static reach_quick_settings_hit_result
reach_quick_settings_hit_test_sound(const reach_quick_settings_layout *layout,
                                    const reach_quick_settings_model *model, float x, float y)
{
    reach_quick_settings_hit_result result = {};
    if (reach_quick_settings_hit_pill(&layout->sound_volume, x, y,
                                      REACH_QUICK_SETTINGS_HIT_MAIN_SLIDER,
                                      REACH_QUICK_SETTINGS_MUTE_SOUND, 1, &result))
    {
        return result;
    }
    if (reach_quick_settings_point_in_rect(layout->output_devices_clip, x, y))
    {
        for (size_t index = 0; index < layout->output_device_row_count; ++index)
        {
            if (reach_quick_settings_point_in_rect(layout->output_device_rows[index].bounds, x, y))
            {
                result.type = REACH_QUICK_SETTINGS_HIT_OUTPUT_DEVICE_ROW;
                result.output_device_index = index;
                if (index < model->output_devices.count)
                {
                    reach_copy_utf16(result.output_device_id, REACH_AUDIO_VOLUME_DEVICE_ID_CAPACITY,
                                     model->output_devices.devices[index].device_id);
                }
                return result;
            }
        }
    }
    if (reach_quick_settings_point_in_rect(layout->app_volumes_clip, x, y))
    {
        for (size_t index = 0; index < layout->app_volume_row_count; ++index)
        {
            if (reach_quick_settings_hit_pill(
                    &layout->app_volume_rows[index], x, y, REACH_QUICK_SETTINGS_HIT_SESSION_SLIDER,
                    REACH_QUICK_SETTINGS_MUTE_FIRST_SESSION + index, 1, &result))
            {
                result.session_index = index;
                if (index < model->sessions.count)
                {
                    reach_copy_utf16(result.session_instance_id,
                                     REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY,
                                     model->sessions.sessions[index].session_instance_id);
                }
                return result;
            }
        }
    }
    if (model->input_available)
    {
        (void)reach_quick_settings_hit_pill(&layout->input_volume, x, y,
                                            REACH_QUICK_SETTINGS_HIT_INPUT_SLIDER,
                                            REACH_QUICK_SETTINGS_MUTE_INPUT, 1, &result);
    }
    return result;
}

static int32_t reach_quick_settings_wifi_row_connectable(const reach_wifi_network *network)
{
    return !network->connected && reach_wifi_security_is_supported(network->security);
}

static reach_quick_settings_hit_result
reach_quick_settings_hit_test_wifi(const reach_quick_settings_layout *layout,
                                   const reach_quick_settings_model *model, float x, float y)
{
    reach_quick_settings_hit_result result = {};
    if (model->wifi_radio != REACH_WIFI_RADIO_UNAVAILABLE &&
        reach_quick_settings_point_in_rect(layout->wifi_switch, x, y))
    {
        result.type = REACH_QUICK_SETTINGS_HIT_WIFI_SWITCH;
        return result;
    }
    if (model->wifi_key_row >= 0)
    {
        if (reach_quick_settings_point_in_rect(layout->wifi_key_field, x, y))
        {
            result.type = REACH_QUICK_SETTINGS_HIT_WIFI_KEY_FIELD;
            return result;
        }
        if (reach_quick_settings_point_in_rect(layout->wifi_key_connect, x, y))
        {
            result.type = REACH_QUICK_SETTINGS_HIT_WIFI_KEY_CONNECT;
            return result;
        }
    }
    for (size_t index = 0; index < layout->wifi_row_count && index < model->wifi_row_count; ++index)
    {
        if (reach_quick_settings_point_in_rect(layout->wifi_forget_buttons[index], x, y))
        {
            result.type = REACH_QUICK_SETTINGS_HIT_WIFI_FORGET;
            result.item_index = index;
            return result;
        }
        if (reach_quick_settings_point_in_rect(layout->wifi_rows[index].bounds, x, y))
        {
            if (reach_quick_settings_wifi_row_connectable(&model->wifi_rows[index]))
            {
                result.type = REACH_QUICK_SETTINGS_HIT_WIFI_ROW;
                result.item_index = index;
            }
            return result;
        }
    }
    return result;
}

static reach_quick_settings_hit_result
reach_quick_settings_hit_test_bluetooth(const reach_quick_settings_layout *layout,
                                        const reach_quick_settings_model *model, float x, float y)
{
    reach_quick_settings_hit_result result = {};
    if (model->bluetooth.available &&
        reach_quick_settings_point_in_rect(layout->bluetooth_switch, x, y))
    {
        result.type = REACH_QUICK_SETTINGS_HIT_BLUETOOTH_SWITCH;
        return result;
    }
    for (size_t button = 0; button < 2; ++button)
    {
        if (reach_quick_settings_point_in_rect(layout->bluetooth_action_buttons[button], x, y))
        {
            result.type = REACH_QUICK_SETTINGS_HIT_BLUETOOTH_ACTION;
            result.item_index = button;
            return result;
        }
    }
    for (size_t index = 0;
         index < layout->bluetooth_row_count && index < model->bluetooth_row_count; ++index)
    {
        if (reach_quick_settings_point_in_rect(layout->bluetooth_rows[index].bounds, x, y))
        {
            const reach_bluetooth_device &device = model->bluetooth_rows[index];
            if (!model->bluetooth_pin_active && (device.paired || device.can_pair))
            {
                result.type = REACH_QUICK_SETTINGS_HIT_BLUETOOTH_ROW;
                result.item_index = index;
            }
            return result;
        }
    }
    return result;
}

reach_quick_settings_hit_result
reach_quick_settings_hit_test(const reach_quick_settings_layout *layout,
                              const reach_quick_settings_model *model, float x, float y)
{
    reach_quick_settings_hit_result result = {};
    if (layout == nullptr || model == nullptr)
    {
        return result;
    }
    for (size_t index = 0; index < REACH_QUICK_SETTINGS_PAGE_COUNT; ++index)
    {
        if (reach_quick_settings_point_in_rect(layout->rail_items[index], x, y))
        {
            result.type = REACH_QUICK_SETTINGS_HIT_RAIL_ITEM;
            result.item_index = index;
            return result;
        }
    }
    switch (model->page)
    {
    case REACH_QUICK_SETTINGS_PAGE_OVERVIEW:
        return reach_quick_settings_hit_test_overview(layout, model, x, y);
    case REACH_QUICK_SETTINGS_PAGE_SOUND:
        return reach_quick_settings_hit_test_sound(layout, model, x, y);
    case REACH_QUICK_SETTINGS_PAGE_WIFI:
        return reach_quick_settings_hit_test_wifi(layout, model, x, y);
    case REACH_QUICK_SETTINGS_PAGE_BLUETOOTH:
        return reach_quick_settings_hit_test_bluetooth(layout, model, x, y);
    default:
        return result;
    }
}

reach_quick_settings_action reach_quick_settings_action_for_hit(reach_quick_settings_hit_result hit)
{
    reach_quick_settings_action action = {};
    action.item_index = hit.item_index;
    action.volume_level = reach_quick_settings_clamp01(hit.volume_level);
    switch (hit.type)
    {
    case REACH_QUICK_SETTINGS_HIT_MAIN_SLIDER:
    case REACH_QUICK_SETTINGS_HIT_OVERVIEW_VOLUME_SLIDER:
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME;
        break;
    case REACH_QUICK_SETTINGS_HIT_SESSION_SLIDER:
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_SESSION_VOLUME;
        action.session_index = hit.session_index;
        reach_copy_utf16(action.session_instance_id, REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY,
                         hit.session_instance_id);
        break;
    case REACH_QUICK_SETTINGS_HIT_BRIGHTNESS_SLIDER:
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_BRIGHTNESS;
        break;
    case REACH_QUICK_SETTINGS_HIT_INPUT_SLIDER:
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_INPUT_VOLUME;
        break;
    case REACH_QUICK_SETTINGS_HIT_RAIL_ITEM:
        action.type = REACH_QUICK_SETTINGS_ACTION_SELECT_PAGE;
        break;
    case REACH_QUICK_SETTINGS_HIT_SOUND_PAGE_BUTTON:
        action.type = REACH_QUICK_SETTINGS_ACTION_SELECT_PAGE;
        action.item_index = REACH_QUICK_SETTINGS_PAGE_SOUND;
        break;
    case REACH_QUICK_SETTINGS_HIT_TOGGLE:
    {
        static const reach_quick_settings_action_type toggle_actions[] = {
            REACH_QUICK_SETTINGS_ACTION_TOGGLE_WIFI, REACH_QUICK_SETTINGS_ACTION_TOGGLE_BLUETOOTH,
            REACH_QUICK_SETTINGS_ACTION_TOGGLE_DARK_MODE, REACH_QUICK_SETTINGS_ACTION_OPEN_PROJECT};
        static_assert(sizeof(toggle_actions) / sizeof(toggle_actions[0]) ==
                          REACH_QUICK_SETTINGS_TOGGLE_COUNT,
                      "every toggle has an action");
        if (hit.item_index < REACH_QUICK_SETTINGS_TOGGLE_COUNT)
        {
            action.type = toggle_actions[hit.item_index];
        }
        break;
    }
    case REACH_QUICK_SETTINGS_HIT_OUTPUT_DEVICE_ROW:
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_OUTPUT_DEVICE;
        action.output_device_index = hit.output_device_index;
        reach_copy_utf16(action.output_device_id, REACH_AUDIO_VOLUME_DEVICE_ID_CAPACITY,
                         hit.output_device_id);
        break;
    case REACH_QUICK_SETTINGS_HIT_MUTE:
        action.type = REACH_QUICK_SETTINGS_ACTION_TOGGLE_MUTE;
        break;
    case REACH_QUICK_SETTINGS_HIT_WIFI_SWITCH:
        action.type = REACH_QUICK_SETTINGS_ACTION_TOGGLE_WIFI;
        break;
    case REACH_QUICK_SETTINGS_HIT_WIFI_ROW:
        action.type = REACH_QUICK_SETTINGS_ACTION_CONNECT_WIFI;
        break;
    case REACH_QUICK_SETTINGS_HIT_WIFI_KEY_FIELD:
        action.type = REACH_QUICK_SETTINGS_ACTION_FOCUS_WIFI_KEY;
        break;
    case REACH_QUICK_SETTINGS_HIT_WIFI_KEY_CONNECT:
        action.type = REACH_QUICK_SETTINGS_ACTION_SUBMIT_WIFI_KEY;
        break;
    case REACH_QUICK_SETTINGS_HIT_WIFI_FORGET:
        action.type = REACH_QUICK_SETTINGS_ACTION_FORGET_WIFI;
        break;
    case REACH_QUICK_SETTINGS_HIT_BLUETOOTH_SWITCH:
        action.type = REACH_QUICK_SETTINGS_ACTION_TOGGLE_BLUETOOTH;
        break;
    case REACH_QUICK_SETTINGS_HIT_BLUETOOTH_ROW:
        action.type = REACH_QUICK_SETTINGS_ACTION_SELECT_BLUETOOTH;
        break;
    case REACH_QUICK_SETTINGS_HIT_BLUETOOTH_ACTION:
        action.type = REACH_QUICK_SETTINGS_ACTION_BLUETOOTH_ACTION;
        break;
    case REACH_QUICK_SETTINGS_HIT_NONE:
    default:
        action.type = REACH_QUICK_SETTINGS_ACTION_NONE;
        break;
    }
    return action;
}

static int32_t reach_quick_settings_hit_is_slider(reach_quick_settings_hit_type type)
{
    return type == REACH_QUICK_SETTINGS_HIT_MAIN_SLIDER ||
           type == REACH_QUICK_SETTINGS_HIT_OVERVIEW_VOLUME_SLIDER ||
           type == REACH_QUICK_SETTINGS_HIT_SESSION_SLIDER ||
           type == REACH_QUICK_SETTINGS_HIT_BRIGHTNESS_SLIDER ||
           type == REACH_QUICK_SETTINGS_HIT_INPUT_SLIDER;
}

reach_quick_settings_action
reach_quick_settings_begin_slider_gesture_if_hit(reach_quick_settings *quick_settings, int32_t x,
                                                 int32_t y)
{
    reach_quick_settings_action action = {};
    if (quick_settings == nullptr)
    {
        return action;
    }
    reach_quick_settings_state *state = reach_quick_settings_state_mut(quick_settings);
    if (!reach_quick_settings_point_in_rect(state->content_bounds, (float)x, (float)y))
    {
        return action;
    }
    reach_quick_settings_hit_result hit =
        reach_quick_settings_hit_test(&state->layout, &state->model, (float)x, (float)y);
    if (!reach_quick_settings_hit_is_slider(hit.type))
    {
        return action;
    }
    action = reach_quick_settings_action_for_hit(hit);
    state->drag.active = 1;
    state->drag.type = hit.type;
    state->drag.last_level = action.volume_level;
    state->drag.level_valid = 1;
    state->drag.session_index = hit.session_index;
    reach_copy_utf16(state->drag.session_instance_id, REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY,
                     hit.session_instance_id);
    return action;
}

uint64_t reach_quick_settings_pressable_target(reach_quick_settings_hit_result hit)
{
    reach_quick_settings_press_target_kind kind = REACH_QUICK_SETTINGS_PRESS_TARGET_NONE;
    size_t index = 0;
    switch (hit.type)
    {
    case REACH_QUICK_SETTINGS_HIT_RAIL_ITEM:
        kind = REACH_QUICK_SETTINGS_PRESS_TARGET_RAIL_ITEM;
        index = hit.item_index;
        break;
    case REACH_QUICK_SETTINGS_HIT_SOUND_PAGE_BUTTON:
        kind = REACH_QUICK_SETTINGS_PRESS_TARGET_SOUND_PAGE_BUTTON;
        break;
    case REACH_QUICK_SETTINGS_HIT_TOGGLE:
        kind = REACH_QUICK_SETTINGS_PRESS_TARGET_TOGGLE;
        index = hit.item_index;
        break;
    case REACH_QUICK_SETTINGS_HIT_OUTPUT_DEVICE_ROW:
        kind = REACH_QUICK_SETTINGS_PRESS_TARGET_OUTPUT_DEVICE_ROW;
        index = hit.output_device_index;
        break;
    case REACH_QUICK_SETTINGS_HIT_MUTE:
        kind = REACH_QUICK_SETTINGS_PRESS_TARGET_MUTE;
        index = hit.item_index;
        break;
    case REACH_QUICK_SETTINGS_HIT_WIFI_SWITCH:
        kind = REACH_QUICK_SETTINGS_PRESS_TARGET_WIFI_SWITCH;
        break;
    case REACH_QUICK_SETTINGS_HIT_WIFI_ROW:
        kind = REACH_QUICK_SETTINGS_PRESS_TARGET_WIFI_ROW;
        index = hit.item_index;
        break;
    case REACH_QUICK_SETTINGS_HIT_WIFI_KEY_FIELD:
        kind = REACH_QUICK_SETTINGS_PRESS_TARGET_WIFI_KEY_FIELD;
        break;
    case REACH_QUICK_SETTINGS_HIT_WIFI_KEY_CONNECT:
        kind = REACH_QUICK_SETTINGS_PRESS_TARGET_WIFI_KEY_CONNECT;
        break;
    case REACH_QUICK_SETTINGS_HIT_WIFI_FORGET:
        kind = REACH_QUICK_SETTINGS_PRESS_TARGET_WIFI_FORGET;
        index = hit.item_index;
        break;
    case REACH_QUICK_SETTINGS_HIT_BLUETOOTH_SWITCH:
        kind = REACH_QUICK_SETTINGS_PRESS_TARGET_BLUETOOTH_SWITCH;
        break;
    case REACH_QUICK_SETTINGS_HIT_BLUETOOTH_ROW:
        kind = REACH_QUICK_SETTINGS_PRESS_TARGET_BLUETOOTH_ROW;
        index = hit.item_index;
        break;
    case REACH_QUICK_SETTINGS_HIT_BLUETOOTH_ACTION:
        kind = REACH_QUICK_SETTINGS_PRESS_TARGET_BLUETOOTH_ACTION;
        index = hit.item_index;
        break;
    default:
        return REACH_PRESSABLE_TARGET_NONE;
    }
    return (static_cast<uint64_t>(kind) << 32) | static_cast<uint32_t>(index);
}

reach_quick_settings_press_target_kind reach_quick_settings_pressable_target_kind(uint64_t target)
{
    return target == REACH_PRESSABLE_TARGET_NONE
               ? REACH_QUICK_SETTINGS_PRESS_TARGET_NONE
               : static_cast<reach_quick_settings_press_target_kind>(target >> 32);
}

size_t reach_quick_settings_pressable_target_index(uint64_t target)
{
    return target == REACH_PRESSABLE_TARGET_NONE ? 0 : static_cast<size_t>(target & UINT32_MAX);
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
    const reach_quick_settings_layout &layout = state->layout;

    reach_rect_f32 track = {};
    switch (state->drag.type)
    {
    case REACH_QUICK_SETTINGS_HIT_SESSION_SLIDER:
        if (state->drag.session_index < layout.app_volume_row_count)
        {
            track = layout.app_volume_rows[state->drag.session_index].bounds;
        }
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_SESSION_VOLUME;
        break;
    case REACH_QUICK_SETTINGS_HIT_BRIGHTNESS_SLIDER:
        track = layout.overview_brightness.bounds;
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_BRIGHTNESS;
        break;
    case REACH_QUICK_SETTINGS_HIT_OVERVIEW_VOLUME_SLIDER:
        track = layout.overview_volume.bounds;
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME;
        break;
    case REACH_QUICK_SETTINGS_HIT_INPUT_SLIDER:
        track = layout.input_volume.bounds;
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_INPUT_VOLUME;
        break;
    default:
        track = layout.sound_volume.bounds;
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME;
        break;
    }

    if (track.width <= 0.0f)
    {
        return reach_quick_settings_action{};
    }

    float next_level = reach_quick_settings_level_for_track(track, (float)x);
    if (state->drag.level_valid && (next_level == state->drag.last_level ||
                                    (fabsf(next_level - state->drag.last_level) < 0.005f &&
                                     next_level > 0.0f && next_level < 1.0f)))
    {
        return reach_quick_settings_action{};
    }

    state->drag.last_level = next_level;
    state->drag.level_valid = 1;
    action.volume_level = next_level;
    action.session_index = state->drag.session_index;
    reach_copy_utf16(action.session_instance_id, REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY,
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
