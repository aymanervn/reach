#ifndef REACH_FEATURES_CONTEXT_MENU_COMMON_H
#define REACH_FEATURES_CONTEXT_MENU_COMMON_H

#include "context_menu_metrics.h"
#include "reach/features/context_menu.h"

typedef struct reach_context_menu_render_input
{
    const reach_theme *theme;
    reach_rect_f32 bounds;
    const reach_rect_f32 *item_slots;
    const uint32_t *item_commands;
    const uint32_t *item_icon_ids;
    const uint16_t (*item_titles)[260];
    size_t item_count;
    size_t hovered_index;
    float notch_center_x;
    int32_t notch_side;
    float dpi_scale;
    int32_t text_alignment_leading;
    int32_t window_list;
    float hover_opacity;
    size_t close_hovered_index;
    float close_hover;
} reach_context_menu_render_input;

reach_result
reach_context_menu_build_render_commands(const reach_context_menu_render_input *input,
                                         reach_render_command_buffer *out_commands);

reach_rect_f32 reach_context_menu_close_button_rect(const reach_context_menu_metrics *metrics,
                                                    reach_rect_f32 item_slot, float dpi_scale);

typedef struct reach_context_menu_hit_result
{
    int32_t hit;
    size_t index;
} reach_context_menu_hit_result;

typedef struct reach_context_menu_action
{
    uint32_t command;
} reach_context_menu_action;

reach_context_menu_hit_result reach_context_menu_hit_test_items(const reach_rect_f32 *item_slots,
                                                                size_t item_count, int32_t x,
                                                                int32_t y);
reach_context_menu_action reach_context_menu_action_for_hit(const uint32_t *item_commands,
                                                            size_t item_count,
                                                            reach_context_menu_hit_result hit);
reach_context_menu_hit_result
reach_context_menu_hit_test_close_buttons(const reach_context_menu_state *state, int32_t x,
                                          int32_t y);
int32_t reach_context_menu_set_hovered(reach_context_menu *menu, size_t index);
int32_t reach_context_menu_set_close_hovered(reach_context_menu *menu, size_t index);

#endif
