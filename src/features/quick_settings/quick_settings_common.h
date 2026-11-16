#ifndef REACH_FEATURES_QUICK_SETTINGS_COMMON_H
#define REACH_FEATURES_QUICK_SETTINGS_COMMON_H

#include <stddef.h>
#include <stdint.h>

#include "reach/features/common/level_presentation.h"
#include "reach/features/common/pressable.h"

typedef struct reach_quick_settings reach_quick_settings;
typedef struct reach_quick_settings_state reach_quick_settings_state;

typedef enum reach_quick_settings_hit_type
{
    REACH_QUICK_SETTINGS_HIT_NONE = 0,
    REACH_QUICK_SETTINGS_HIT_MAIN_SLIDER,
    REACH_QUICK_SETTINGS_HIT_SESSION_SLIDER,
    REACH_QUICK_SETTINGS_HIT_BRIGHTNESS_SLIDER,
    REACH_QUICK_SETTINGS_HIT_OVERVIEW_VOLUME_SLIDER,
    REACH_QUICK_SETTINGS_HIT_RAIL_ITEM,
    REACH_QUICK_SETTINGS_HIT_SOUND_PAGE_BUTTON,
    REACH_QUICK_SETTINGS_HIT_TOGGLE,
    REACH_QUICK_SETTINGS_HIT_OUTPUT_DEVICE_ROW,
    REACH_QUICK_SETTINGS_HIT_MUTE,
    REACH_QUICK_SETTINGS_HIT_INPUT_SLIDER,
    REACH_QUICK_SETTINGS_HIT_WIFI_SWITCH,
    REACH_QUICK_SETTINGS_HIT_WIFI_ROW
} reach_quick_settings_hit_type;

enum
{
    REACH_QUICK_SETTINGS_MUTE_OVERVIEW = 0,
    REACH_QUICK_SETTINGS_MUTE_SOUND,
    REACH_QUICK_SETTINGS_MUTE_INPUT,
    REACH_QUICK_SETTINGS_MUTE_FIRST_SESSION
};

typedef enum reach_quick_settings_press_target_kind
{
    REACH_QUICK_SETTINGS_PRESS_TARGET_NONE = 0,
    REACH_QUICK_SETTINGS_PRESS_TARGET_RAIL_ITEM,
    REACH_QUICK_SETTINGS_PRESS_TARGET_SOUND_PAGE_BUTTON,
    REACH_QUICK_SETTINGS_PRESS_TARGET_TOGGLE,
    REACH_QUICK_SETTINGS_PRESS_TARGET_OUTPUT_DEVICE_ROW,
    REACH_QUICK_SETTINGS_PRESS_TARGET_MUTE,
    REACH_QUICK_SETTINGS_PRESS_TARGET_WIFI_SWITCH,
    REACH_QUICK_SETTINGS_PRESS_TARGET_WIFI_ROW
} reach_quick_settings_press_target_kind;
#ifdef __cplusplus
extern "C"
{
#endif
    reach_quick_settings_state *
    reach_quick_settings_state_mut(reach_quick_settings *quick_settings);
    void reach_quick_settings_reset(reach_quick_settings *quick_settings);
    reach_rect_f32 reach_quick_settings_content_bounds_for(reach_rect_f32 surface_bounds,
                                                           const reach_theme *theme,
                                                           float dpi_scale, int32_t direction);
    uint64_t reach_quick_settings_press_feedback_target(const reach_quick_settings *quick_settings);
    reach_motion
    reach_quick_settings_press_feedback_motion(const reach_quick_settings *quick_settings);
    void reach_quick_settings_page_motions(
        const reach_quick_settings *quick_settings,
        reach_motion out_toggles[REACH_QUICK_SETTINGS_TOGGLE_COUNT],
        reach_motion out_page_opacity[REACH_QUICK_SETTINGS_PAGE_COUNT],
        reach_motion out_page_shift[REACH_QUICK_SETTINGS_PAGE_COUNT],
        reach_motion *out_rail_marker);
    void reach_quick_settings_row_motions(
        const reach_quick_settings *quick_settings,
        reach_keyed_row out_output_motions[REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES],
        reach_quick_settings_output_device_row
            out_output_devices[REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES],
        reach_keyed_row out_app_motions[REACH_AUDIO_VOLUME_MAX_SESSIONS],
        reach_quick_settings_app_volume_row out_app_volumes[REACH_AUDIO_VOLUME_MAX_SESSIONS]);
    void
    reach_quick_settings_wifi_motions(const reach_quick_settings *quick_settings,
                                      reach_keyed_row out_rows[REACH_QUICK_SETTINGS_WIFI_ROWS],
                                      reach_wifi_network out_items[REACH_QUICK_SETTINGS_WIFI_ROWS],
                                      reach_motion *out_switch);
    reach_quick_settings_layout reach_quick_settings_layout_for_sections_scaled(
        reach_rect_f32 content_bounds, const reach_theme *theme,
        const reach_quick_settings_model *model, float output_devices_span, float app_volumes_span,
        float dpi_scale);
    reach_quick_settings_output_device_row_layout
    reach_quick_settings_output_device_row_layout_at(reach_rect_f32 panel, float top,
                                                     float dpi_scale);
    reach_quick_settings_level_pill_layout
    reach_quick_settings_app_volume_row_layout_at(reach_rect_f32 panel, float top, float dpi_scale);
#ifdef __cplusplus
}
#endif

typedef enum reach_quick_settings_action_type
{
    REACH_QUICK_SETTINGS_ACTION_NONE = 0,
    REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME,
    REACH_QUICK_SETTINGS_ACTION_SET_SESSION_VOLUME,
    REACH_QUICK_SETTINGS_ACTION_SET_BRIGHTNESS,
    REACH_QUICK_SETTINGS_ACTION_SELECT_PAGE,
    REACH_QUICK_SETTINGS_ACTION_TOGGLE_WIFI,
    REACH_QUICK_SETTINGS_ACTION_TOGGLE_BLUETOOTH,
    REACH_QUICK_SETTINGS_ACTION_TOGGLE_DARK_MODE,
    REACH_QUICK_SETTINGS_ACTION_OPEN_PROJECT,
    REACH_QUICK_SETTINGS_ACTION_SET_OUTPUT_DEVICE,
    REACH_QUICK_SETTINGS_ACTION_TOGGLE_MUTE,
    REACH_QUICK_SETTINGS_ACTION_SET_INPUT_VOLUME,
    REACH_QUICK_SETTINGS_ACTION_CONNECT_WIFI
} reach_quick_settings_action_type;

typedef struct reach_quick_settings_hit_result
{
    reach_quick_settings_hit_type type;
    size_t item_index;
    float volume_level;
    size_t session_index;
    size_t output_device_index;
    uint16_t session_instance_id[REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY];
    uint16_t output_device_id[REACH_AUDIO_VOLUME_DEVICE_ID_CAPACITY];
} reach_quick_settings_hit_result;

typedef struct reach_quick_settings_action
{
    reach_quick_settings_action_type type;
    size_t item_index;
    float volume_level;
    size_t session_index;
    size_t output_device_index;
    uint16_t session_instance_id[REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY];
    uint16_t output_device_id[REACH_AUDIO_VOLUME_DEVICE_ID_CAPACITY];
} reach_quick_settings_action;

reach_quick_settings_hit_result
reach_quick_settings_hit_test(const reach_quick_settings_layout *layout,
                              const reach_quick_settings_model *model, float x, float y);
reach_quick_settings_action
reach_quick_settings_action_for_hit(reach_quick_settings_hit_result hit);
reach_quick_settings_action
reach_quick_settings_begin_slider_gesture_if_hit(reach_quick_settings *quick_settings, int32_t x,
                                                 int32_t y);
reach_quick_settings_action reach_quick_settings_drag_move(reach_quick_settings *quick_settings,
                                                           int32_t x, int32_t y);
void reach_quick_settings_end_drag(reach_quick_settings *quick_settings);
int32_t reach_quick_settings_drag_active(const reach_quick_settings *quick_settings);
uint64_t reach_quick_settings_pressable_target(reach_quick_settings_hit_result hit);
reach_quick_settings_press_target_kind reach_quick_settings_pressable_target_kind(uint64_t target);
size_t reach_quick_settings_pressable_target_index(uint64_t target);

static inline float reach_quick_settings_clamp01(float value)
{
    return reach_level_clamp01(value);
}

static inline float reach_quick_settings_clamp_min0(float value)
{
    return value < 0.0f ? 0.0f : value;
}

#endif
