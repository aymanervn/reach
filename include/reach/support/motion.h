#ifndef REACH_SUPPORT_MOTION_H
#define REACH_SUPPORT_MOTION_H

#include <stddef.h>
#include <stdint.h>

#include "reach/support/easing.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_motion
    {
        float from;
        float to;
        double started_seconds;
        double duration_seconds;
        reach_easing easing;
        uint64_t generation;
        int32_t has_limits;
        float minimum;
        float maximum;
    } reach_motion;

    enum
    {
        REACH_MOTION_MAX_SEGMENTS = 4
    };

    typedef struct reach_motion_segment
    {
        double start;
        double end;
        float value;
        float end_value;
        float linear;
        float quadratic;
        float cubic;
    } reach_motion_segment;

    int32_t reach_motion_equal(const reach_motion *first, const reach_motion *second);
    double reach_motion_now_seconds(void);
    void reach_motion_set(reach_motion *motion, float value);
    void reach_motion_start(reach_motion *motion, float from, float to, double duration_seconds,
                            reach_easing easing);
    void reach_motion_start_at(reach_motion *motion, float from, float to, double duration_seconds,
                               reach_easing easing, double now_seconds);
    void reach_motion_animate_to(reach_motion *motion, float to, double duration_seconds,
                                 reach_easing easing);
    void reach_motion_animate_to_at(reach_motion *motion, float to, double duration_seconds,
                                    reach_easing easing, double now_seconds);
    reach_motion reach_motion_affine(const reach_motion *motion, float multiplier, float offset);
    reach_motion reach_motion_clamp(const reach_motion *motion, float minimum, float maximum);
    size_t reach_motion_compile_at(const reach_motion *motion, double now_seconds,
                                   reach_motion_segment segments[REACH_MOTION_MAX_SEGMENTS]);
    float reach_motion_value_at(const reach_motion *motion, double now_seconds);
    float reach_motion_value(const reach_motion *motion);
    int32_t reach_motion_active(const reach_motion *motion);
    int32_t reach_motion_active_at(const reach_motion *motion, double now_seconds);
    double reach_motion_time_to_value_at(const reach_motion *motion, float target_value,
                                         double now_seconds);
    double reach_motion_deadline(const reach_motion *motion);
    uint32_t reach_motion_wait_ms(double deadline_seconds);

#ifdef __cplusplus
}
#endif

#endif
