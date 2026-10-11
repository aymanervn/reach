#include "reach/support/motion.h"

#include <chrono>
#include <math.h>

double reach_motion_now_seconds(void)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void reach_motion_set(reach_motion *motion, float value)
{
    if (motion != nullptr)
    {
        uint64_t generation = motion->generation + 1;
        *motion = {value, value, 0.0, 0.0, REACH_EASING_EASE_IN_OUT, generation};
    }
}

void reach_motion_start(reach_motion *motion, float from, float to, double duration_seconds,
                        reach_easing easing)
{
    if (motion == nullptr)
    {
        return;
    }
    if (duration_seconds <= 0.0 || from == to)
    {
        reach_motion_set(motion, to);
        return;
    }
    uint64_t generation = motion->generation + 1;
    *motion = {from, to, reach_motion_now_seconds(), duration_seconds, easing, generation};
}

void reach_motion_animate_to(reach_motion *motion, float to, double duration_seconds,
                             reach_easing easing)
{
    if (motion != nullptr)
    {
        reach_motion_start(motion, reach_motion_value(motion), to, duration_seconds, easing);
    }
}

float reach_motion_value_at(const reach_motion *motion, double now_seconds)
{
    if (motion == nullptr)
    {
        return 0.0f;
    }
    if (motion->duration_seconds <= 0.0)
    {
        return motion->to;
    }
    float progress = (float)((now_seconds - motion->started_seconds) / motion->duration_seconds);
    return motion->from +
           (motion->to - motion->from) * reach_easing_apply(progress, motion->easing);
}

float reach_motion_value(const reach_motion *motion)
{
    return reach_motion_value_at(motion, reach_motion_now_seconds());
}

int32_t reach_motion_active(const reach_motion *motion)
{
    return motion != nullptr && motion->duration_seconds > 0.0 &&
           reach_motion_now_seconds() < reach_motion_deadline(motion);
}

double reach_motion_deadline(const reach_motion *motion)
{
    return motion != nullptr && motion->duration_seconds > 0.0
               ? motion->started_seconds + motion->duration_seconds
               : 0.0;
}

uint32_t reach_motion_wait_ms(double deadline_seconds)
{
    if (deadline_seconds <= 0.0)
    {
        return UINT32_MAX;
    }
    double remaining = ceil((deadline_seconds - reach_motion_now_seconds()) * 1000.0);
    return remaining <= 0.0 ? 0 : remaining >= UINT32_MAX ? UINT32_MAX - 1 : (uint32_t)remaining;
}
