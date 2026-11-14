#ifndef REACH_FEATURES_COMMON_SCROLL_VIEW_H
#define REACH_FEATURES_COMMON_SCROLL_VIEW_H

#include "reach/core/geometry.h"
#include "reach/core/render_commands.h"
#include "reach/core/scrollbar.h"
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

    typedef enum reach_scroll_view_result_flags
    {
        REACH_SCROLL_VIEW_HANDLED = 1,
        REACH_SCROLL_VIEW_CHANGED = 2,
        REACH_SCROLL_VIEW_CAPTURE = 4,
        REACH_SCROLL_VIEW_RELEASE = 8
    } reach_scroll_view_result_flags;

    typedef struct reach_scroll_view_config
    {
        float unit;
        float step;
        float wheel_delta;
        float minimum_thumb;
    } reach_scroll_view_config;

    typedef struct reach_scroll_view
    {
        reach_scroll_view_config config;
        reach_scrollbar_model model;
        reach_scrollbar_drag drag;
        reach_motion offset;
        reach_rect_f32 viewport;
        reach_rect_f32 track;
        float content_extent;
        float viewport_extent;
    } reach_scroll_view;

    typedef struct reach_scroll_view_window
    {
        size_t first;
        size_t count;
    } reach_scroll_view_window;

    typedef struct reach_scroll_view_group
    {
        uint64_t id;
        uint64_t parent_id;
        reach_rect_f32 bounds;
        const reach_motion *opacity;
    } reach_scroll_view_group;

    typedef struct reach_scroll_view_colors
    {
        reach_color track;
        reach_color thumb;
    } reach_scroll_view_colors;

    void reach_scroll_view_init(reach_scroll_view *view, const reach_scroll_view_config *config);
    void reach_scroll_view_configure(reach_scroll_view *view,
                                     const reach_scroll_view_config *config);
    void reach_scroll_view_set_extents(reach_scroll_view *view, float content_extent,
                                       float viewport_extent);
    void reach_scroll_view_set_frame(reach_scroll_view *view, reach_rect_f32 viewport,
                                     reach_rect_f32 track);
    int32_t reach_scroll_view_set_position(reach_scroll_view *view, float position);
    float reach_scroll_view_position(const reach_scroll_view *view);
    int32_t reach_scroll_view_sync(reach_scroll_view *view, const reach_theme *theme,
                                   reach_scroll_view_mode mode, double now_seconds);

    int32_t reach_scroll_view_scroll_by(reach_scroll_view *view, float delta, double now_seconds);
    uint32_t reach_scroll_view_wheel(reach_scroll_view *view, float x, float y, int32_t wheel_delta,
                                     double now_seconds);
    int32_t reach_scroll_view_hit(const reach_scroll_view *view, float x, float y);
    uint32_t reach_scroll_view_pointer_down(reach_scroll_view *view, float x, float y,
                                            double now_seconds);
    uint32_t reach_scroll_view_pointer_move(reach_scroll_view *view, float y, double now_seconds);
    uint32_t reach_scroll_view_pointer_up(reach_scroll_view *view);
    int32_t reach_scroll_view_dragging(const reach_scroll_view *view);

    float reach_scroll_view_value_at(const reach_scroll_view *view, double now_seconds);
    reach_motion reach_scroll_view_content_offset(const reach_scroll_view *view);
    int32_t reach_scroll_view_active(const reach_scroll_view *view);
    int32_t reach_scroll_view_pending(const reach_scroll_view *view);
    int32_t reach_scroll_view_advance(reach_scroll_view *view, double now_seconds);
    double reach_scroll_view_deadline(const reach_scroll_view *view);
    void reach_scroll_view_settle(reach_scroll_view *view);
    reach_scroll_view_window reach_scroll_view_item_window(const reach_scroll_view *view,
                                                           float pitch, size_t item_count,
                                                           size_t visible_count, size_t overscan,
                                                           size_t capacity);

    int32_t reach_scroll_view_has_scrollbar(const reach_scroll_view *view);
    reach_rect_f32 reach_scroll_view_thumb_at(const reach_scroll_view *view, double now_seconds);
    reach_result reach_scroll_view_push_viewport(const reach_scroll_view *view,
                                                 const reach_scroll_view_group *group,
                                                 reach_render_command_buffer *out);
    reach_result reach_scroll_view_push_scrollbar(const reach_scroll_view *view,
                                                  reach_rect_f32 origin,
                                                  reach_scroll_view_colors colors,
                                                  const reach_scroll_view_group *retained,
                                                  double now_seconds,
                                                  reach_render_command_buffer *out);

#ifdef __cplusplus
}
#endif

#endif
