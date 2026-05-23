#ifndef REACH_FEATURES_LAUNCHER_H
#define REACH_FEATURES_LAUNCHER_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/render_commands.h"
#include "reach/core/theme.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_launcher_render_input {
    const reach_ui_state *state;
    const reach_launcher_layout *layout;
    int32_t text_alignment_leading;
} reach_launcher_render_input;

typedef enum reach_launcher_hit_type {
    REACH_LAUNCHER_HIT_NONE = 0,
    REACH_LAUNCHER_HIT_PINNED_APP = 1,
    REACH_LAUNCHER_HIT_SEARCH_RESULT = 2
} reach_launcher_hit_type;

typedef struct reach_launcher_hit_result {
    reach_launcher_hit_type type;
    size_t index;
} reach_launcher_hit_result;

typedef enum reach_launcher_action_type {
    REACH_LAUNCHER_ACTION_NONE = 0,
    REACH_LAUNCHER_ACTION_LAUNCH_PINNED = 1,
    REACH_LAUNCHER_ACTION_OPEN_RESULT = 2
} reach_launcher_action_type;

typedef struct reach_launcher_action {
    reach_launcher_action_type type;
    size_t pinned_index;
    uint32_t pin_id;
} reach_launcher_action;

reach_result reach_launcher_build_render_commands(const reach_launcher_render_input *input, reach_render_command_buffer *out_commands);
reach_launcher_hit_result reach_launcher_hit_test(const reach_ui_state *state, const reach_launcher_layout *layout, int32_t x, int32_t y);
reach_launcher_action reach_launcher_action_for_hit(const reach_ui_state *state, reach_launcher_hit_result hit);

#ifdef __cplusplus
}
#endif

#endif
