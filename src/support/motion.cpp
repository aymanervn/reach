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
    reach_motion_start_at(motion, from, to, duration_seconds, easing, reach_motion_now_seconds());
}

void reach_motion_start_at(reach_motion *motion, float from, float to, double duration_seconds,
                           reach_easing easing, double now_seconds)
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
    *motion = {from, to, now_seconds, duration_seconds, easing, generation};
}

void reach_motion_animate_to(reach_motion *motion, float to, double duration_seconds,
                             reach_easing easing)
{
    reach_motion_animate_to_at(motion, to, duration_seconds, easing, reach_motion_now_seconds());
}

void reach_motion_animate_to_at(reach_motion *motion, float to, double duration_seconds,
                                reach_easing easing, double now_seconds)
{
    if (motion != nullptr)
    {
        reach_motion_start_at(motion, reach_motion_value_at(motion, now_seconds), to,
                              duration_seconds, easing, now_seconds);
    }
}

reach_motion reach_motion_affine(const reach_motion *motion, float multiplier, float offset)
{
    reach_motion result = motion != nullptr ? *motion : reach_motion{};
    result.from = result.from * multiplier + offset;
    result.to = result.to * multiplier + offset;
    if (result.generation == 0 || multiplier == 0.0f)
        reach_motion_set(&result, result.to);
    return result;
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
    return reach_motion_active_at(motion, reach_motion_now_seconds());
}

int32_t reach_motion_active_at(const reach_motion *motion, double now_seconds)
{
    return motion != nullptr && motion->duration_seconds > 0.0 &&
           now_seconds < reach_motion_deadline(motion);
}

double reach_motion_time_to_value_at(const reach_motion *motion, float target_value,
                                     double now_seconds)
{
    if (!reach_motion_active_at(motion, now_seconds) || motion->from == motion->to)
        return 0.0;
    float needed = (target_value - motion->from) / (motion->to - motion->from);
    if (needed <= 0.0f)
        return 0.0;
    if (needed >= 1.0f)
        return fmax(0.0, reach_motion_deadline(motion) - now_seconds);
    double elapsed = now_seconds - motion->started_seconds;
    float lo = fmaxf(0.0f, (float)(elapsed / motion->duration_seconds));
    float hi = 1.0f;
    if (reach_easing_apply(lo, motion->easing) >= needed)
        return 0.0;
    for (int step = 0; step < 32; ++step)
    {
        float middle = (lo + hi) * 0.5f;
        if (reach_easing_apply(middle, motion->easing) < needed)
            lo = middle;
        else
            hi = middle;
    }
    return fmax(0.0, motion->started_seconds + hi * motion->duration_seconds - now_seconds);
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
