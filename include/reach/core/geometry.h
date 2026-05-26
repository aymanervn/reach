#ifndef REACH_CORE_GEOMETRY_H
#define REACH_CORE_GEOMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_color {
    float r;
    float g;
    float b;
    float a;
} reach_color;

typedef struct reach_rect_f32 {
    float x;
    float y;
    float width;
    float height;
} reach_rect_f32;

#ifdef __cplusplus
}
#endif

#endif
