#ifndef REACH_FEATURES_COMMON_SCROLL_VIEW_H
#define REACH_FEATURES_COMMON_SCROLL_VIEW_H

#include "reach/core/theme.h"
#include "reach/support/motion.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_scroll_view_mode
    {
        REACH_SCROLL_VIEW_SET = 0,
        REACH_SCROLL_VIEW_GLIDE,
        REACH_SCROLL_VIEW_WHEEL
    } reach_scroll_view_mode;

    typedef struct reach_scroll_view
    {
        reach_motion offset;
    } reach_scroll_view;

    typedef struct reach_scroll_view_window
    {
        size_t first;
        size_t count;
    } reach_scroll_view_window;

    void reach_scroll_view_init(reach_scroll_view *view);
    int32_t reach_scroll_view_retarget(reach_scroll_view *view, const reach_theme *theme,
                                       float target, reach_scroll_view_mode mode,
                                       double now_seconds);
    float reach_scroll_view_value_at(const reach_scroll_view *view, double now_seconds);
    float reach_scroll_view_target(const reach_scroll_view *view);
    reach_motion reach_scroll_view_content_offset(const reach_scroll_view *view);
    int32_t reach_scroll_view_active(const reach_scroll_view *view);
    double reach_scroll_view_deadline(const reach_scroll_view *view);
    void reach_scroll_view_settle(reach_scroll_view *view);
    reach_scroll_view_window reach_scroll_view_item_window(const reach_scroll_view *view,
                                                           float pitch, size_t item_count,
                                                           size_t visible_count, size_t overscan,
                                                           size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
