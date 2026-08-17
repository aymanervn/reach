#ifndef REACH_FEATURES_TOP_BAR_COMMON_H
#define REACH_FEATURES_TOP_BAR_COMMON_H

#include "reach/features/top_bar.h"

#include "top_bar_metrics.h"
#include "top_bar_now_playing.h"
#include "top_bar_window_push.h"

struct reach_top_bar
{
    reach_animation_manager manager;
    reach_animation_track tracks[REACH_TOP_BAR_ANIM_COUNT];
    reach_top_bar_state state;
    reach_top_bar_now_playing *now_playing_subfeature;
    reach_top_bar_window_push *window_push;
    reach_now_playing_service *now_playing;
    reach_icon_service *icons;
    reach_window_tracking *windows;
    reach_app_control *apps;
    reach_system_stats *stats;
    reach_clock *clock;
    reach_input_language_service *input_language;
    reach_system_status *status;
    float now_playing_target_width;
    float current_app_target_width;
    float tray_target_width;
    float quick_settings_target_width;
    reach_rect_f32 push_monitor_bounds;
    reach_rect_f32 push_shown_bounds;
    float push_depth;
    int32_t push_can_hide;
};

reach_top_bar_state *reach_top_bar_state_mut(reach_top_bar *top_bar);
reach_top_bar_now_playing *reach_top_bar_now_playing_subfeature(reach_top_bar *top_bar);
reach_icon_service *reach_top_bar_icons(reach_top_bar *top_bar);
int32_t reach_top_bar_bluetooth_absence_pending(const reach_top_bar *top_bar);

reach_top_bar_pointer_region reach_top_bar_hit_test(const reach_top_bar_layout *layout,
                                                    int32_t local_x, int32_t local_y);
size_t reach_top_bar_tray_icon_at(const reach_top_bar_layout *layout, int32_t local_x,
                                  int32_t local_y);
int32_t reach_top_bar_feedback_press(reach_top_bar *top_bar, size_t slot);
int32_t reach_top_bar_feedback_release(reach_top_bar *top_bar);

typedef struct reach_top_bar_event_result
{
    int32_t handled;
    int32_t redraw;
    int32_t sync_pointer_subscriptions;
    uint32_t action_kind;
    uint64_t action_id;
} reach_top_bar_event_result;

void reach_top_bar_pointer_down(reach_top_bar *top_bar, int32_t local_x, int32_t local_y,
                                reach_top_bar_event_result *out);
void reach_top_bar_pointer_up(reach_top_bar *top_bar, int32_t local_x, int32_t local_y,
                              reach_top_bar_event_result *out);
void reach_top_bar_pointer_move(reach_top_bar *top_bar, int32_t local_x, int32_t local_y,
                                reach_top_bar_event_result *out);
void reach_top_bar_pointer_context(reach_top_bar *top_bar, int32_t local_x, int32_t local_y,
                                   reach_top_bar_event_result *out);
void reach_top_bar_pointer_cancel(reach_top_bar *top_bar, reach_top_bar_event_result *out);
void reach_top_bar_pointer_leave(reach_top_bar *top_bar, reach_top_bar_event_result *out);

static inline reach_rect_f32 reach_top_bar_rect(float x, float y, float width, float height)
{
    reach_rect_f32 rect = {};
    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;
    return rect;
}

static inline float reach_top_bar_text_advance(const uint16_t *text, float text_size)
{
    if (text == nullptr || text_size <= 0.0f)
    {
        return 0.0f;
    }
    size_t length = 0;
    while (text[length] != 0)
    {
        ++length;
    }
    return (float)length * text_size * reach_top_bar_metrics_values.glyph_advance_ratio;
}

static inline reach_rect_f32 reach_top_bar_text_run(float left, float height, float advance,
                                                    float text_size)
{
    return reach_top_bar_rect(left, 0.0f, advance + text_size, height);
}

#endif
