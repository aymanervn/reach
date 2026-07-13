#ifndef REACH_FEATURES_QUICK_SETTINGS_COMMON_H
#define REACH_FEATURES_QUICK_SETTINGS_COMMON_H

#include <stddef.h>
#include <stdint.h>

/*
 * Feature-internal mutable access to the capsule state. The public
 * reach_quick_settings_state_ptr() is const; everything outside
 * src/features/quick_settings mutates through the semantic ops only.
 */
typedef struct reach_quick_settings reach_quick_settings;
typedef struct reach_quick_settings_state reach_quick_settings_state;

typedef enum reach_quick_settings_hit_type
{
    REACH_QUICK_SETTINGS_HIT_NONE = 0,
    REACH_QUICK_SETTINGS_HIT_MAIN_SLIDER,
    REACH_QUICK_SETTINGS_HIT_SESSION_SLIDER,
    REACH_QUICK_SETTINGS_HIT_BRIGHTNESS_SLIDER,
    REACH_QUICK_SETTINGS_HIT_NETWORK_TILE,
    REACH_QUICK_SETTINGS_HIT_BLUETOOTH_TILE,
    REACH_QUICK_SETTINGS_HIT_BATTERY_SAVER_TILE,
    REACH_QUICK_SETTINGS_HIT_PROJECT_TILE,
    REACH_QUICK_SETTINGS_HIT_OUTPUT_DEVICE_BUTTON,
    REACH_QUICK_SETTINGS_HIT_OUTPUT_DEVICE_ROW,
    REACH_QUICK_SETTINGS_HIT_EXPAND_BUTTON
} reach_quick_settings_hit_type;
#ifdef __cplusplus
extern "C"
{
#endif
reach_quick_settings_state *reach_quick_settings_state_mut(reach_quick_settings *quick_settings);
void reach_quick_settings_reset(reach_quick_settings *quick_settings);
void reach_quick_settings_tick(reach_quick_settings *quick_settings, double delta_seconds);
void reach_quick_settings_start_height_animation(reach_quick_settings *quick_settings,
                                                 float from_height, float to_height);
float reach_quick_settings_height_animation_value(const reach_quick_settings *quick_settings);
#ifdef __cplusplus
}
#endif

typedef enum reach_quick_settings_action_type
{
    REACH_QUICK_SETTINGS_ACTION_NONE = 0,
    REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME,
    REACH_QUICK_SETTINGS_ACTION_SET_SESSION_VOLUME,
    REACH_QUICK_SETTINGS_ACTION_SET_SESSION_MUTED,
    REACH_QUICK_SETTINGS_ACTION_SET_BRIGHTNESS,
    REACH_QUICK_SETTINGS_ACTION_NETWORK_TILE,
    REACH_QUICK_SETTINGS_ACTION_TOGGLE_BLUETOOTH,
    REACH_QUICK_SETTINGS_ACTION_TOGGLE_BATTERY_SAVER,
    REACH_QUICK_SETTINGS_ACTION_OPEN_PROJECT,
    REACH_QUICK_SETTINGS_ACTION_TOGGLE_OUTPUT_DEVICES,
    REACH_QUICK_SETTINGS_ACTION_SET_OUTPUT_DEVICE,
    REACH_QUICK_SETTINGS_ACTION_EXPAND
} reach_quick_settings_action_type;

typedef struct reach_quick_settings_hit_result
{
    reach_quick_settings_hit_type type;
    float volume_level;
    size_t session_index;
    size_t output_device_index;
    uint16_t session_instance_id[REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY];
    uint16_t output_device_id[REACH_AUDIO_VOLUME_DEVICE_ID_CAPACITY];
} reach_quick_settings_hit_result;

typedef struct reach_quick_settings_action
{
    reach_quick_settings_action_type type;
    float volume_level;
    int32_t muted;
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
reach_quick_settings_begin_drag_if_hit(reach_quick_settings *quick_settings, int32_t x, int32_t y);
reach_quick_settings_action reach_quick_settings_drag_move(reach_quick_settings *quick_settings,
                                                           int32_t x, int32_t y);
void reach_quick_settings_end_drag(reach_quick_settings *quick_settings);
int32_t reach_quick_settings_drag_active(const reach_quick_settings *quick_settings);

static inline float reach_quick_settings_clamp01(float value)
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

static inline float reach_quick_settings_clamp_min0(float value)
{
    return value < 0.0f ? 0.0f : value;
}

static inline void reach_quick_settings_copy_utf16(uint16_t *dst, size_t dst_count,
                                                   const uint16_t *src)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }

    size_t index = 0;
    if (src != nullptr)
    {
        while (index + 1 < dst_count && src[index] != 0)
        {
            dst[index] = src[index];
            ++index;
        }
    }
    dst[index] = 0;
}

#endif
