#ifndef REACH_FEATURES_CLIPBOARD_COMMON_H
#define REACH_FEATURES_CLIPBOARD_COMMON_H

#include "reach/features/clipboard.h"

reach_clipboard_state *reach_clipboard_feature_state_mut(reach_clipboard_feature *clipboard);

typedef enum reach_clipboard_hit_type
{
    REACH_CLIPBOARD_HIT_NONE = 0,
    REACH_CLIPBOARD_HIT_ITEM = 1,
    REACH_CLIPBOARD_HIT_SCROLLBAR_TRACK = 2,
    REACH_CLIPBOARD_HIT_SCROLLBAR_THUMB = 3,
    REACH_CLIPBOARD_HIT_ITEM_CLOSE = 4,
    REACH_CLIPBOARD_HIT_CLEAR = 5
} reach_clipboard_hit_type;

typedef struct reach_clipboard_hit_result
{
    reach_clipboard_hit_type type;
    size_t index;
} reach_clipboard_hit_result;

typedef enum reach_clipboard_layout_track
{
    REACH_CLIPBOARD_LAYOUT_HEIGHT = 0,
    REACH_CLIPBOARD_LAYOUT_ITEM_WIDTH = 1
} reach_clipboard_layout_track;

reach_clipboard_hit_result reach_clipboard_hit_test(const reach_clipboard_model *model,
                                                    const reach_clipboard_layout *layout, int32_t x,
                                                    int32_t y);

void reach_clipboard_feature_reset(reach_clipboard_feature *clipboard);
void reach_clipboard_feature_tick(reach_clipboard_feature *clipboard, double delta_seconds);
void reach_clipboard_feature_collapse_all_hover(reach_clipboard_feature *clipboard);
void reach_clipboard_feature_move_hover(reach_clipboard_feature *clipboard, size_t previous,
                                        size_t next);
void reach_clipboard_feature_clear_hover(reach_clipboard_feature *clipboard, size_t previous);
int32_t reach_clipboard_feature_any_hover_active(const reach_clipboard_feature *clipboard);
int32_t reach_clipboard_feature_any_animation_active(const reach_clipboard_feature *clipboard);
void reach_clipboard_feature_fill_hover_values(const reach_clipboard_feature *clipboard,
                                               float *out_values, size_t count);
float reach_clipboard_feature_animate_layout_value(reach_clipboard_feature *clipboard,
                                                   reach_clipboard_layout_track track, float target,
                                                   int32_t *out_active);

typedef enum reach_clipboard_action_type
{
    REACH_CLIPBOARD_ACTION_NONE = 0,
    REACH_CLIPBOARD_ACTION_CLEAR_ALL = 1,
    REACH_CLIPBOARD_ACTION_REMOVE_ITEM = 2,
    REACH_CLIPBOARD_ACTION_RESTORE_ITEM = 3
} reach_clipboard_action_type;

typedef struct reach_clipboard_event_result
{
    int32_t handled;
    int32_t redraw;
    int32_t relayout;
    int32_t request_update;
    int32_t capture_pointer;
    reach_clipboard_action_type action;
    size_t item_index;
    uint64_t item_id;
} reach_clipboard_event_result;

void reach_clipboard_pointer_down(reach_clipboard_feature *clipboard, int32_t x, int32_t y,
                                  reach_clipboard_event_result *out);
void reach_clipboard_pointer_up(reach_clipboard_feature *clipboard, int32_t x, int32_t y,
                                reach_clipboard_event_result *out);
void reach_clipboard_scrollbar_release(reach_clipboard_feature *clipboard,
                                       reach_clipboard_event_result *out);
void reach_clipboard_scrollbar_drag_move(reach_clipboard_feature *clipboard, int32_t y,
                                         reach_clipboard_event_result *out);
void reach_clipboard_pointer_move(reach_clipboard_feature *clipboard, int32_t x, int32_t y,
                                  reach_clipboard_event_result *out);
void reach_clipboard_wheel(reach_clipboard_feature *clipboard, int32_t x, int32_t y,
                           int32_t wheel_delta, reach_clipboard_event_result *out);
void reach_clipboard_clear_press_state(reach_clipboard_feature *clipboard);
int32_t reach_clipboard_pointer_leave(reach_clipboard_feature *clipboard);

#endif
