#ifndef REACH_FEATURES_CONTEXT_MENU_COMMON_H
#define REACH_FEATURES_CONTEXT_MENU_COMMON_H

#include "context_menu_metrics.h"
#include "reach/features/context_menu.h"
#include "reach/features/common/pressable.h"

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
    int32_t top_bar_dropdown;
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

typedef struct reach_context_menu_preview_card
{
    reach_window_id window;
    size_t index;
    float source_width;
    float source_height;
    float header_height;
    float header_gap;
    reach_rect_motion motion;
    reach_motion size;
    reach_motion header_top;
    uint64_t render_id;
    int32_t live;
} reach_context_menu_preview_card;

typedef struct reach_context_menu_preview_placement
{
    reach_window_id window;
    size_t index;
    reach_rect_f32 preview;
    reach_rect_f32 header;
    reach_rect_f32 close;
    int32_t live;
} reach_context_menu_preview_placement;

typedef struct reach_context_menu_preview_layout
{
    reach_context_menu_preview_card cards[REACH_MENU_MAX_WINDOWS];
    size_t count;
    reach_rect_motion body;
    reach_rect_motion viewport;
    reach_rect_f32 envelope;
    reach_motion notch;
    float header_y;
    float border;
    float card_scale;
    reach_motion card_size;
} reach_context_menu_preview_layout;

typedef struct reach_context_menu_preview_origin
{
    reach_window_id window;
    float screen_x;
} reach_context_menu_preview_origin;

struct reach_context_menu
{
    reach_context_menu_state state;
    reach_motion width;
    int32_t layout_initialized;
    int32_t layout_cleanup_pending;
    uint64_t next_card_id = UINT64_C(0x10000);
    reach_motion hover;
    reach_motion close_hover;
    reach_motion_spec smooth_spec;
    reach_motion_spec fade_in_spec;
    reach_motion_spec fade_out_spec;
    reach_motion reflow;
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
    reach_context_menu_preview_origin preview_origins[REACH_MENU_MAX_WINDOWS];
    size_t preview_origin_count;
};

reach_context_menu_preview_placement
reach_context_menu_preview_card_at(const reach_context_menu *menu, size_t index,
                                   double now_seconds);
reach_context_menu_hit_result reach_context_menu_hit_test_menu_items(const reach_context_menu *menu,
                                                                     int32_t x, int32_t y);

void reach_context_menu_stop_preview_reflow(reach_context_menu *menu);
void reach_context_menu_reset_preview_layout(reach_context_menu *menu);
void reach_context_menu_anchor_presentation(reach_context_menu *menu);

static inline int32_t reach_context_menu_command_group(uint32_t command)
{
    switch (command)
    {
    case REACH_CONTEXT_MENU_COMMAND_SYSTEM_INFORMATION:
    case REACH_CONTEXT_MENU_COMMAND_TASK_MANAGER:
    case REACH_CONTEXT_MENU_COMMAND_DEVICE_MANAGER:
        return 1;
    case REACH_CONTEXT_MENU_COMMAND_REACH_SETTINGS:
    case REACH_CONTEXT_MENU_COMMAND_CONTROL_PANEL:
    case REACH_CONTEXT_MENU_COMMAND_ENVIRONMENT_VARIABLES:
    case REACH_CONTEXT_MENU_COMMAND_SERVICES:
        return 2;
    case REACH_CONTEXT_MENU_COMMAND_POWER_SLEEP:
    case REACH_CONTEXT_MENU_COMMAND_POWER_RESTART:
    case REACH_CONTEXT_MENU_COMMAND_POWER_SHUTDOWN:
        return 3;
    case REACH_CONTEXT_MENU_COMMAND_POWER_LOCK:
    case REACH_CONTEXT_MENU_COMMAND_POWER_SIGN_OUT:
        return 4;
    default:
        return 0;
    }
}

static inline int32_t reach_context_menu_separator_before(const uint32_t *commands, size_t index)
{
    if (commands == nullptr || index == 0)
        return 0;
    int32_t group = reach_context_menu_command_group(commands[index]);
    int32_t previous = reach_context_menu_command_group(commands[index - 1]);
    return group != 0 && previous != 0 && group != previous;
}

static inline void reach_context_menu_load_motion_specs(reach_context_menu *menu,
                                                        const reach_theme *theme)
{
    const reach_theme *resolved = theme != nullptr ? theme : reach_theme_default();
    menu->smooth_spec = reach_theme_motion_preset(resolved, REACH_MOTION_PRESET_SMOOTH);
    menu->fade_in_spec = reach_theme_motion_preset(resolved, REACH_MOTION_PRESET_FADE_IN);
    menu->fade_out_spec = reach_theme_motion_preset(resolved, REACH_MOTION_PRESET_FADE_OUT);
}
reach_rect_f32 reach_context_menu_preview_bounds_at(const reach_context_menu *menu,
                                                    const reach_rect_motion *bounds, double now);
reach_result reach_context_menu_reflow_previews(reach_context_menu *menu,
                                                reach_context_menu_preview_layout *layout,
                                                double now, int32_t restart, int32_t cut);

int32_t reach_context_menu_arrange_previews(reach_context_menu *menu,
                                            const reach_context_menu_open_context *ctx);
int32_t reach_context_menu_settle_previews(reach_context_menu *menu);
const uint16_t *reach_context_menu_preview_icon_path(const reach_context_menu *menu,
                                                     reach_window_id window);
reach_result reach_context_menu_render_previews(reach_context_menu *menu,
                                                const reach_context_menu_render_context *ctx,
                                                reach_render_command_buffer *out_commands);

#endif
