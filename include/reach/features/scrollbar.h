#ifndef REACH_FEATURES_SCROLLBAR_H
#define REACH_FEATURES_SCROLLBAR_H

#include <stdint.h>

#include "reach/core/geometry.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_scrollbar_drag_mode
    {
        REACH_SCROLLBAR_DRAG_FREE = 0,
        REACH_SCROLLBAR_DRAG_STEPPED = 1
    } reach_scrollbar_drag_mode;

    typedef struct reach_scrollbar_model
    {
        float offset;
        float target;
        float maximum;
        float step;
        reach_scrollbar_drag_mode drag_mode;
    } reach_scrollbar_model;

    typedef struct reach_scrollbar_layout
    {
        reach_rect_f32 track;
        reach_rect_f32 thumb;
    } reach_scrollbar_layout;

    typedef struct reach_scrollbar_drag
    {
        int32_t active;
        float grab_offset;
    } reach_scrollbar_drag;

    void reach_scrollbar_model_init(reach_scrollbar_model *model,
                                    reach_scrollbar_drag_mode drag_mode, float step);
    void reach_scrollbar_set_extents(reach_scrollbar_model *model, float content_extent,
                                     float viewport_extent);
    void reach_scrollbar_set_target(reach_scrollbar_model *model, float target);
    void reach_scrollbar_scroll(reach_scrollbar_model *model, float delta);
    int32_t reach_scrollbar_update(reach_scrollbar_model *model, double delta_seconds);
    reach_scrollbar_layout reach_scrollbar_compute_layout(const reach_scrollbar_model *model,
                                                          reach_rect_f32 track,
                                                          float viewport_extent,
                                                          float content_extent,
                                                          float minimum_thumb_extent);
    void reach_scrollbar_begin_drag(reach_scrollbar_model *model, reach_scrollbar_drag *drag,
                                    const reach_scrollbar_layout *layout, float pointer_position,
                                    int32_t on_thumb);
    void reach_scrollbar_update_drag(reach_scrollbar_model *model,
                                     const reach_scrollbar_drag *drag,
                                     const reach_scrollbar_layout *layout,
                                     float pointer_position);
    void reach_scrollbar_end_drag(reach_scrollbar_drag *drag);

#ifdef __cplusplus
}
#endif

#endif
