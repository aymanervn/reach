#ifndef REACH_FEATURES_TOP_BAR_COMMON_H
#define REACH_FEATURES_TOP_BAR_COMMON_H

#include "reach/features/top_bar.h"

struct reach_top_bar
{
    reach_animation_manager manager;
    reach_animation_track tracks[REACH_TOP_BAR_ANIM_COUNT];
    reach_top_bar_state state;
};

reach_top_bar_state *reach_top_bar_state_mut(reach_top_bar *top_bar);

reach_top_bar_pointer_region reach_top_bar_hit_test(const reach_top_bar_layout *layout,
                                                    int32_t local_x, int32_t local_y);
int32_t reach_top_bar_feedback_press(reach_top_bar *top_bar, size_t slot);
int32_t reach_top_bar_feedback_release(reach_top_bar *top_bar);

static inline reach_rect_f32 reach_top_bar_rect(float x, float y, float width, float height)
{
    reach_rect_f32 rect = {};
    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;
    return rect;
}

#endif
