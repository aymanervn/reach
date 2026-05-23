#ifndef REACH_CORE_RENDER_COMMANDS_H
#define REACH_CORE_RENDER_COMMANDS_H

#include "reach/core/ui_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

#define REACH_MAX_RENDER_COMMANDS 256

typedef enum reach_render_command_type {
    REACH_RENDER_COMMAND_NONE = 0,
    REACH_RENDER_COMMAND_RECT = 1,
    REACH_RENDER_COMMAND_TEXT = 2,
    REACH_RENDER_COMMAND_ICON = 3,
    REACH_RENDER_COMMAND_BLUR_BACKGROUND = 4,
    REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE = 5,
    REACH_RENDER_COMMAND_BACKPLATE_EDGE = 6,
    REACH_RENDER_COMMAND_TRIANGLE = 7,
    REACH_RENDER_COMMAND_NOTCH_STROKE = 8,
    REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT = 9,
    REACH_RENDER_COMMAND_VECTOR_ICON = 10
} reach_render_command_type;

typedef enum reach_vector_icon_id {
    REACH_VECTOR_ICON_NONE = 0,
    REACH_VECTOR_ICON_POWER = 1,
    REACH_VECTOR_ICON_LOCK = 2,
    REACH_VECTOR_ICON_SLEEP = 3,
    REACH_VECTOR_ICON_RESTART = 4,
    REACH_VECTOR_ICON_SHUTDOWN = 5,
    REACH_VECTOR_ICON_SIGN_OUT = 6
} reach_vector_icon_id;

typedef enum reach_text_weight {
    REACH_TEXT_WEIGHT_NORMAL = 400,
    REACH_TEXT_WEIGHT_BOLD = 700
} reach_text_weight;

typedef struct reach_render_command {
    reach_render_command_type type;
    reach_rect_f32 rect;
    reach_color color;
    float radius;
    float stroke_width;
    uint64_t icon_id;
    int32_t text_weight;
    float text_size;
    float notch_center_x;
    float notch_width;
    float notch_height;
    int32_t text_alignment;
    int32_t text_ellipsis;
    uint16_t text[260];
} reach_render_command;

typedef struct reach_render_command_buffer {
    reach_render_command commands[REACH_MAX_RENDER_COMMANDS];
    size_t count;
} reach_render_command_buffer;

void reach_render_command_buffer_clear(reach_render_command_buffer *buffer);
reach_result reach_render_command_buffer_push(reach_render_command_buffer *buffer, const reach_render_command *command);
reach_result reach_ui_build_render_commands(const reach_ui_state *state, const reach_ui_layout *layout, reach_render_command_buffer *buffer);

#ifdef __cplusplus
}
#endif

#endif
