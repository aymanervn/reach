#include "reach/support/motion.h"

#include <math.h>
#include <initializer_list>

static bool reach_motion_mapping_valid(const reach_motion *motion)
{
    if (!motion->has_mapping)
        return motion->mapping_extra_count == 0;
    if (!isfinite(motion->mapping_center) || !isfinite(motion->mapping_value) ||
        !isfinite(motion->mapping_left_slope) || !isfinite(motion->mapping_right_slope) ||
        motion->mapping_extra_count < 0 ||
        motion->mapping_extra_count > REACH_MOTION_MAX_EXTRA_KNOTS)
        return false;
    float previous = motion->mapping_center;
    for (int32_t at = 0; at < motion->mapping_extra_count; ++at)
    {
        float center = motion->mapping_extra_centers[at];
        if (!isfinite(center) || !isfinite(motion->mapping_extra_deltas[at]) || center <= previous)
            return false;
        previous = center;
    }
    return true;
}

static float reach_motion_limit(const reach_motion *motion, float value)
{
    if (motion->has_mapping)
    {
        float raw = value;
        value = motion->mapping_value +
                (raw - motion->mapping_center) * (raw < motion->mapping_center
                                                      ? motion->mapping_left_slope
                                                      : motion->mapping_right_slope);
        for (int32_t at = 0; at < motion->mapping_extra_count && at < REACH_MOTION_MAX_EXTRA_KNOTS;
             ++at)
            value += fmaxf(0, raw - motion->mapping_extra_centers[at]) *
                     motion->mapping_extra_deltas[at];
    }
    return motion->has_limits ? fminf(motion->maximum, fmaxf(motion->minimum, value)) : value;
}

int32_t reach_motion_equal(const reach_motion *first, const reach_motion *second)
{
    if (first == second)
        return 1;
    if (first == nullptr || second == nullptr || first->mapping_extra_count < 0 ||
        first->mapping_extra_count > REACH_MOTION_MAX_EXTRA_KNOTS ||
        first->mapping_extra_count != second->mapping_extra_count)
        return 0;
    for (int32_t at = 0; at < first->mapping_extra_count; ++at)
        if (first->mapping_extra_centers[at] != second->mapping_extra_centers[at] ||
            first->mapping_extra_deltas[at] != second->mapping_extra_deltas[at])
            return 0;
    return first != nullptr && second != nullptr && first->from == second->from &&
           first->to == second->to && first->started_seconds == second->started_seconds &&
           first->duration_seconds == second->duration_seconds && first->easing == second->easing &&
           first->generation == second->generation && first->has_limits == second->has_limits &&
           first->minimum == second->minimum && first->maximum == second->maximum &&
           first->repeating == second->repeating &&
           first->hold_start_seconds == second->hold_start_seconds &&
           first->hold_end_seconds == second->hold_end_seconds &&
           first->has_mapping == second->has_mapping &&
           first->mapping_center == second->mapping_center &&
           first->mapping_left_slope == second->mapping_left_slope &&
           first->mapping_right_slope == second->mapping_right_slope &&
           first->mapping_value == second->mapping_value;
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
    if (result.has_mapping)
    {
        result.mapping_left_slope *= multiplier;
        result.mapping_right_slope *= multiplier;
        result.mapping_value = result.mapping_value * multiplier + offset;
        for (int32_t at = 0; at < result.mapping_extra_count && at < REACH_MOTION_MAX_EXTRA_KNOTS;
             ++at)
            result.mapping_extra_deltas[at] *= multiplier;
    }
    else
    {
        result.from = result.from * multiplier + offset;
        result.to = result.to * multiplier + offset;
    }
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

reach_result reach_motion_peak(const reach_motion *motion, float center, float radius,
                               reach_motion *out_motion)
{
    if (motion == nullptr || out_motion == nullptr || !reach_motion_mapping_valid(motion) ||
        motion->has_mapping || !isfinite(center) || !isfinite(radius) || radius <= 0 ||
        (motion->has_limits &&
         (motion->minimum > center - radius || motion->maximum < center + radius)))
        return REACH_INVALID_ARGUMENT;
    reach_motion result = *motion;
    result.has_mapping = 1;
    result.mapping_center = center;
    result.mapping_left_slope = 1 / radius;
    result.mapping_right_slope = -1 / radius;
    result.mapping_value = 1;
    result.has_limits = 1;
    result.minimum = 0;
    result.maximum = 1;
    if (result.generation == 0)
        reach_motion_set(&result, reach_motion_limit(&result, result.to));
    *out_motion = result;
    return REACH_OK;
}

reach_result reach_motion_range(const reach_motion *motion, float *out_minimum, float *out_maximum)
{
    if (motion == nullptr || out_minimum == nullptr || out_maximum == nullptr ||
        !isfinite(motion->from) || !isfinite(motion->to) ||
        (motion->has_limits && (!isfinite(motion->minimum) || !isfinite(motion->maximum) ||
                                motion->minimum > motion->maximum)) ||
        !reach_motion_mapping_valid(motion))
        return REACH_INVALID_ARGUMENT;
    float first = reach_motion_limit(motion, motion->from);
    float last = reach_motion_limit(motion, motion->to);
    if (!isfinite(first) || !isfinite(last))
        return REACH_INVALID_ARGUMENT;
    float minimum = fminf(first, last), maximum = fmaxf(first, last);
    if (motion->repeating && motion->duration_seconds == 0)
    {
        *out_minimum = minimum;
        *out_maximum = maximum;
        return REACH_OK;
    }
    if (motion->has_mapping && motion->mapping_center > fminf(motion->from, motion->to) &&
        motion->mapping_center < fmaxf(motion->from, motion->to))
    {
        float middle = reach_motion_limit(motion, motion->mapping_center);
        if (!isfinite(middle))
            return REACH_INVALID_ARGUMENT;
        minimum = fminf(minimum, middle);
        maximum = fmaxf(maximum, middle);
    }
    if (!isfinite(minimum) || !isfinite(maximum))
        return REACH_INVALID_ARGUMENT;
    for (int32_t at = 0; at < motion->mapping_extra_count; ++at)
    {
        float center = motion->mapping_extra_centers[at];
        if (center <= fminf(motion->from, motion->to) || center >= fmaxf(motion->from, motion->to))
            continue;
        float value = reach_motion_limit(motion, center);
        if (!isfinite(value))
            return REACH_INVALID_ARGUMENT;
        minimum = fminf(minimum, value);
        maximum = fmaxf(maximum, value);
    }
    *out_minimum = minimum;
    *out_maximum = maximum;
    return REACH_OK;
}

static bool reach_motion_remove_inactive_limits(reach_motion *motion)
{
    if (!motion->has_limits)
        return true;
    reach_motion unbounded = *motion;
    unbounded.has_limits = 0;
    float minimum = 0, maximum = 0;
    if (reach_motion_range(&unbounded, &minimum, &maximum) != REACH_OK)
        return false;
    if (minimum >= motion->minimum && maximum <= motion->maximum)
    {
        motion->has_limits = 0;
        return true;
    }
    return false;
}

reach_result reach_motion_hinge(const reach_motion *motion, float center, float value,
                                float left_slope, float right_slope, reach_motion *out_motion)
{
    if (motion == nullptr || out_motion == nullptr || !reach_motion_mapping_valid(motion) ||
        motion->has_mapping || !isfinite(center) || !isfinite(value) || !isfinite(left_slope) ||
        !isfinite(right_slope))
        return REACH_INVALID_ARGUMENT;
    reach_motion result = *motion;
    if (!reach_motion_remove_inactive_limits(&result))
        return REACH_INVALID_ARGUMENT;
    result.has_mapping = 1;
    result.mapping_center = center;
    result.mapping_left_slope = left_slope;
    result.mapping_right_slope = right_slope;
    result.mapping_value = value;
    float minimum = 0, maximum = 0;
    if (reach_motion_range(&result, &minimum, &maximum) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    if ((!result.repeating && result.duration_seconds <= 0) || result.from == result.to ||
        result.generation == 0)
        reach_motion_set(&result, reach_motion_limit(&result, result.to));
    *out_motion = result;
    return REACH_OK;
}

static bool reach_motion_same_epoch(const reach_motion &a, const reach_motion &b)
{
    return a.started_seconds == b.started_seconds && a.duration_seconds == b.duration_seconds &&
           a.easing == b.easing && a.generation == b.generation && a.repeating == b.repeating &&
           a.hold_start_seconds == b.hold_start_seconds && a.hold_end_seconds == b.hold_end_seconds;
}

static void reach_motion_line(const reach_motion &motion, double raw, double &slope, double &offset)
{
    slope = 1;
    offset = 0;
    if (motion.has_mapping)
    {
        slope =
            raw < motion.mapping_center ? motion.mapping_left_slope : motion.mapping_right_slope;
        offset = motion.mapping_value - slope * motion.mapping_center;
        for (int32_t at = 0; at < motion.mapping_extra_count; ++at)
            if (raw >= motion.mapping_extra_centers[at])
            {
                slope += motion.mapping_extra_deltas[at];
                offset -=
                    (double)motion.mapping_extra_deltas[at] * motion.mapping_extra_centers[at];
            }
    }
    double value = slope * raw + offset;
    if (motion.has_limits && (value <= motion.minimum || value >= motion.maximum))
    {
        offset = fmin(motion.maximum, fmax(motion.minimum, value));
        slope = 0;
    }
}

struct reach_motion_boundaries
{
    float values[64];
    size_t count;

    bool add(double value, float low, float high)
    {
        float point = (float)value;
        if (!isfinite(point) || point <= low || point >= high)
            return true;
        for (size_t at = 0; at < count; ++at)
            if (values[at] == point)
                return true;
        if (count == 64)
            return false;
        size_t at = count++;
        while (at > 0 && values[at - 1] > point)
        {
            values[at] = values[at - 1];
            --at;
        }
        values[at] = point;
        return true;
    }
};

static bool reach_motion_raw_boundaries(const reach_motion &motion, reach_motion_boundaries &points)
{
    float low = fminf(motion.from, motion.to), high = fmaxf(motion.from, motion.to);
    if (motion.has_mapping)
    {
        if (!points.add(motion.mapping_center, low, high))
            return false;
        for (int32_t at = 0; at < motion.mapping_extra_count; ++at)
            if (!points.add(motion.mapping_extra_centers[at], low, high))
                return false;
    }
    if (!motion.has_limits)
        return true;
    reach_motion unbounded = motion;
    unbounded.has_limits = 0;
    reach_motion_boundaries original = points;
    for (size_t at = 0; at <= original.count; ++at)
    {
        float begin = at == 0 ? low : original.values[at - 1];
        float end = at == original.count ? high : original.values[at];
        double slope = 0, offset = 0;
        reach_motion_line(unbounded, ((double)begin + end) * 0.5, slope, offset);
        if (slope == 0)
            continue;
        for (float limit : {motion.minimum, motion.maximum})
        {
            double root = (limit - offset) / slope;
            if (root > begin && root < end && !points.add(root, low, high))
                return false;
        }
    }
    return true;
}

static reach_result reach_motion_compose(const reach_motion &a, float a_factor,
                                         const reach_motion &b, float b_factor, float offset,
                                         int selection, reach_motion *out_motion)
{
    if (!reach_motion_same_epoch(a, b) || a.from == a.to || b.from == b.to)
        return REACH_INVALID_ARGUMENT;
    float low = fminf(a.from, a.to), high = fmaxf(a.from, a.to);
    double ratio = ((double)b.to - b.from) / ((double)a.to - a.from);
    double shift = b.from - ratio * a.from;
    reach_motion_boundaries a_points = {}, b_points = {}, points = {};
    if (!reach_motion_raw_boundaries(a, a_points) || !reach_motion_raw_boundaries(b, b_points))
        return REACH_INVALID_ARGUMENT;
    for (size_t at = 0; at < a_points.count; ++at)
        if (!points.add(a_points.values[at], low, high))
            return REACH_INVALID_ARGUMENT;
    for (size_t at = 0; at < b_points.count; ++at)
        if (!points.add((b_points.values[at] - shift) / ratio, low, high))
            return REACH_INVALID_ARGUMENT;
    auto lines =
        [&](double raw, double &a_slope, double &a_offset, double &b_slope, double &b_offset)
    {
        reach_motion_line(a, raw, a_slope, a_offset);
        reach_motion_line(b, raw * ratio + shift, b_slope, b_offset);
        b_offset += b_slope * shift;
        b_slope *= ratio;
    };
    if (selection != 0)
    {
        reach_motion_boundaries original = points;
        for (size_t at = 0; at <= original.count; ++at)
        {
            float begin = at == 0 ? low : original.values[at - 1];
            float end = at == original.count ? high : original.values[at];
            double sa = 0, oa = 0, sb = 0, ob = 0;
            lines(((double)begin + end) * 0.5, sa, oa, sb, ob);
            if (sa != sb)
            {
                double root = (ob - oa) / (sa - sb);
                if (root > begin && root < end && !points.add(root, low, high))
                    return REACH_INVALID_ARGUMENT;
            }
        }
    }
    reach_motion result = a;
    result.has_limits = 0;
    result.minimum = result.maximum = 0;
    result.has_mapping = 0;
    result.mapping_extra_count = 0;
    auto output = [&](double raw)
    {
        float av = reach_motion_limit(&a, (float)raw);
        float bv = reach_motion_limit(&b, (float)(raw * ratio + shift));
        return selection < 0   ? fminf(av, bv)
               : selection > 0 ? fmaxf(av, bv)
                               : a_factor * av + b_factor * bv + offset;
    };
    float previous_slope = 0;
    for (size_t at = 0; at <= points.count; ++at)
    {
        float begin = at == 0 ? low : points.values[at - 1];
        float end = at == points.count ? high : points.values[at];
        double middle = ((double)begin + end) * 0.5;
        double sa = 0, oa = 0, sb = 0, ob = 0;
        lines(middle, sa, oa, sb, ob);
        float slope =
            (float)(selection == 0 ? a_factor * sa + b_factor * sb
                                   : ((selection < 0 ? sa * middle + oa <= sb * middle + ob
                                                     : sa * middle + oa >= sb * middle + ob)
                                          ? sa
                                          : sb));
        if (!isfinite(slope))
            return REACH_INVALID_ARGUMENT;
        if (at > 0 && slope != previous_slope)
        {
            if (!result.has_mapping)
            {
                result.has_mapping = 1;
                result.mapping_center = begin;
                result.mapping_value = output(begin);
                result.mapping_left_slope = previous_slope;
                result.mapping_right_slope = slope;
            }
            else
            {
                if (result.mapping_extra_count == REACH_MOTION_MAX_EXTRA_KNOTS)
                    return REACH_INVALID_ARGUMENT;
                int32_t index = result.mapping_extra_count++;
                result.mapping_extra_centers[index] = begin;
                result.mapping_extra_deltas[index] = slope - previous_slope;
            }
        }
        previous_slope = slope;
    }
    if (!result.has_mapping)
    {
        result.from = output(a.from);
        result.to = output(a.to);
        result.mapping_center = result.mapping_value = 0;
        result.mapping_left_slope = result.mapping_right_slope = 0;
    }
    float minimum = 0, maximum = 0;
    if (reach_motion_range(&result, &minimum, &maximum) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    *out_motion = result;
    return REACH_OK;
}

static reach_result reach_motion_select(const reach_motion *first, const reach_motion *second,
                                        int selection, reach_motion *out_motion)
{
    if (first == nullptr || second == nullptr || out_motion == nullptr)
        return REACH_INVALID_ARGUMENT;
    float a_min = 0, a_max = 0, b_min = 0, b_max = 0;
    if (reach_motion_range(first, &a_min, &a_max) != REACH_OK ||
        reach_motion_range(second, &b_min, &b_max) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    if (a_min == a_max)
    {
        *out_motion = reach_motion_clamp(second, selection < 0 ? fminf(a_min, b_min) : a_min,
                                         selection < 0 ? a_min : fmaxf(a_min, b_max));
        return REACH_OK;
    }
    if (b_min == b_max)
        return reach_motion_select(second, first, selection, out_motion);
    return reach_motion_compose(*first, 1, *second, 1, 0, selection, out_motion);
}

reach_result reach_motion_min(const reach_motion *first, const reach_motion *second,
                              reach_motion *out_motion)
{
    return reach_motion_select(first, second, -1, out_motion);
}

reach_result reach_motion_max(const reach_motion *first, const reach_motion *second,
                              reach_motion *out_motion)
{
    return reach_motion_select(first, second, 1, out_motion);
}

reach_result reach_motion_combine(const reach_motion *first, float first_factor,
                                  const reach_motion *second, float second_factor, float offset,
                                  reach_motion *out_motion)
{
    if (first == nullptr || second == nullptr || out_motion == nullptr || !isfinite(first_factor) ||
        !isfinite(second_factor) || !isfinite(offset))
        return REACH_INVALID_ARGUMENT;
    reach_motion a = *first, b = *second;
    float a_min = 0, a_max = 0, b_min = 0, b_max = 0;
    if (reach_motion_range(&a, &a_min, &a_max) != REACH_OK ||
        reach_motion_range(&b, &b_min, &b_max) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    if (a_min == a_max || first_factor == 0)
    {
        reach_motion result = reach_motion_affine(&b, second_factor, offset + first_factor * a_min);
        if (reach_motion_range(&result, &a_min, &a_max) != REACH_OK)
            return REACH_INVALID_ARGUMENT;
        *out_motion = result;
        return REACH_OK;
    }
    if (b_min == b_max || second_factor == 0)
    {
        reach_motion result = reach_motion_affine(&a, first_factor, offset + second_factor * b_min);
        if (reach_motion_range(&result, &a_min, &a_max) != REACH_OK)
            return REACH_INVALID_ARGUMENT;
        *out_motion = result;
        return REACH_OK;
    }
    if (!reach_motion_same_epoch(a, b))
        return REACH_INVALID_ARGUMENT;
    if (!reach_motion_remove_inactive_limits(&a) || !reach_motion_remove_inactive_limits(&b) ||
        a.mapping_extra_count != 0 || b.mapping_extra_count != 0)
        return reach_motion_compose(*first, first_factor, *second, second_factor, offset, 0,
                                    out_motion);
    if (!a.has_mapping && !b.has_mapping)
    {
        reach_motion result = a;
        result.from = first_factor * a.from + second_factor * b.from + offset;
        result.to = first_factor * a.to + second_factor * b.to + offset;
        if (reach_motion_range(&result, &a_min, &a_max) != REACH_OK)
            return REACH_INVALID_ARGUMENT;
        *out_motion = result;
        return REACH_OK;
    }
    if (!a.has_mapping)
        return reach_motion_combine(&b, second_factor, &a, first_factor, offset, out_motion);
    if (b.has_mapping && (a.from != b.from || a.to != b.to || a.mapping_center != b.mapping_center))
        return reach_motion_compose(*first, first_factor, *second, second_factor, offset, 0,
                                    out_motion);
    float slope = (b.to - b.from) / (a.to - a.from);
    float b_value = b.has_mapping ? b.mapping_value : b.from + slope * (a.mapping_center - a.from);
    reach_motion result = a;
    result.mapping_left_slope = first_factor * a.mapping_left_slope +
                                second_factor * (b.has_mapping ? b.mapping_left_slope : slope);
    result.mapping_right_slope = first_factor * a.mapping_right_slope +
                                 second_factor * (b.has_mapping ? b.mapping_right_slope : slope);
    result.mapping_value = first_factor * a.mapping_value + second_factor * b_value + offset;
    if (reach_motion_range(&result, &a_min, &a_max) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    *out_motion = result;
    return REACH_OK;
}

static bool reach_motion_travels(const reach_motion *motion)
{
    float from = reach_motion_limit(motion, motion->from);
    float to = reach_motion_limit(motion, motion->to);
    if (from != to)
        return true;
    if (!motion->has_mapping || (motion->repeating && motion->duration_seconds == 0))
        return false;
    if (motion->mapping_center > fminf(motion->from, motion->to) &&
        motion->mapping_center < fmaxf(motion->from, motion->to) &&
        reach_motion_limit(motion, motion->mapping_center) != from)
        return true;
    for (int32_t at = 0; at < motion->mapping_extra_count && at < REACH_MOTION_MAX_EXTRA_KNOTS;
         ++at)
        if (motion->mapping_extra_centers[at] > fminf(motion->from, motion->to) &&
            motion->mapping_extra_centers[at] < fmaxf(motion->from, motion->to) &&
            reach_motion_limit(motion, motion->mapping_extra_centers[at]) != from)
            return true;
    return false;
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
        !isfinite(motion->from) || !isfinite(motion->to) || !reach_motion_mapping_valid(motion) ||
        (motion->has_limits && (!isfinite(motion->minimum) || !isfinite(motion->maximum) ||
                                motion->minimum > motion->maximum)))
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
    auto add_boundary = [&](double value)
    {
        if (span == 0)
            return;
        double progress = (value - motion->from) / span;
        if (progress > 0 && progress < 1)
        {
            double at = reach_motion_crossing_progress(progress, motion->easing) * duration;
            if (at > elapsed && at < duration)
                boundaries[boundary_count++] = at;
        }
    };
    reach_motion_boundaries raw_boundaries = {};
    if (!reach_motion_raw_boundaries(*motion, raw_boundaries))
        return 0;
    for (size_t at = 0; at < raw_boundaries.count; ++at)
        add_boundary(raw_boundaries.values[at]);
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
        double multiplier = 1;
        double offset = 0;
        reach_motion_line(*motion, middle_value, multiplier, offset);
        middle_value = middle_value * multiplier + offset;
        if (motion->has_limits &&
            (middle_value <= motion->minimum || middle_value >= motion->maximum))
            segment.value = fminf(motion->maximum, fmaxf(motion->minimum, (float)middle_value));
        else
        {
            segment.value = reach_motion_limit(
                motion, (float)(motion->from + span * (a + b * t + c * t * t + d * t * t * t)));
            segment.linear =
                (float)(multiplier * span * (b + 2.0 * c * t + 3.0 * d * t * t) / duration);
            segment.quadratic =
                (float)(multiplier * span * (c + 3.0 * d * t) / (duration * duration));
            segment.cubic = (float)(multiplier * span * d / (duration * duration * duration));
        }
        segments[count++] = segment;
    }
    return count;
}

void reach_motion_repeat_at(reach_motion *motion, float from, float to, double travel_seconds,
                            double hold_start_seconds, double hold_end_seconds, reach_easing easing,
                            double now_seconds)
{
    if (motion == nullptr)
        return;
    double cycle = travel_seconds * 2 + hold_start_seconds + hold_end_seconds;
    if (!isfinite(travel_seconds) || travel_seconds < 0 || !isfinite(hold_start_seconds) ||
        hold_start_seconds < 0 || !isfinite(hold_end_seconds) || hold_end_seconds < 0 ||
        !isfinite(cycle) || cycle <= 0 || !isfinite(now_seconds) || !isfinite(from) ||
        !isfinite(to))
    {
        reach_motion_set(motion, to);
        return;
    }
    if (from == to || (travel_seconds == 0 && (hold_start_seconds == 0 || hold_end_seconds == 0)))
    {
        reach_motion_set(motion, hold_start_seconds == 0 ? to : from);
        return;
    }
    uint64_t generation = motion->generation + 1;
    *motion = {from, to, now_seconds, travel_seconds, easing, generation};
    motion->repeating = 1;
    motion->hold_start_seconds = hold_start_seconds;
    motion->hold_end_seconds = hold_end_seconds;
}

double reach_motion_cycle_seconds(const reach_motion *motion)
{
    if (motion == nullptr || !motion->repeating || !isfinite(motion->duration_seconds) ||
        motion->duration_seconds < 0 || !isfinite(motion->hold_start_seconds) ||
        motion->hold_start_seconds < 0 || !isfinite(motion->hold_end_seconds) ||
        motion->hold_end_seconds < 0 ||
        (motion->duration_seconds == 0 &&
         (motion->hold_start_seconds == 0 || motion->hold_end_seconds == 0)))
        return 0;
    double cycle =
        motion->duration_seconds * 2 + motion->hold_start_seconds + motion->hold_end_seconds;
    return isfinite(cycle) ? cycle : 0;
}

static double reach_motion_repeat_phase(const reach_motion *motion, double now_seconds,
                                        double cycle)
{
    double elapsed = fmax(0.0, now_seconds - motion->started_seconds);
    double phase = fmod(elapsed, cycle);
    if (motion->duration_seconds == 0)
    {
        double beginning = motion->started_seconds + floor(elapsed / cycle) * cycle;
        if (now_seconds >= beginning + cycle)
            return 0;
        if (now_seconds == beginning + motion->hold_start_seconds)
            return motion->hold_start_seconds;
    }
    return phase;
}

size_t
reach_motion_compile_repeat_at(const reach_motion *motion, double now_seconds,
                               reach_motion_segment segments[REACH_MOTION_MAX_REPEAT_SEGMENTS])
{
    double cycle = reach_motion_cycle_seconds(motion);
    if (cycle <= 0 || segments == nullptr || !isfinite(now_seconds) ||
        !isfinite(motion->started_seconds) || !isfinite(motion->from) || !isfinite(motion->to) ||
        !reach_motion_mapping_valid(motion) ||
        (motion->has_limits && (!isfinite(motion->minimum) || !isfinite(motion->maximum) ||
                                motion->minimum > motion->maximum)) ||
        !reach_motion_travels(motion))
        return 0;
    double phase = reach_motion_repeat_phase(motion, now_seconds, cycle);
    double end = phase + cycle;
    if (!isfinite(end))
        return 0;
    size_t count = 0;
    for (int iteration = 0; iteration < 2; ++iteration)
    {
        double base = iteration * cycle;
        double points[] = {base, base + motion->hold_start_seconds,
                           base + motion->hold_start_seconds + motion->duration_seconds,
                           base + motion->hold_start_seconds + motion->duration_seconds +
                               motion->hold_end_seconds,
                           base + cycle};
        for (size_t interval = 0; interval < 4; ++interval)
        {
            double left = fmax(phase, points[interval]);
            double right = fmin(end, points[interval + 1]);
            if (left >= right)
                continue;
            if (interval == 0 || interval == 2)
            {
                if (count == REACH_MOTION_MAX_REPEAT_SEGMENTS)
                    return 0;
                float value = reach_motion_limit(motion, interval == 0 ? motion->from : motion->to);
                segments[count++] = {left - phase, right - phase, value, value, 0, 0, 0};
                continue;
            }
            reach_motion leg = *motion;
            leg.repeating = 0;
            leg.started_seconds = points[interval];
            if (interval == 3)
            {
                leg.from = motion->to;
                leg.to = motion->from;
            }
            reach_motion_segment pieces[REACH_MOTION_MAX_SEGMENTS] = {};
            size_t piece_count = reach_motion_compile_at(&leg, left, pieces);
            for (size_t index = 0; index < piece_count; ++index)
            {
                reach_motion_segment piece = pieces[index];
                double piece_left = left + piece.start;
                double piece_right = fmin(right, left + piece.end);
                if (piece_left >= piece_right)
                    continue;
                if (count == REACH_MOTION_MAX_REPEAT_SEGMENTS)
                    return 0;
                piece.start = piece_left - phase;
                piece.end = piece_right - phase;
                piece.end_value = reach_motion_value_at(&leg, piece_right);
                segments[count++] = piece;
            }
        }
    }
    if (motion->duration_seconds == 0)
        for (size_t index = 0; index < count; ++index)
            segments[index].end_value = segments[(index + 1) % count].value;
    return count;
}

float reach_motion_value_at(const reach_motion *motion, double now_seconds)
{
    if (motion == nullptr)
    {
        return 0.0f;
    }
    if (motion->repeating)
    {
        double cycle = reach_motion_cycle_seconds(motion);
        if (cycle <= 0 || now_seconds <= motion->started_seconds)
            return reach_motion_limit(motion, motion->from);
        double phase = reach_motion_repeat_phase(motion, now_seconds, cycle);
        if (motion->duration_seconds == 0)
            return reach_motion_limit(motion, phase < motion->hold_start_seconds ? motion->from
                                                                                 : motion->to);
        if (phase <= motion->hold_start_seconds)
            return reach_motion_limit(motion, motion->from);
        phase -= motion->hold_start_seconds;
        if (phase <= motion->duration_seconds)
            return reach_motion_limit(
                motion,
                motion->from + (motion->to - motion->from) *
                                   reach_easing_apply((float)(phase / motion->duration_seconds),
                                                      motion->easing));
        phase -= motion->duration_seconds;
        if (phase <= motion->hold_end_seconds)
            return reach_motion_limit(motion, motion->to);
        phase -= motion->hold_end_seconds;
        return reach_motion_limit(
            motion, motion->to + (motion->from - motion->to) *
                                     reach_easing_apply((float)(phase / motion->duration_seconds),
                                                        motion->easing));
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
    if (motion != nullptr && motion->repeating)
        return reach_motion_cycle_seconds(motion) > 0 && reach_motion_travels(motion);
    return motion != nullptr && motion->duration_seconds > 0.0 &&
           now_seconds < reach_motion_deadline(motion);
}

double reach_motion_time_to_value_at(const reach_motion *motion, float target_value,
                                     double now_seconds)
{
    if (!reach_motion_active_at(motion, now_seconds) || motion->from == motion->to)
        return 0.0;
    if (motion->repeating && motion->duration_seconds == 0)
    {
        if (reach_motion_value_at(motion, now_seconds) == target_value)
            return 0;
        float from = reach_motion_limit(motion, motion->from);
        float to = reach_motion_limit(motion, motion->to);
        if (target_value != from && target_value != to)
            return INFINITY;
        double cycle = reach_motion_cycle_seconds(motion);
        double phase = reach_motion_repeat_phase(motion, now_seconds, cycle);
        double boundary = target_value == to ? motion->hold_start_seconds : cycle;
        if (boundary < phase)
            boundary += cycle;
        return fmax(0.0, motion->started_seconds - now_seconds) + boundary - phase;
    }
    if (motion->has_mapping)
    {
        if (reach_motion_value_at(motion, now_seconds) == target_value)
            return 0;
        reach_motion_segment pieces[REACH_MOTION_MAX_REPEAT_SEGMENTS] = {};
        size_t count = motion->repeating
                           ? reach_motion_compile_repeat_at(motion, now_seconds, pieces)
                           : reach_motion_compile_at(motion, now_seconds, pieces);
        for (size_t index = 0; index < count; ++index)
        {
            const auto &piece = pieces[index];
            if (target_value < fminf(piece.value, piece.end_value) ||
                target_value > fmaxf(piece.value, piece.end_value))
                continue;
            double low = 0, high = piece.end - piece.start;
            for (int step = 0; step < 48; ++step)
            {
                double t = (low + high) * 0.5;
                double value = piece.value + piece.linear * t + piece.quadratic * t * t +
                               piece.cubic * t * t * t;
                if (piece.end_value > piece.value ? value < target_value : value > target_value)
                    low = t;
                else
                    high = t;
            }
            return fmax(0.0, motion->started_seconds - now_seconds) + piece.start + high;
        }
        return motion->repeating ? INFINITY
                                 : fmax(0.0, reach_motion_deadline(motion) - now_seconds);
    }
    if (motion->repeating)
    {
        float current = reach_motion_value_at(motion, now_seconds);
        if (current == target_value)
            return 0;
        float low =
            fminf(reach_motion_limit(motion, motion->from), reach_motion_limit(motion, motion->to));
        float high =
            fmaxf(reach_motion_limit(motion, motion->from), reach_motion_limit(motion, motion->to));
        if (target_value < low || target_value > high)
            return INFINITY;
        double cycle = reach_motion_cycle_seconds(motion);
        double phase = reach_motion_repeat_phase(motion, now_seconds, cycle);
        double progress = ((double)target_value - motion->from) / (motion->to - motion->from);
        double forward =
            motion->hold_start_seconds +
            reach_motion_crossing_progress(progress, motion->easing) * motion->duration_seconds;
        double backward =
            motion->hold_start_seconds + motion->duration_seconds + motion->hold_end_seconds +
            reach_motion_crossing_progress(1 - progress, motion->easing) * motion->duration_seconds;
        if (forward < phase)
            forward += cycle;
        if (backward < phase)
            backward += cycle;
        return fmax(0.0, motion->started_seconds - now_seconds) + fmin(forward, backward) - phase;
    }
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
    return motion != nullptr && !motion->repeating && motion->duration_seconds > 0.0
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
