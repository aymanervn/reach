#ifndef REACH_SUPPORT_MOTION_H
#define REACH_SUPPORT_MOTION_H

#include "reach/support/animation.h"

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
    } reach_motion;

    double reach_motion_now_seconds(void);
    void reach_motion_set(reach_motion *motion, float value);
    void reach_motion_start(reach_motion *motion, float from, float to, double duration_seconds,
                            reach_easing easing);
    void reach_motion_animate_to(reach_motion *motion, float to, double duration_seconds,
                                 reach_easing easing);
    float reach_motion_value_at(const reach_motion *motion, double now_seconds);
    float reach_motion_value(const reach_motion *motion);
    int32_t reach_motion_active(const reach_motion *motion);
    double reach_motion_deadline(const reach_motion *motion);
    uint32_t reach_motion_wait_ms(double deadline_seconds);

#ifdef __cplusplus
}
#endif

#endif
