#ifndef REACH_FEATURES_QUICK_SETTINGS_H
#define REACH_FEATURES_QUICK_SETTINGS_H

#include "reach/core/render_commands.h"
#include "reach/core/theme.h"
#include "reach/support/util.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_quick_settings_model {
    float main_volume_level;
    int32_t main_muted;
    int32_t expanded;
} reach_quick_settings_model;

typedef struct reach_quick_settings_layout {
    reach_rect_f32 content_bounds;

    reach_rect_f32 main_slider_track;
    reach_rect_f32 main_slider_fill;

    reach_rect_f32 expand_button;
    reach_rect_f32 expand_button_label;
    reach_rect_f32 expand_button_icon;
} reach_quick_settings_layout;

typedef enum reach_quick_settings_hit_type {
    REACH_QUICK_SETTINGS_HIT_NONE = 0,
    REACH_QUICK_SETTINGS_HIT_MAIN_SLIDER,
    REACH_QUICK_SETTINGS_HIT_EXPAND_BUTTON
} reach_quick_settings_hit_type;

typedef struct reach_quick_settings_hit_result {
    reach_quick_settings_hit_type type;
    float volume_level;
} reach_quick_settings_hit_result;

typedef enum reach_quick_settings_action_type {
    REACH_QUICK_SETTINGS_ACTION_NONE = 0,
    REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME,
    REACH_QUICK_SETTINGS_ACTION_EXPAND
} reach_quick_settings_action_type;

typedef struct reach_quick_settings_action {
    reach_quick_settings_action_type type;
    float volume_level;
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

reach_quick_settings_layout reach_quick_settings_layout_for_content_bounds(
    reach_rect_f32 content_bounds,
    const reach_theme *theme
);

reach_quick_settings_hit_result reach_quick_settings_hit_test(
    const reach_quick_settings_layout *layout,
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

#ifdef __cplusplus
}
#endif

#endif
