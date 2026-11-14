#ifndef REACH_CORE_SCROLLBAR_H
#define REACH_CORE_SCROLLBAR_H

#include <stdint.h>

#include "reach/core/geometry.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_scrollbar_model
    {
        float position;
        float maximum;
        float step;
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

    void reach_scrollbar_model_init(reach_scrollbar_model *model, float step);
    void reach_scrollbar_set_extents(reach_scrollbar_model *model, float content_extent,
                                     float viewport_extent);
    void reach_scrollbar_set_position(reach_scrollbar_model *model, float position);
    reach_scrollbar_layout reach_scrollbar_compute_layout(const reach_scrollbar_model *model,
                                                          reach_rect_f32 track, float position,
                                                          float viewport_extent,
                                                          float content_extent,
                                                          float minimum_thumb_extent);
    void reach_scrollbar_begin_drag(reach_scrollbar_model *model, reach_scrollbar_drag *drag,
                                    const reach_scrollbar_layout *layout, float pointer_position,
                                    int32_t on_thumb);
    void reach_scrollbar_update_drag(reach_scrollbar_model *model, const reach_scrollbar_drag *drag,
                                     const reach_scrollbar_layout *layout, float pointer_position);
    void reach_scrollbar_end_drag(reach_scrollbar_drag *drag);

#ifdef __cplusplus
}
#endif

#endif
