#ifndef REACH_CORE_ANIMATED_RECT_H
#define REACH_CORE_ANIMATED_RECT_H

#include "reach/core/geometry.h"
#include "reach/support/motion.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_rect_motion
    {
        reach_motion x;
        reach_motion y;
        reach_motion width;
        reach_motion height;
    } reach_rect_motion;

    static inline reach_rect_f32 reach_rect_motion_value_at(const reach_rect_motion *rect,
                                                            double now_seconds)
    {
        reach_rect_f32 result = {0};
        if (rect != 0)
        {
            result.x = reach_motion_value_at(&rect->x, now_seconds);
            result.y = reach_motion_value_at(&rect->y, now_seconds);
            result.width = reach_motion_value_at(&rect->width, now_seconds);
            result.height = reach_motion_value_at(&rect->height, now_seconds);
        }
        return result;
    }

    static inline reach_rect_motion reach_rect_motion_transform(const reach_rect_motion *rect,
                                                                reach_transform_f32 transform)
    {
        reach_rect_motion result = {0};
        if (rect != 0)
        {
            result.x = reach_motion_affine(&rect->x, transform.scale_x, transform.offset_x);
            result.y = reach_motion_affine(&rect->y, transform.scale_y, transform.offset_y);
            result.width = reach_motion_affine(&rect->width, transform.scale_x, 0.0f);
            result.height = reach_motion_affine(&rect->height, transform.scale_y, 0.0f);
        }
        return result;
    }

#ifdef __cplusplus
}
#endif

#endif
