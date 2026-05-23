#ifndef REACH_FEATURES_QUICK_SETTINGS_H
#define REACH_FEATURES_QUICK_SETTINGS_H

#include "reach/core/render_commands.h"
#include "reach/core/theme.h"
#include "reach/ports/audio_volume.h"
#include "reach/support/util.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_quick_settings_model {
    float main_volume_level;
    int32_t main_muted;
    int32_t expanded;
    reach_audio_volume_session_list sessions;
} reach_quick_settings_model;

typedef struct reach_quick_settings_volume_pill_model {
    float volume_level;
    int32_t muted;
    uint32_t icon_id;
    uint16_t label[REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY];
    uint16_t session_instance_id[REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY];
} reach_quick_settings_volume_pill_model;

typedef struct reach_quick_settings_volume_pill_layout {
    reach_rect_f32 bounds;
    reach_rect_f32 header_icon;
    reach_rect_f32 header_label;
    reach_rect_f32 slider_track;
    reach_rect_f32 slider_fill;
} reach_quick_settings_volume_pill_layout;

typedef struct reach_quick_settings_layout {
    reach_rect_f32 content_bounds;

    reach_quick_settings_volume_pill_layout main_volume_pill;
    reach_rect_f32 main_slider_track;
    reach_rect_f32 main_slider_fill;
    reach_quick_settings_volume_pill_layout session_volume_pills[REACH_AUDIO_VOLUME_MAX_SESSIONS];
    size_t session_pill_count;

    reach_rect_f32 expand_button;
    reach_rect_f32 expand_button_label;
    reach_rect_f32 expand_button_icon;
} reach_quick_settings_layout;

typedef enum reach_quick_settings_hit_type {
    REACH_QUICK_SETTINGS_HIT_NONE = 0,
    REACH_QUICK_SETTINGS_HIT_MAIN_SLIDER,
    REACH_QUICK_SETTINGS_HIT_SESSION_SLIDER,
    REACH_QUICK_SETTINGS_HIT_EXPAND_BUTTON
} reach_quick_settings_hit_type;

typedef struct reach_quick_settings_hit_result {
    reach_quick_settings_hit_type type;
    float volume_level;
    size_t session_index;
    uint16_t session_instance_id[REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY];
} reach_quick_settings_hit_result;

typedef enum reach_quick_settings_action_type {
    REACH_QUICK_SETTINGS_ACTION_NONE = 0,
    REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME,
    REACH_QUICK_SETTINGS_ACTION_SET_SESSION_VOLUME,
    REACH_QUICK_SETTINGS_ACTION_SET_SESSION_MUTED,
    REACH_QUICK_SETTINGS_ACTION_EXPAND
} reach_quick_settings_action_type;

typedef struct reach_quick_settings_action {
    reach_quick_settings_action_type type;
    float volume_level;
    int32_t muted;
    size_t session_index;
    uint16_t session_instance_id[REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY];
} reach_quick_settings_action;

typedef struct reach_quick_settings_render_input {
    reach_quick_settings_model model;
    reach_quick_settings_layout layout;
    reach_theme theme;
} reach_quick_settings_render_input;

void reach_quick_settings_model_init(
    reach_quick_settings_model *model
);

void reach_quick_settings_model_set_main_volume(
    reach_quick_settings_model *model,
    float volume_level,
    int32_t muted
);

void reach_quick_settings_model_set_sessions(
    reach_quick_settings_model *model,
    const reach_audio_volume_session_list *sessions
);

uint32_t reach_quick_settings_volume_icon_id(
    float volume_level,
    int32_t muted
);

void reach_quick_settings_volume_pill_model_init(
    reach_quick_settings_volume_pill_model *model,
    float volume_level,
    int32_t muted,
    const uint16_t *label
);

reach_quick_settings_volume_pill_layout reach_quick_settings_volume_pill_layout_for_bounds(
    reach_rect_f32 bounds,
    const reach_theme *theme
);

float reach_quick_settings_volume_pill_level_for_x(
    const reach_quick_settings_volume_pill_layout *layout,
    float x
);

reach_quick_settings_layout reach_quick_settings_layout_for_content_bounds(
    reach_rect_f32 content_bounds,
    const reach_theme *theme,
    const reach_quick_settings_model *model
);

float reach_quick_settings_content_height_for_model(
    const reach_quick_settings_model *model
);

reach_quick_settings_hit_result reach_quick_settings_hit_test(
    const reach_quick_settings_layout *layout,
    const reach_quick_settings_model *model,
    float x,
    float y
);

reach_quick_settings_action reach_quick_settings_action_for_hit(
    reach_quick_settings_hit_result hit
);

reach_result reach_quick_settings_build_render_commands(
    const reach_quick_settings_render_input *input,
    reach_render_command_buffer *commands
);

reach_result reach_quick_settings_push_volume_pill_commands(
    const reach_quick_settings_volume_pill_model *model,
    const reach_quick_settings_volume_pill_layout *layout,
    const reach_theme *theme,
    reach_render_command_buffer *commands
);

#ifdef __cplusplus
}
#endif

#endif
