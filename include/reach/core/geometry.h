#ifndef REACH_CORE_GEOMETRY_H
#define REACH_CORE_GEOMETRY_H

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

#ifdef __cplusplus
}
#endif

#endif
