#ifndef REACH_FEATURES_CONTEXT_MENU_H
#define REACH_FEATURES_CONTEXT_MENU_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/render_commands.h"
#include "reach/theme.h"

#ifdef __cplusplus
extern "C" {
#endif

#define REACH_CONTEXT_MENU_MAX_ITEMS 4

typedef enum reach_context_menu_command {
    REACH_CONTEXT_MENU_COMMAND_UNPIN = 100,
    REACH_CONTEXT_MENU_COMMAND_CLOSE = 101,
    REACH_CONTEXT_MENU_COMMAND_OPEN_NEW = 102,
    REACH_CONTEXT_MENU_COMMAND_PIN = 103
} reach_context_menu_command;

typedef struct reach_context_menu_render_input {
    const reach_theme *theme;
    reach_rect_f32 bounds;
    const reach_rect_f32 *item_slots;
    const uint32_t *item_commands;
    size_t item_count;
    size_t hovered_index;
    size_t target_index;
    const reach_dock_layout *dock_layout;
    int32_t has_layout;
    int32_t text_alignment_leading;
} reach_context_menu_render_input;

typedef struct reach_context_menu_hit_result {
    int32_t hit;
    size_t index;
} reach_context_menu_hit_result;

typedef struct reach_context_menu_action {
    uint32_t command;
} reach_context_menu_action;

void reach_context_menu_build_dock_item_commands(
    int32_t pinned,
    int32_t has_path,
    int32_t has_window,
    uint32_t *out_commands,
    size_t *out_count);
const uint16_t *reach_context_menu_command_text(uint32_t command);
reach_result reach_context_menu_build_render_commands(const reach_context_menu_render_input *input, reach_render_command_buffer *out_commands);
reach_context_menu_hit_result reach_context_menu_hit_test_items(
    const reach_rect_f32 *item_slots,
    size_t item_count,
    int32_t x,
    int32_t y);
reach_context_menu_action reach_context_menu_action_for_hit(
    const uint32_t *item_commands,
    size_t item_count,
    reach_context_menu_hit_result hit);

#ifdef __cplusplus
}
#endif

#endif
