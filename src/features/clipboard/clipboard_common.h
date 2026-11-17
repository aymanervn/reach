#ifndef REACH_FEATURES_CLIPBOARD_COMMON_H
#define REACH_FEATURES_CLIPBOARD_COMMON_H

#include "reach/features/clipboard.h"

reach_clipboard_state *reach_clipboard_feature_state_mut(reach_clipboard_feature *clipboard);

static inline size_t reach_clipboard_count_clamped(const reach_clipboard_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    return model->count <= REACH_CLIPBOARD_MAX_ITEMS ? model->count : REACH_CLIPBOARD_MAX_ITEMS;
}

typedef enum reach_clipboard_hit_type
{
    REACH_CLIPBOARD_HIT_NONE = 0,
    REACH_CLIPBOARD_HIT_ITEM = 1,
    REACH_CLIPBOARD_HIT_SCROLLBAR = 2,
    REACH_CLIPBOARD_HIT_ITEM_CLOSE = 4,
    REACH_CLIPBOARD_HIT_CLEAR = 5
} reach_clipboard_hit_type;

typedef struct reach_clipboard_hit_result
{
    reach_clipboard_hit_type type;
    size_t index;
} reach_clipboard_hit_result;

reach_clipboard_hit_result reach_clipboard_hit_test(const reach_clipboard_model *model,
                                                    const reach_clipboard_layout *layout, int32_t x,
                                                    int32_t y);

void reach_clipboard_feature_reset(reach_clipboard_feature *clipboard);
void reach_clipboard_feature_collapse_all_hover(reach_clipboard_feature *clipboard);
void reach_clipboard_feature_move_hover(reach_clipboard_feature *clipboard, size_t previous,
                                        size_t next);
reach_clipboard_layout
reach_clipboard_feature_input_layout(const reach_clipboard_feature *clipboard, double now_seconds);
typedef struct reach_clipboard_frame
{
    float envelope_height;
    reach_rect_f32 origin;
    reach_rect_motion panel;
    reach_motion content_y;
} reach_clipboard_frame;

reach_clipboard_frame reach_clipboard_panel_frame(const reach_clipboard_layout *layout,
                                                  const reach_motion *height,
                                                  int32_t content_follows);
void reach_clipboard_feature_track_pointer(reach_clipboard_feature *clipboard, int32_t inside,
                                           int32_t x, int32_t y);

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
    int32_t sync_pointer_subscriptions;
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
void reach_clipboard_cancel_press(reach_clipboard_feature *clipboard,
                                  reach_clipboard_event_result *out);
int32_t reach_clipboard_pointer_leave(reach_clipboard_feature *clipboard);

#endif
