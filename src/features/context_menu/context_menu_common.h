#ifndef REACH_FEATURES_CONTEXT_MENU_COMMON_H
#define REACH_FEATURES_CONTEXT_MENU_COMMON_H

#include "context_menu_metrics.h"
#include "reach/features/context_menu.h"
#include "reach/features/common/pressable.h"
#include "reach/support/animation.h"

typedef struct reach_context_menu_render_input
{
    const reach_theme *theme;
    reach_rect_f32 bounds;
    reach_rect_f32 item_viewport;
    const reach_rect_f32 *item_slots;
    const uint32_t *item_commands;
    const uint32_t *item_icon_ids;
    size_t item_count;
    size_t hovered_index;
    float notch_center_x;
    int32_t notch_side;
    float dpi_scale;
    int32_t text_alignment_leading;
} reach_context_menu_render_input;

reach_result reach_context_menu_build_render_commands(const reach_context_menu_render_input *input,
                                                      reach_render_command_buffer *out_commands);

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
reach_context_menu_hit_test_close_buttons(const reach_context_menu *menu, int32_t x, int32_t y);
int32_t reach_context_menu_set_hovered(reach_context_menu *menu, size_t index);
int32_t reach_context_menu_set_close_hovered(reach_context_menu *menu, size_t index);

enum
{
    REACH_CONTEXT_MENU_ANIM_HOVER = 0,
    REACH_CONTEXT_MENU_ANIM_CLOSE_HOVER,
    REACH_CONTEXT_MENU_ANIM_WIDTH,
    REACH_CONTEXT_MENU_ANIM_COUNT
};

typedef struct reach_context_menu_preview_card
{
    reach_window_id window;
    size_t index;
    reach_rect_f32 preview;
    reach_rect_f32 header;
    reach_rect_f32 close;
    int32_t live;
} reach_context_menu_preview_card;

typedef struct reach_context_menu_preview_layout
{
    reach_context_menu_preview_card cards[REACH_MENU_MAX_WINDOWS];
    size_t count;
    reach_rect_f32 previous;
    reach_rect_f32 next;
    reach_rect_f32 footer;
    int32_t overflow;
    size_t page_starts[REACH_MENU_MAX_WINDOWS];
    size_t page_count;
    size_t page_index;
} reach_context_menu_preview_layout;

struct reach_context_menu
{
    reach_context_menu_state state;
    reach_animation_manager animations;
    reach_animation_track animation_tracks[REACH_CONTEXT_MENU_ANIM_COUNT];
    reach_feature_transition popup_transition;
    reach_pressable pressable;
    uint64_t pressable_identity;
    reach_menu_request request;
    uintptr_t request_windows[REACH_MENU_MAX_WINDOWS];
    float hover_margin;
    reach_window_tracking *windows;
    reach_icon_service *icons;
    reach_context_menu_preview_layout previews;
    reach_context_menu_open_context preview_context;
    size_t preview_generation;
};

int32_t reach_context_menu_arrange_previews(reach_context_menu *menu,
                                            const reach_context_menu_open_context *ctx);
const uint16_t *reach_context_menu_preview_icon_path(const reach_context_menu *menu,
                                                     reach_window_id window);
reach_result reach_context_menu_render_previews(reach_context_menu *menu,
                                                const reach_context_menu_render_context *ctx,
                                                reach_render_command_buffer *out_commands);

#endif
