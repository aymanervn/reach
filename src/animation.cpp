#include "reach/animation.h"

float reach_lerp_float(float a, float b, float t)
{
    return a + (b - a) * t;
}

reach_vec2 reach_lerp_vec2(reach_vec2 a, reach_vec2 b, float t)
{
    reach_vec2 value = { reach_lerp_float(a.x, b.x, t), reach_lerp_float(a.y, b.y, t) };
    return value;
}

float reach_ease_in_out(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

void reach_float_animation_start(reach_float_animation *animation, float from, float to, double duration_seconds)
{
    REACH_ASSERT(animation != nullptr);
    if (animation != nullptr) {
        *animation = { from, to, from, 0.0, duration_seconds, REACH_EASING_EASE_IN_OUT };
    }
}

void reach_vec2_animation_start(reach_vec2_animation *animation, reach_vec2 from, reach_vec2 to, double duration_seconds)
{
    REACH_ASSERT(animation != nullptr);
    if (animation != nullptr) {
        *animation = { from, to, from, 0.0, duration_seconds, REACH_EASING_EASE_IN_OUT };
    }
}

void reach_float_animation_update(reach_float_animation *animation, double delta_seconds)
{
    REACH_ASSERT(animation != nullptr);
    if (animation == nullptr || animation->duration_seconds <= 0.0) {
        return;
    }
    animation->elapsed_seconds += delta_seconds;
    float t = (float)(animation->elapsed_seconds / animation->duration_seconds);
    if (t > 1.0f) {
        t = 1.0f;
    }
    animation->value = reach_lerp_float(animation->from, animation->to, reach_ease_in_out(t));
}

void reach_vec2_animation_update(reach_vec2_animation *animation, double delta_seconds)
{
    REACH_ASSERT(animation != nullptr);
    if (animation == nullptr || animation->duration_seconds <= 0.0) {
        return;
    }
    animation->elapsed_seconds += delta_seconds;
    float t = (float)(animation->elapsed_seconds / animation->duration_seconds);
    if (t > 1.0f) {
        t = 1.0f;
    }
    animation->value = reach_lerp_vec2(animation->from, animation->to, reach_ease_in_out(t));
}
