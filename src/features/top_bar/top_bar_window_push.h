#ifndef REACH_FEATURES_TOP_BAR_WINDOW_PUSH_H
#define REACH_FEATURES_TOP_BAR_WINDOW_PUSH_H

#include "reach/core/geometry.h"
#include "reach/services/app_control.h"
#include "reach/services/window_tracking.h"

typedef struct reach_top_bar_window_push reach_top_bar_window_push;

typedef struct reach_top_bar_window_push_request
{
    reach_rect_f32 monitor_bounds;
    float push_depth;
    float reveal_progress;
    int32_t bar_can_hide;
    int32_t hover_revealed;
} reach_top_bar_window_push_request;

reach_result reach_top_bar_window_push_create(reach_top_bar_window_push **out_push);
void reach_top_bar_window_push_destroy(reach_top_bar_window_push *push);

void reach_top_bar_window_push_attach(reach_top_bar_window_push *push, reach_app_control *apps,
                                      reach_window_tracking *windows);

void reach_top_bar_window_push_apply(reach_top_bar_window_push *push,
                                     const reach_top_bar_window_push_request *request);
void reach_top_bar_window_push_release(reach_top_bar_window_push *push);

#endif
