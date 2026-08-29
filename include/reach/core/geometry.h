#ifndef REACH_CORE_GEOMETRY_H
#define REACH_CORE_GEOMETRY_H

#include <math.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_rect_f32
    {
        float x;
        float y;
        float width;
        float height;
    } reach_rect_f32;

    typedef struct reach_point_i32
    {
        int32_t x;
        int32_t y;
    } reach_point_i32;

    typedef struct reach_point_f32
    {
        float x;
        float y;
    } reach_point_f32;

    typedef struct reach_transform_f32
    {
        float scale_x;
        float scale_y;
        float offset_x;
        float offset_y;
    } reach_transform_f32;

    static inline int32_t reach_rect_equal(reach_rect_f32 a, reach_rect_f32 b)
    {
        return fabsf(a.x - b.x) < 0.5f && fabsf(a.y - b.y) < 0.5f &&
               fabsf(a.width - b.width) < 0.5f && fabsf(a.height - b.height) < 0.5f;
    }

#ifdef __cplusplus
}
#endif

#endif
