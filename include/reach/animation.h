#ifndef REACH_ANIMATION_H
#define REACH_ANIMATION_H

#include "reach/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum reach_easing {
    REACH_EASING_LINEAR = 0,
    REACH_EASING_EASE_IN_OUT = 1
} reach_easing;

typedef struct reach_float_animation {
    float from;
    float to;
    float value;
    double elapsed_seconds;
    double duration_seconds;
    reach_easing easing;
} reach_float_animation;

typedef struct reach_vec2_animation {
    reach_vec2 from;
    reach_vec2 to;
    reach_vec2 value;
    double elapsed_seconds;
    double duration_seconds;
    reach_easing easing;
} reach_vec2_animation;

float reach_lerp_float(float a, float b, float t);
reach_vec2 reach_lerp_vec2(reach_vec2 a, reach_vec2 b, float t);
float reach_ease_in_out(float t);
void reach_float_animation_start(reach_float_animation *animation, float from, float to, double duration_seconds);
void reach_vec2_animation_start(reach_vec2_animation *animation, reach_vec2 from, reach_vec2 to, double duration_seconds);
void reach_float_animation_update(reach_float_animation *animation, double delta_seconds);
void reach_vec2_animation_update(reach_vec2_animation *animation, double delta_seconds);

#ifdef __cplusplus
}
#endif

#endif
