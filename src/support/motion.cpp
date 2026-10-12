#include "reach/support/motion.h"

#include <math.h>

static float reach_motion_limit(const reach_motion *motion, float value)
{
    return motion->has_limits ? fminf(motion->maximum, fmaxf(motion->minimum, value)) : value;
}

int32_t reach_motion_equal(const reach_motion *first, const reach_motion *second)
{
    if (first == second)
        return 1;
    return first != nullptr && second != nullptr && first->from == second->from &&
           first->to == second->to && first->started_seconds == second->started_seconds &&
           first->duration_seconds == second->duration_seconds && first->easing == second->easing &&
           first->generation == second->generation && first->has_limits == second->has_limits &&
           first->minimum == second->minimum && first->maximum == second->maximum;
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
    if (result.has_limits)
    {
        float minimum = result.minimum * multiplier + offset;
        float maximum = result.maximum * multiplier + offset;
        result.minimum = fminf(minimum, maximum);
        result.maximum = fmaxf(minimum, maximum);
    }
    if (result.generation == 0 || multiplier == 0.0f)
        reach_motion_set(&result, reach_motion_limit(&result, result.to));
    return result;
}

reach_motion reach_motion_clamp(const reach_motion *motion, float minimum, float maximum)
{
    reach_motion result = motion != nullptr ? *motion : reach_motion{};
    if (!isfinite(minimum) || !isfinite(maximum) || minimum > maximum)
        return result;
    result.minimum = result.has_limits ? fminf(maximum, fmaxf(minimum, result.minimum)) : minimum;
    result.maximum = result.has_limits ? fminf(maximum, fmaxf(minimum, result.maximum)) : maximum;
    result.has_limits = 1;
    if (result.generation == 0)
        reach_motion_set(&result, reach_motion_limit(&result, result.to));
    return result;
}

static double reach_motion_crossing_progress(double progress, reach_easing easing)
{
    if (easing == REACH_EASING_EASE_IN)
        return cbrt(progress);
    if (easing == REACH_EASING_EASE_OUT)
        return 1.0 - cbrt(1.0 - progress);
    return progress < 0.5 ? cbrt(progress / 4.0) : 1.0 - cbrt((1.0 - progress) / 4.0);
}

size_t reach_motion_compile_at(const reach_motion *motion, double now_seconds,
                               reach_motion_segment segments[REACH_MOTION_MAX_SEGMENTS])
{
    if (motion == nullptr || segments == nullptr || !isfinite(now_seconds) ||
        !isfinite(motion->started_seconds) || !isfinite(motion->duration_seconds) ||
        !isfinite(motion->from) || !isfinite(motion->to))
        return 0;
    double elapsed = fmax(0.0, now_seconds - motion->started_seconds);
    double duration = motion->duration_seconds;
    if (duration <= elapsed)
        return 0;
    double boundaries[REACH_MOTION_MAX_SEGMENTS + 1] = {elapsed, duration};
    size_t boundary_count = 2;
    double split = duration * 0.5;
    if (motion->easing != REACH_EASING_EASE_IN && motion->easing != REACH_EASING_EASE_OUT &&
        split > elapsed)
        boundaries[boundary_count++] = split;
    double span = (double)motion->to - motion->from;
    if (motion->has_limits && span != 0.0)
    {
        float limits[2] = {motion->minimum, motion->maximum};
        for (float limit : limits)
        {
            double progress = ((double)limit - motion->from) / span;
            if (progress > 0.0 && progress < 1.0)
            {
                double at = reach_motion_crossing_progress(progress, motion->easing) * duration;
                if (at > elapsed && at < duration)
                    boundaries[boundary_count++] = at;
            }
        }
    }
    for (size_t index = 1; index < boundary_count; ++index)
    {
        double value = boundaries[index];
        size_t position = index;
        while (position > 0 && boundaries[position - 1] > value)
        {
            boundaries[position] = boundaries[position - 1];
            --position;
        }
        boundaries[position] = value;
    }
    size_t count = 0;
    for (size_t index = 1; index < boundary_count; ++index)
    {
        double at = boundaries[index - 1];
        double end = boundaries[index];
        if (end <= at)
            continue;
        double t = at / duration;
        double middle = (at + end) / (2.0 * duration);
        double a = 0.0, b = 0.0, c = 0.0, d = 0.0;
        if (motion->easing == REACH_EASING_EASE_IN)
            d = 1.0;
        else if (motion->easing == REACH_EASING_EASE_OUT)
        {
            b = 3.0;
            c = -3.0;
            d = 1.0;
        }
        else if (middle < 0.5)
            d = 4.0;
        else
        {
            a = -3.0;
            b = 12.0;
            c = -12.0;
            d = 4.0;
        }
        double middle_value = motion->from + span * (a + b * middle + c * middle * middle +
                                                     d * middle * middle * middle);
        reach_motion_segment segment = {};
        segment.start = at - elapsed;
        segment.end = end - elapsed;
        double end_t = end / duration;
        segment.end_value = reach_motion_limit(
            motion, end == duration
                        ? motion->to
                        : (float)(motion->from + span * (a + b * end_t + c * end_t * end_t +
                                                         d * end_t * end_t * end_t)));
        if (motion->has_limits &&
            (middle_value <= motion->minimum || middle_value >= motion->maximum))
            segment.value = reach_motion_limit(motion, (float)middle_value);
        else
        {
            segment.value = reach_motion_limit(
                motion, (float)(motion->from + span * (a + b * t + c * t * t + d * t * t * t)));
            segment.linear = (float)(span * (b + 2.0 * c * t + 3.0 * d * t * t) / duration);
            segment.quadratic = (float)(span * (c + 3.0 * d * t) / (duration * duration));
            segment.cubic = (float)(span * d / (duration * duration * duration));
        }
        segments[count++] = segment;
    }
    return count;
}

float reach_motion_value_at(const reach_motion *motion, double now_seconds)
{
    if (motion == nullptr)
    {
        return 0.0f;
    }
    if (motion->duration_seconds <= 0.0 || now_seconds >= reach_motion_deadline(motion))
    {
        return reach_motion_limit(motion, motion->to);
    }
    if (now_seconds <= motion->started_seconds)
        return reach_motion_limit(motion, motion->from);
    float progress = (float)((now_seconds - motion->started_seconds) / motion->duration_seconds);
    return reach_motion_limit(motion,
                              motion->from + (motion->to - motion->from) *
                                                 reach_easing_apply(progress, motion->easing));
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
    float current = reach_motion_value_at(motion, now_seconds);
    float endpoint = reach_motion_limit(motion, motion->to);
    if (motion->to > motion->from ? target_value <= current : target_value >= current)
        return 0.0;
    if (motion->to > motion->from ? target_value > endpoint : target_value < endpoint)
        return fmax(0.0, reach_motion_deadline(motion) - now_seconds);
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
