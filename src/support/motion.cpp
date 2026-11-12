#include "reach/support/motion.h"

#include <float.h>
#include <math.h>
#include <initializer_list>

static constexpr double reach_motion_pi = 3.14159265358979323846;
static constexpr double reach_spring_settle_ratio = 5e-6;

struct reach_spring
{
    double to;
    double displacement;
    double velocity;
    double omega;
    double decay;
    double frequency;
    double cosine;
    double sine;
    double amplitude;
    double phase;
    double turn;
    bool critical;
};

static bool reach_spring_valid(const reach_motion &motion)
{
    return motion.easing == REACH_EASING_SPRING && !motion.repeating && isfinite(motion.from) &&
           isfinite(motion.to) && isfinite(motion.spring_response) && motion.spring_response > 0 &&
           isfinite(motion.spring_damping) && motion.spring_damping >= .5f &&
           motion.spring_damping <= 1 && isfinite(motion.spring_velocity);
}

static reach_spring reach_spring_shape(double to, double displacement, double velocity,
                                       double response, double damping)
{
    reach_spring spring = {};
    spring.to = to;
    spring.displacement = displacement;
    spring.velocity = velocity;
    spring.omega = 2 * reach_motion_pi / response;
    spring.critical = damping >= 1 - 1e-6;
    spring.decay = spring.critical ? spring.omega : damping * spring.omega;
    spring.frequency = spring.critical ? 0 : spring.omega * sqrt(1 - damping * damping);
    spring.cosine = spring.displacement;
    spring.sine = spring.critical
                      ? spring.velocity + spring.omega * spring.displacement
                      : (spring.velocity + spring.decay * spring.displacement) / spring.frequency;
    if (!spring.critical)
    {
        spring.amplitude = hypot(spring.cosine, spring.sine);
        spring.phase = atan2(-spring.sine, spring.cosine);
        spring.turn = atan2(spring.frequency, -spring.decay);
    }
    return spring;
}

static reach_spring reach_spring_make(const reach_motion &motion)
{
    return reach_spring_shape(motion.to, (double)motion.from - motion.to, motion.spring_velocity,
                              motion.spring_response, motion.spring_damping);
}

static double reach_spring_offset(const reach_spring &spring, double t)
{
    double envelope = exp(-spring.decay * t);
    if (spring.critical)
        return (spring.cosine + spring.sine * t) * envelope;
    return envelope *
           (spring.cosine * cos(spring.frequency * t) + spring.sine * sin(spring.frequency * t));
}

static double reach_spring_rate(const reach_spring &spring, double t)
{
    double envelope = exp(-spring.decay * t);
    if (spring.critical)
        return (spring.velocity - spring.omega * spring.sine * t) * envelope;
    return envelope * (spring.velocity * cos(spring.frequency * t) -
                       (spring.cosine * spring.frequency + spring.decay * spring.sine) *
                           sin(spring.frequency * t));
}

static double reach_spring_bound(const reach_spring &spring, int order, double begin, double end)
{
    double scale = pow(spring.omega, order);
    if (spring.critical)
    {
        double base = spring.cosine - order * spring.sine / spring.omega;
        auto magnitude = [&](double t)
        { return fabs(base + spring.sine * t) * exp(-spring.omega * t); };
        double peak = fmax(magnitude(begin), magnitude(end));
        if (spring.sine != 0)
        {
            double stationary = 1 / spring.omega - base / spring.sine;
            if (stationary > begin && stationary < end)
                peak = fmax(peak, magnitude(stationary));
        }
        return scale * peak;
    }
    double first = spring.frequency * begin + spring.phase + order * spring.turn;
    double last = spring.frequency * end + spring.phase + order * spring.turn;
    double peak = 1;
    if (last - first < reach_motion_pi && ceil(first / reach_motion_pi) * reach_motion_pi > last)
        peak = fmax(fabs(cos(first)), fabs(cos(last)));
    return scale * spring.amplitude * exp(-spring.decay * begin) * peak;
}

template <typename Visit>
static bool reach_spring_each_turn(const reach_spring &spring, double begin, double end,
                                   Visit &&visit)
{
    if (spring.critical)
    {
        if (spring.sine != 0)
        {
            double turn = 1 / spring.omega - spring.cosine / spring.sine;
            if (turn > begin && turn < end)
                return visit(turn);
        }
        return true;
    }
    double offset = spring.phase + spring.turn - reach_motion_pi / 2;
    double index = floor((spring.frequency * begin + offset) / reach_motion_pi);
    for (int visited = 0; visited < 4096; ++visited, index += 1)
    {
        double turn = (index * reach_motion_pi - offset) / spring.frequency;
        if (turn <= begin)
            continue;
        if (turn >= end)
            return true;
        if (!visit(turn))
            return false;
    }
    return false;
}

static double reach_spring_settle(const reach_motion &motion)
{
    reach_spring spring = reach_spring_make(motion);
    double amplitude = fabs(spring.displacement) + fabs(spring.velocity) / spring.omega;
    if (!(amplitude > 0) || !isfinite(amplitude))
        return 0;
    auto envelope = [&](double t) { return exp(-spring.decay * t) * (1 + spring.omega * t); };
    double start = (spring.omega - spring.decay) / (spring.decay * spring.omega);
    double finish = start + 1 / spring.omega;
    for (int step = 0; step < 128 && envelope(finish) > reach_spring_settle_ratio; ++step)
        finish = start + 2 * (finish - start);
    if (!isfinite(finish) || envelope(finish) > reach_spring_settle_ratio)
        return 0;
    for (int step = 0; step < 96; ++step)
    {
        double middle = (start + finish) * .5;
        if (envelope(middle) > reach_spring_settle_ratio)
            start = middle;
        else
            finish = middle;
    }
    if (!spring.critical)
        finish = fmin(finish, log(spring.omega / (spring.frequency * reach_spring_settle_ratio)) /
                                  spring.decay);
    return finish;
}

static bool reach_motion_raw_domain(const reach_motion &motion, float &low, float &high)
{
    low = fminf(motion.from, motion.to);
    high = fmaxf(motion.from, motion.to);
    if (motion.easing != REACH_EASING_SPRING || !(motion.duration_seconds > 0))
        return true;
    if (!reach_spring_valid(motion) || !isfinite(motion.duration_seconds))
        return false;
    reach_spring spring = reach_spring_make(motion);
    auto include = [&](double t)
    {
        float raw = (float)(spring.to + reach_spring_offset(spring, t));
        low = fminf(low, raw);
        high = fmaxf(high, raw);
        return true;
    };
    include(motion.duration_seconds);
    return reach_spring_each_turn(spring, 0, motion.duration_seconds, include) && isfinite(low) &&
           isfinite(high);
}

static bool reach_motion_moves(const reach_motion &motion)
{
    return motion.from != motion.to ||
           (motion.easing == REACH_EASING_SPRING && motion.spring_velocity != 0);
}

static bool reach_motion_mapping_valid(const reach_motion *motion)
{
    if (motion->easing == REACH_EASING_SPRING && !reach_spring_valid(*motion))
        return false;
    if (!motion->has_mapping)
        return motion->mapping_extra_count == 0;
    if (motion->has_mapping != 1 && motion->has_mapping != REACH_MOTION_MAPPING_STEPS &&
        motion->has_mapping != REACH_MOTION_MAPPING_RATIO &&
        motion->has_mapping != REACH_MOTION_MAPPING_QUADRATIC)
        return false;
    if (motion->has_mapping == REACH_MOTION_MAPPING_QUADRATIC)
        return !motion->repeating && motion->mapping_extra_count == 0 &&
               motion->mapping_center == 0 && isfinite(motion->mapping_value) &&
               isfinite(motion->mapping_left_slope) && isfinite(motion->mapping_right_slope);
    if (motion->has_mapping == REACH_MOTION_MAPPING_RATIO)
    {
        float low = 0, high = 0;
        return !motion->repeating && motion->mapping_extra_count == 0 &&
               isfinite(motion->mapping_value) && isfinite(motion->mapping_left_slope) &&
               isfinite(motion->mapping_center) && isfinite(motion->mapping_right_slope) &&
               reach_motion_raw_domain(*motion, low, high) &&
               motion->mapping_center + motion->mapping_right_slope * motion->from > 0 &&
               motion->mapping_center + motion->mapping_right_slope * motion->to > 0 &&
               motion->mapping_center + motion->mapping_right_slope * low > 0 &&
               motion->mapping_center + motion->mapping_right_slope * high > 0;
    }
    if (motion->has_mapping == REACH_MOTION_MAPPING_STEPS &&
        (motion->repeating || motion->mapping_left_slope != 0))
        return false;
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
    if (motion->has_mapping == REACH_MOTION_MAPPING_QUADRATIC)
        value = (float)((double)motion->mapping_value +
                        value * ((double)motion->mapping_left_slope +
                                 (double)motion->mapping_right_slope * value));
    else if (motion->has_mapping == REACH_MOTION_MAPPING_RATIO)
        value =
            (float)(((double)motion->mapping_value + motion->mapping_left_slope * (double)value) /
                    ((double)motion->mapping_center + motion->mapping_right_slope * (double)value));
    else if (motion->has_mapping == REACH_MOTION_MAPPING_STEPS)
    {
        float raw = value;
        auto passed = [&](float threshold)
        { return raw > threshold || (raw == threshold && motion->to >= motion->from); };
        value = motion->mapping_value;
        if (passed(motion->mapping_center))
            value += motion->mapping_right_slope;
        for (int32_t at = 0; at < motion->mapping_extra_count && at < REACH_MOTION_MAX_EXTRA_KNOTS;
             ++at)
            if (passed(motion->mapping_extra_centers[at]))
                value += motion->mapping_extra_deltas[at];
    }
    else if (motion->has_mapping)
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
           first->mapping_value == second->mapping_value &&
           first->spring_response == second->spring_response &&
           first->spring_damping == second->spring_damping &&
           first->spring_velocity == second->spring_velocity;
}

void reach_motion_set(reach_motion *motion, float value)
{
    if (motion != nullptr)
    {
        uint64_t generation = motion->generation + 1;
        *motion = {value, value, 0.0, 0.0, REACH_EASING_EASE_IN_OUT, generation};
    }
}

void reach_motion_start_at(reach_motion *motion, float from, float to, double duration_seconds,
                           reach_easing easing, double now_seconds)
{
    if (motion == nullptr)
    {
        return;
    }
    if (duration_seconds <= 0.0 || from == to || easing == REACH_EASING_SPRING)
    {
        reach_motion_set(motion, to);
        return;
    }
    uint64_t generation = motion->generation + 1;
    *motion = {from, to, now_seconds, duration_seconds, easing, generation};
}

void reach_motion_spring_start_at(reach_motion *motion, float from, float to, float velocity,
                                  float response_seconds, float damping, double now_seconds)
{
    if (motion == nullptr)
        return;
    reach_motion spring = {from, to, now_seconds, 0.0, REACH_EASING_SPRING, motion->generation + 1};
    spring.spring_response = response_seconds;
    spring.spring_damping = damping;
    spring.spring_velocity = velocity;
    if (!isfinite(now_seconds) || !reach_spring_valid(spring) || !reach_motion_moves(spring))
    {
        reach_motion_set(motion, to);
        return;
    }
    spring.duration_seconds = reach_spring_settle(spring);
    if (!(spring.duration_seconds > 0) || !isfinite(spring.duration_seconds))
    {
        reach_motion_set(motion, to);
        return;
    }
    *motion = spring;
}

void reach_motion_spring_to_at(reach_motion *motion, float to, float response_seconds,
                               float damping, double now_seconds)
{
    if (motion != nullptr)
        reach_motion_spring_start_at(motion, reach_motion_value_at(motion, now_seconds), to,
                                     reach_motion_velocity_at(motion, now_seconds),
                                     response_seconds, damping, now_seconds);
}

reach_motion_spec reach_motion_spec_curve(double duration_seconds, reach_easing easing)
{
    reach_motion_spec spec = {};
    spec.easing = easing;
    spec.duration_seconds = duration_seconds;
    return spec;
}

reach_motion_spec reach_motion_spec_spring(float response_seconds, float damping)
{
    reach_motion_spec spec = {};
    spec.easing = REACH_EASING_SPRING;
    spec.response_seconds = response_seconds;
    spec.damping = damping;
    return spec;
}

reach_motion_spec reach_motion_spec_scaled(reach_motion_spec spec, float time_scale)
{
    if (!(time_scale > 0) || !isfinite(time_scale))
        return spec;
    spec.response_seconds *= time_scale;
    spec.duration_seconds *= time_scale;
    return spec;
}

void reach_motion_start_spec_at(reach_motion *motion, float from, float to, float velocity,
                                reach_motion_spec spec, double now_seconds)
{
    if (spec.easing == REACH_EASING_SPRING)
        reach_motion_spring_start_at(motion, from, to, velocity, spec.response_seconds,
                                     spec.damping, now_seconds);
    else
        reach_motion_start_at(motion, from, to, spec.duration_seconds, spec.easing, now_seconds);
}

void reach_motion_animate_spec_to_at(reach_motion *motion, float to, reach_motion_spec spec,
                                     double now_seconds)
{
    if (spec.easing == REACH_EASING_SPRING)
        reach_motion_spring_to_at(motion, to, spec.response_seconds, spec.damping, now_seconds);
    else
        reach_motion_animate_to_at(motion, to, spec.duration_seconds, spec.easing, now_seconds);
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
    if (result.has_mapping == REACH_MOTION_MAPPING_RATIO)
    {
        result.mapping_value = result.mapping_value * multiplier + result.mapping_center * offset;
        result.mapping_left_slope =
            result.mapping_left_slope * multiplier + result.mapping_right_slope * offset;
    }
    else if (result.has_mapping)
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
        result.spring_velocity *= multiplier;
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
    float low = 0, high = 0;
    if (!reach_motion_raw_domain(*motion, low, high))
        return REACH_INVALID_ARGUMENT;
    float first = reach_motion_limit(motion, motion->from);
    float last = reach_motion_limit(motion, motion->to);
    float lowest = reach_motion_limit(motion, low);
    float highest = reach_motion_limit(motion, high);
    if (!isfinite(first) || !isfinite(last) || !isfinite(lowest) || !isfinite(highest))
        return REACH_INVALID_ARGUMENT;
    float minimum = fminf(first, last), maximum = fmaxf(first, last);
    if (motion->repeating && motion->duration_seconds == 0)
    {
        *out_minimum = minimum;
        *out_maximum = maximum;
        return REACH_OK;
    }
    minimum = fminf(minimum, fminf(lowest, highest));
    maximum = fmaxf(maximum, fmaxf(lowest, highest));
    if (motion->has_mapping == REACH_MOTION_MAPPING_QUADRATIC && motion->mapping_right_slope != 0)
    {
        double critical = -(double)motion->mapping_left_slope / (2 * motion->mapping_right_slope);
        if (critical > low && critical < high)
        {
            float value = reach_motion_limit(motion, (float)critical);
            if (!isfinite(value))
                return REACH_INVALID_ARGUMENT;
            minimum = fminf(minimum, value);
            maximum = fmaxf(maximum, value);
        }
    }
    auto include_boundary = [&](float center)
    {
        if (center < low || center > high)
            return true;
        for (float raw : {center, nextafterf(center, -INFINITY), nextafterf(center, INFINITY)})
        {
            if (raw < low || raw > high ||
                (raw != center && motion->has_mapping != REACH_MOTION_MAPPING_STEPS))
                continue;
            float value = reach_motion_limit(motion, raw);
            if (!isfinite(value))
                return false;
            minimum = fminf(minimum, value);
            maximum = fmaxf(maximum, value);
        }
        return true;
    };
    if ((motion->has_mapping == 1 || motion->has_mapping == REACH_MOTION_MAPPING_STEPS) &&
        !include_boundary(motion->mapping_center))
        return REACH_INVALID_ARGUMENT;
    for (int32_t at = 0; at < motion->mapping_extra_count; ++at)
        if (!include_boundary(motion->mapping_extra_centers[at]))
            return REACH_INVALID_ARGUMENT;
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
    if ((!result.repeating && result.duration_seconds <= 0) || !reach_motion_moves(result) ||
        result.generation == 0)
        reach_motion_set(&result, reach_motion_limit(&result, result.to));
    *out_motion = result;
    return REACH_OK;
}

static double reach_motion_ulp(float value)
{
    return ldexp(1.0, ilogb(fmaxf(fabsf(value), FLT_MIN)) - FLT_MANT_DIG + 1);
}

static double reach_spring_velocity_reach(const reach_motion &motion)
{
    return 1 / (exp(1.0) * reach_spring_make(motion).decay);
}

static bool reach_motion_same_epoch(const reach_motion &a, const reach_motion &b,
                                    bool normalized = false)
{
    if (!(a.started_seconds == b.started_seconds && a.duration_seconds == b.duration_seconds &&
          a.easing == b.easing && a.generation == b.generation && a.repeating == b.repeating &&
          a.hold_start_seconds == b.hold_start_seconds && a.hold_end_seconds == b.hold_end_seconds))
        return false;
    if (a.easing != REACH_EASING_SPRING)
        return true;
    if (a.spring_response != b.spring_response || a.spring_damping != b.spring_damping)
        return false;
    double a_displacement = (double)a.from - a.to, b_displacement = (double)b.from - b.to;
    double first = a_displacement * b.spring_velocity;
    double second = b_displacement * a.spring_velocity;
    double mismatch = fabs(first - second);
    if (mismatch == 0)
        return true;
    double omega = 2 * reach_motion_pi / a.spring_response;
    double a_position = reach_motion_ulp(a.from) + reach_motion_ulp(a.to);
    double b_position = reach_motion_ulp(b.from) + reach_motion_ulp(b.to);
    double rounding = a_position * (fabs(b.spring_velocity) + omega * fabs(b_displacement)) +
                      b_position * (fabs(a.spring_velocity) + omega * fabs(a_displacement)) +
                      reach_motion_ulp(a.spring_velocity) * fabs(b_displacement) +
                      reach_motion_ulp(b.spring_velocity) * fabs(a_displacement);
    if (mismatch > 1e-5 * (fabs(first) + fabs(second)) + 4 * rounding)
        return false;
    double drift = a_displacement != 0
                       ? mismatch * reach_spring_velocity_reach(a) / fabs(a_displacement)
                       : INFINITY;
    if (!normalized && a.spring_velocity != 0)
        drift = fmin(drift, mismatch / fabs(a.spring_velocity));
    double b_range =
        fabs(b_displacement) + fabs(b.spring_velocity) * reach_spring_velocity_reach(b);
    return drift <= fmax(fmin(.01, 1e-3 * b_range), 4 * fmax(a_position, b_position));
}

static bool reach_motion_raw_relation(const reach_motion &a, const reach_motion &b, double &ratio,
                                      double &shift)
{
    double a_span = (double)a.to - a.from;
    bool by_velocity = a.easing == REACH_EASING_SPRING &&
                       fabs(a.spring_velocity) * reach_spring_velocity_reach(a) > fabs(a_span);
    ratio = by_velocity   ? (double)b.spring_velocity / a.spring_velocity
            : a_span != 0 ? ((double)b.to - b.from) / a_span
                          : 0;
    shift = b.from - ratio * a.from;
    return ratio != 0 && isfinite(ratio) && isfinite(shift);
}

static bool reach_motion_normalize(reach_motion &result, const reach_motion &source)
{
    double span = (double)source.to - source.from;
    if (source.easing == REACH_EASING_SPRING)
    {
        if (span == 0)
            return false;
        result.spring_velocity = (float)(source.spring_velocity / span);
    }
    result.from = 0;
    result.to = 1;
    return true;
}

reach_result reach_motion_multiply(const reach_motion *first, const reach_motion *second,
                                   reach_motion *out_motion)
{
    if (first == nullptr || second == nullptr || out_motion == nullptr)
        return REACH_INVALID_ARGUMENT;
    reach_motion a = *first, b = *second;
    float a_min = 0, a_max = 0, b_min = 0, b_max = 0;
    if (reach_motion_range(&a, &a_min, &a_max) != REACH_OK ||
        reach_motion_range(&b, &b_min, &b_max) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    reach_motion result = {};
    if (a_min == a_max || b_min == b_max)
        result =
            a_min == a_max ? reach_motion_affine(&b, a_min, 0) : reach_motion_affine(&a, b_min, 0);
    else
    {
        if (!reach_motion_same_epoch(a, b, true) || a.repeating || a.has_mapping || b.has_mapping ||
            !isfinite(a.started_seconds) || !isfinite(a.duration_seconds) ||
            a.duration_seconds < 0 || !reach_motion_remove_inactive_limits(&a) ||
            !reach_motion_remove_inactive_limits(&b))
            return REACH_INVALID_ARGUMENT;
        result = a;
        if (!reach_motion_normalize(result, a) ||
            (b.easing == REACH_EASING_SPRING && b.from == b.to))
            return REACH_INVALID_ARGUMENT;
        result.has_mapping = REACH_MOTION_MAPPING_QUADRATIC;
        result.mapping_center = 0;
        result.mapping_value = (float)((double)a.from * b.from);
        result.mapping_left_slope =
            (float)((double)a.from * ((double)b.to - b.from) + b.from * ((double)a.to - a.from));
        result.mapping_right_slope = (float)(((double)a.to - a.from) * ((double)b.to - b.from));
    }
    if (reach_motion_range(&result, &a_min, &a_max) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    *out_motion = result;
    return REACH_OK;
}

static size_t reach_motion_quadratic_roots(double a, double b, double c, double roots[2])
{
    if (a == 0)
    {
        if (b == 0)
            return 0;
        roots[0] = -c / b;
        return 1;
    }
    double discriminant = b * b - 4 * a * c;
    if (discriminant < 0)
        return 0;
    double q = -.5 * (b + copysign(sqrt(discriminant), b));
    if (q == 0)
    {
        roots[0] = -b / (2 * a);
        return 1;
    }
    roots[0] = q / a;
    roots[1] = c / q;
    return discriminant == 0 ? 1 : 2;
}

static bool reach_motion_polynomial(reach_motion motion, double coefficients[3])
{
    if ((motion.has_mapping && motion.has_mapping != REACH_MOTION_MAPPING_QUADRATIC) ||
        (motion.easing == REACH_EASING_SPRING && motion.from == motion.to) ||
        !reach_motion_remove_inactive_limits(&motion))
        return false;
    double from = motion.from, span = (double)motion.to - from;
    if (!motion.has_mapping)
    {
        coefficients[0] = from;
        coefficients[1] = span;
        coefficients[2] = 0;
    }
    else
    {
        coefficients[0] = motion.mapping_value +
                          from * (motion.mapping_left_slope + motion.mapping_right_slope * from);
        coefficients[1] =
            span * (motion.mapping_left_slope + 2 * motion.mapping_right_slope * from);
        coefficients[2] = motion.mapping_right_slope * span * span;
    }
    return true;
}

reach_result reach_motion_ratio(const reach_motion *numerator, const reach_motion *denominator,
                                reach_motion *out_motion)
{
    if (numerator == nullptr || denominator == nullptr || out_motion == nullptr ||
        numerator->has_mapping || denominator->has_mapping || numerator->repeating ||
        denominator->repeating || !isfinite(numerator->started_seconds) ||
        !isfinite(denominator->started_seconds) || !isfinite(numerator->duration_seconds) ||
        !isfinite(denominator->duration_seconds) || numerator->duration_seconds < 0 ||
        denominator->duration_seconds < 0)
        return REACH_INVALID_ARGUMENT;
    reach_motion a = *numerator, b = *denominator;
    float a_min = 0, a_max = 0, b_min = 0, b_max = 0;
    if (!reach_motion_remove_inactive_limits(&a) || !reach_motion_remove_inactive_limits(&b) ||
        a.has_limits || b.has_limits || reach_motion_range(&a, &a_min, &a_max) != REACH_OK ||
        reach_motion_range(&b, &b_min, &b_max) != REACH_OK || b_min <= 0)
        return REACH_INVALID_ARGUMENT;
    if (b_min == b_max)
    {
        reach_motion result = reach_motion_affine(&a, 1 / b_min, 0);
        if (reach_motion_range(&result, &a_min, &a_max) != REACH_OK)
            return REACH_INVALID_ARGUMENT;
        *out_motion = result;
        return REACH_OK;
    }
    if (a_min != a_max && !reach_motion_same_epoch(b, a, true))
        return REACH_INVALID_ARGUMENT;
    reach_motion result = b;
    if (!reach_motion_normalize(result, b) ||
        (a_min != a_max && a.easing == REACH_EASING_SPRING && a.from == a.to))
        return REACH_INVALID_ARGUMENT;
    result.has_mapping = REACH_MOTION_MAPPING_RATIO;
    result.mapping_value = a_min == a_max ? a_min : a.from;
    result.mapping_left_slope = a_min == a_max ? 0 : a.to - a.from;
    result.mapping_center = b.from;
    result.mapping_right_slope = b.to - b.from;
    result.mapping_extra_count = 0;
    if (reach_motion_range(&result, &a_min, &a_max) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    if (a_min == a_max)
        reach_motion_set(&result, a_min);
    *out_motion = result;
    return REACH_OK;
}

static void reach_motion_line(const reach_motion &motion, double raw, double &slope, double &offset)
{
    slope = 1;
    offset = 0;
    if (motion.has_mapping == REACH_MOTION_MAPPING_STEPS)
    {
        slope = 0;
        offset = reach_motion_limit(&motion, (float)raw);
        return;
    }
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
    float low = 0, high = 0;
    if (!reach_motion_raw_domain(motion, low, high))
        return false;
    if (motion.has_mapping == REACH_MOTION_MAPPING_QUADRATIC)
    {
        if (motion.mapping_right_slope != 0 &&
            !points.add(-(double)motion.mapping_left_slope / (2 * motion.mapping_right_slope), low,
                        high))
            return false;
        if (motion.has_limits)
            for (float limit : {motion.minimum, motion.maximum})
            {
                double roots[2] = {};
                size_t count = reach_motion_quadratic_roots(
                    motion.mapping_right_slope, motion.mapping_left_slope,
                    (double)motion.mapping_value - limit, roots);
                for (size_t at = 0; at < count; ++at)
                    if (!points.add(roots[at], low, high))
                        return false;
            }
        return true;
    }
    if (motion.has_mapping == REACH_MOTION_MAPPING_RATIO)
    {
        if (motion.has_limits)
            for (float limit : {motion.minimum, motion.maximum})
            {
                double divisor = motion.mapping_left_slope - limit * motion.mapping_right_slope;
                if (divisor != 0 &&
                    !points.add((limit * motion.mapping_center - motion.mapping_value) / divisor,
                                low, high))
                    return false;
            }
        return true;
    }
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

reach_result reach_motion_lookup(const reach_motion *motion, const float *thresholds,
                                 const float *values, size_t threshold_count,
                                 reach_motion *out_motion)
{
    float minimum = 0, maximum = 0;
    if (motion == nullptr || values == nullptr || out_motion == nullptr ||
        threshold_count > REACH_MOTION_MAX_EXTRA_KNOTS + 1 ||
        (threshold_count > 0 && thresholds == nullptr) || motion->repeating ||
        !isfinite(motion->started_seconds) || !isfinite(motion->duration_seconds) ||
        motion->duration_seconds < 0 || motion->has_mapping == REACH_MOTION_MAPPING_STEPS ||
        motion->has_mapping == REACH_MOTION_MAPPING_RATIO ||
        motion->has_mapping == REACH_MOTION_MAPPING_QUADRATIC ||
        reach_motion_range(motion, &minimum, &maximum) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    for (size_t index = 0; index <= threshold_count; ++index)
        if (!isfinite(values[index]) ||
            (index < threshold_count &&
             (!isfinite(thresholds[index]) ||
              (index > 0 && thresholds[index] <= thresholds[index - 1]))))
            return REACH_INVALID_ARGUMENT;
    auto lookup = [&](float value)
    {
        size_t index = 0;
        while (index < threshold_count && value >= thresholds[index])
            ++index;
        return values[index];
    };
    reach_motion_boundaries points = {};
    float low = 0, high = 0;
    if (!reach_motion_raw_boundaries(*motion, points) ||
        !reach_motion_raw_domain(*motion, low, high))
        return REACH_INVALID_ARGUMENT;
    for (size_t at = 0; at <= points.count; ++at)
    {
        float begin = at == 0 ? low : points.values[at - 1];
        float end = at == points.count ? high : points.values[at];
        double slope = 0, offset = 0;
        reach_motion_line(*motion, ((double)begin + end) * 0.5, slope, offset);
        if (slope < 0)
            return REACH_INVALID_ARGUMENT;
    }
    reach_motion result = *motion;
    result.has_mapping = 0;
    result.mapping_center = result.mapping_left_slope = result.mapping_right_slope = 0;
    result.mapping_extra_count = 0;
    result.mapping_value = lookup(reach_motion_limit(motion, low));
    result.has_limits = 0;
    result.minimum = result.maximum = 0;
    float previous_value = result.mapping_value;
    for (size_t index = 0; index < threshold_count; ++index)
    {
        if (thresholds[index] <= minimum || thresholds[index] > maximum)
            continue;
        float value = values[index + 1];
        if (value == previous_value)
            continue;
        bool found = false;
        for (size_t at = 0; at <= points.count && !found; ++at)
        {
            float begin = at == 0 ? low : points.values[at - 1];
            float end = at == points.count ? high : points.values[at];
            double slope = 0, offset = 0;
            reach_motion_line(*motion, ((double)begin + end) * 0.5, slope, offset);
            if (slope <= 0)
                continue;
            double root = (thresholds[index] - offset) / slope;
            if (root < begin || root > end)
                continue;
            float point = (float)root;
            if (!result.has_mapping)
            {
                result.has_mapping = REACH_MOTION_MAPPING_STEPS;
                result.mapping_center = point;
                result.mapping_right_slope = value - previous_value;
            }
            else
            {
                float previous_point =
                    result.mapping_extra_count > 0
                        ? result.mapping_extra_centers[result.mapping_extra_count - 1]
                        : result.mapping_center;
                if (point <= previous_point ||
                    result.mapping_extra_count == REACH_MOTION_MAX_EXTRA_KNOTS)
                    return REACH_INVALID_ARGUMENT;
                int32_t extra = result.mapping_extra_count++;
                result.mapping_extra_centers[extra] = point;
                result.mapping_extra_deltas[extra] = value - previous_value;
            }
            previous_value = value;
            found = true;
        }
        if (!found)
            return REACH_INVALID_ARGUMENT;
    }
    if (!result.has_mapping || result.generation == 0 || result.duration_seconds <= 0)
        reach_motion_set(&result, lookup(reach_motion_limit(motion, motion->to)));
    if (reach_motion_range(&result, &minimum, &maximum) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    *out_motion = result;
    return REACH_OK;
}

static reach_result reach_motion_compose(const reach_motion &a, float a_factor,
                                         const reach_motion &b, float b_factor, float offset,
                                         int selection, reach_motion *out_motion)
{
    if (a.has_mapping == REACH_MOTION_MAPPING_QUADRATIC ||
        b.has_mapping == REACH_MOTION_MAPPING_QUADRATIC ||
        a.has_mapping == REACH_MOTION_MAPPING_RATIO ||
        b.has_mapping == REACH_MOTION_MAPPING_RATIO ||
        a.has_mapping == REACH_MOTION_MAPPING_STEPS ||
        b.has_mapping == REACH_MOTION_MAPPING_STEPS || !reach_motion_same_epoch(a, b))
        return REACH_INVALID_ARGUMENT;
    float low = 0, high = 0;
    double ratio = 0, shift = 0;
    if (!reach_motion_raw_relation(a, b, ratio, shift) || !reach_motion_raw_domain(a, low, high))
        return REACH_INVALID_ARGUMENT;
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
        result.spring_velocity = a.spring_velocity * previous_slope;
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
    if (a.has_mapping == REACH_MOTION_MAPPING_QUADRATIC ||
        b.has_mapping == REACH_MOTION_MAPPING_QUADRATIC)
    {
        double ac[3] = {}, bc[3] = {};
        if (!reach_motion_same_epoch(a, b, true) || !reach_motion_polynomial(a, ac) ||
            !reach_motion_polynomial(b, bc))
            return REACH_INVALID_ARGUMENT;
        reach_motion result = a;
        if (!reach_motion_normalize(result, a))
            return REACH_INVALID_ARGUMENT;
        result.has_limits = 0;
        result.has_mapping = REACH_MOTION_MAPPING_QUADRATIC;
        result.mapping_center = 0;
        result.mapping_value = (float)(first_factor * ac[0] + second_factor * bc[0] + offset);
        result.mapping_left_slope = (float)(first_factor * ac[1] + second_factor * bc[1]);
        result.mapping_right_slope = (float)(first_factor * ac[2] + second_factor * bc[2]);
        result.mapping_extra_count = 0;
        if (reach_motion_range(&result, &a_min, &a_max) != REACH_OK)
            return REACH_INVALID_ARGUMENT;
        *out_motion = result;
        return REACH_OK;
    }
    if (a.has_mapping == REACH_MOTION_MAPPING_RATIO ||
        b.has_mapping == REACH_MOTION_MAPPING_RATIO ||
        a.has_mapping == REACH_MOTION_MAPPING_STEPS ||
        b.has_mapping == REACH_MOTION_MAPPING_STEPS || !reach_motion_same_epoch(a, b))
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
        result.spring_velocity =
            first_factor * a.spring_velocity + second_factor * b.spring_velocity;
        if (reach_motion_range(&result, &a_min, &a_max) != REACH_OK)
            return REACH_INVALID_ARGUMENT;
        *out_motion = result;
        return REACH_OK;
    }
    if (!a.has_mapping)
        return reach_motion_combine(&b, second_factor, &a, first_factor, offset, out_motion);
    if (b.has_mapping &&
        (a.from != b.from || a.to != b.to || a.mapping_center != b.mapping_center ||
         a.spring_velocity != b.spring_velocity))
        return reach_motion_compose(*first, first_factor, *second, second_factor, offset, 0,
                                    out_motion);
    double ratio = 0, shift = 0;
    if (!b.has_mapping && !reach_motion_raw_relation(a, b, ratio, shift))
        return REACH_INVALID_ARGUMENT;
    float slope =
        a.easing == REACH_EASING_SPRING ? (float)ratio : (b.to - b.from) / (a.to - a.from);
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
    if (motion->has_mapping == REACH_MOTION_MAPPING_QUADRATIC)
    {
        float minimum = 0, maximum = 0;
        return reach_motion_range(motion, &minimum, &maximum) == REACH_OK && minimum != maximum;
    }
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
    if (easing == REACH_EASING_EXPONENTIAL_OUT)
        return -log1p(-progress * -expm1(-12.0)) / 12.0;
    return progress < 0.5 ? cbrt(progress / 4.0) : 1.0 - cbrt((1.0 - progress) / 4.0);
}

static void reach_motion_mapped_raw(const reach_motion &motion, double t, double *raw,
                                    double derivatives[4])
{
    double value = 0, first = 0, second = 0, third = 0, fourth = 0;
    if (motion.easing == REACH_EASING_EXPONENTIAL_OUT)
    {
        double normalization = -expm1(-12.0), decay = exp(-12 * t) / normalization;
        value = -expm1(-12 * t) / normalization;
        first = 12 * decay;
        second = -144 * decay;
        third = 1728 * decay;
        fourth = -20736 * decay;
    }
    else if (motion.easing == REACH_EASING_EASE_IN ||
             (motion.easing == REACH_EASING_EASE_IN_OUT && t <= .5))
    {
        double factor = motion.easing == REACH_EASING_EASE_IN ? 1 : 4;
        value = factor * t * t * t;
        first = 3 * factor * t * t;
        second = 6 * factor * t;
        third = 6 * factor;
    }
    else
    {
        double factor = motion.easing == REACH_EASING_EASE_OUT ? 1 : 4;
        double inverse = 1 - t;
        value = 1 - factor * inverse * inverse * inverse;
        first = 3 * factor * inverse * inverse;
        second = -6 * factor * inverse;
        third = 6 * factor;
    }
    double span = (double)motion.to - motion.from;
    *raw = motion.from + span * value;
    derivatives[0] = span * first;
    derivatives[1] = span * second;
    derivatives[2] = span * third;
    derivatives[3] = span * fourth;
}

static double reach_motion_ratio_value(const reach_motion &motion, double raw)
{
    return motion.has_mapping == REACH_MOTION_MAPPING_QUADRATIC
               ? motion.mapping_value +
                     raw * (motion.mapping_left_slope + motion.mapping_right_slope * raw)
               : (motion.mapping_value + motion.mapping_left_slope * raw) /
                     (motion.mapping_center + motion.mapping_right_slope * raw);
}

static bool reach_motion_nonlinear_piece(const reach_motion &motion, double begin, double end,
                                         double elapsed, double tolerance,
                                         reach_motion_segment *segments, size_t *count,
                                         size_t depth)
{
    double first_raw = 0, last_raw = 0, middle_raw = 0;
    double first_derivatives[4] = {}, last_derivatives[4] = {}, middle_derivatives[4] = {};
    reach_motion_mapped_raw(motion, begin, &first_raw, first_derivatives);
    reach_motion_mapped_raw(motion, end, &last_raw, last_derivatives);
    reach_motion_mapped_raw(motion, (begin + end) * .5, &middle_raw, middle_derivatives);
    double first = reach_motion_ratio_value(motion, first_raw);
    double last = reach_motion_ratio_value(motion, last_raw);
    double middle = reach_motion_ratio_value(motion, middle_raw);
    bool constant = motion.has_limits && (middle <= motion.minimum || middle >= motion.maximum);
    double first_denominator = motion.mapping_center + motion.mapping_right_slope * first_raw;
    double last_denominator = motion.mapping_center + motion.mapping_right_slope * last_raw;
    double denominator = fmin(first_denominator, last_denominator);
    double determinant = (double)motion.mapping_left_slope * motion.mapping_center -
                         (double)motion.mapping_value * motion.mapping_right_slope;
    double derivatives[4] = {};
    for (size_t index = 0; index < 4; ++index)
        derivatives[index] = fmax(fabs(first_derivatives[index]), fabs(last_derivatives[index]));
    double fourth = 0;
    if (!constant && motion.has_mapping == REACH_MOTION_MAPPING_QUADRATIC)
        fourth = fmax(fabs(motion.mapping_left_slope + 2 * motion.mapping_right_slope * first_raw),
                      fabs(motion.mapping_left_slope + 2 * motion.mapping_right_slope * last_raw)) *
                     derivatives[3] +
                 fabs(motion.mapping_right_slope) *
                     (8 * derivatives[0] * derivatives[2] + 6 * derivatives[1] * derivatives[1]);
    else if (!constant && motion.mapping_right_slope == 0)
        fourth = fabs(motion.mapping_left_slope / motion.mapping_center) * derivatives[3];
    else if (!constant)
    {
        double factor = fabs(motion.mapping_right_slope);
        double d1 = derivatives[0] * factor, d2 = derivatives[1] * factor;
        double d3 = derivatives[2] * factor, d4 = derivatives[3] * factor;
        fourth =
            fabs(determinant / motion.mapping_right_slope) *
            (24 * pow(d1, 4) / pow(denominator, 5) + 36 * d1 * d1 * d2 / pow(denominator, 4) +
             (6 * d2 * d2 + 8 * d1 * d3) / pow(denominator, 3) + d4 / (denominator * denominator));
    }
    if (!isfinite(fourth) || fourth * pow(end - begin, 4) / 384 > tolerance)
    {
        if (depth >= 24 || *count >= REACH_MOTION_MAX_SEGMENTS)
            return false;
        double split = (begin + end) * .5;
        return reach_motion_nonlinear_piece(motion, begin, split, elapsed, tolerance, segments,
                                            count, depth + 1) &&
               reach_motion_nonlinear_piece(motion, split, end, elapsed, tolerance, segments, count,
                                            depth + 1);
    }
    if (*count == REACH_MOTION_MAX_SEGMENTS)
        return false;
    reach_motion_segment segment = {};
    segment.start = begin * motion.duration_seconds - elapsed;
    segment.end = end * motion.duration_seconds - elapsed;
    if (motion.has_limits)
    {
        first = fmin(motion.maximum, fmax(motion.minimum, first));
        last = fmin(motion.maximum, fmax(motion.minimum, last));
    }
    segment.value = (float)first;
    segment.end_value = (float)last;
    if (!constant)
    {
        double dt = segment.end - segment.start;
        double first_derivative =
            motion.has_mapping == REACH_MOTION_MAPPING_QUADRATIC
                ? (motion.mapping_left_slope + 2 * motion.mapping_right_slope * first_raw) *
                      first_derivatives[0] / motion.duration_seconds
                : determinant * first_derivatives[0] /
                      (first_denominator * first_denominator * motion.duration_seconds);
        double last_derivative =
            motion.has_mapping == REACH_MOTION_MAPPING_QUADRATIC
                ? (motion.mapping_left_slope + 2 * motion.mapping_right_slope * last_raw) *
                      last_derivatives[0] / motion.duration_seconds
                : determinant * last_derivatives[0] /
                      (last_denominator * last_denominator * motion.duration_seconds);
        double slope = ((double)segment.end_value - segment.value) / dt;
        segment.linear = (float)first_derivative;
        segment.quadratic = (float)((3 * slope - 2 * first_derivative - last_derivative) / dt);
        segment.cubic = (float)((first_derivative + last_derivative - 2 * slope) / (dt * dt));
    }
    segments[(*count)++] = segment;
    return true;
}

static double reach_motion_map_slope(const reach_motion &motion, double raw, bool forward)
{
    if (motion.has_mapping == REACH_MOTION_MAPPING_QUADRATIC)
        return motion.mapping_left_slope + 2.0 * motion.mapping_right_slope * raw;
    if (motion.has_mapping == REACH_MOTION_MAPPING_RATIO)
    {
        double denominator = motion.mapping_center + motion.mapping_right_slope * raw;
        return ((double)motion.mapping_left_slope * motion.mapping_center -
                (double)motion.mapping_value * motion.mapping_right_slope) /
               (denominator * denominator);
    }
    if (motion.has_mapping == REACH_MOTION_MAPPING_STEPS)
        return 0;
    if (!motion.has_mapping)
        return 1;
    auto beyond = [&](float center) { return forward ? raw >= center : raw > center; };
    double slope =
        beyond(motion.mapping_center) ? motion.mapping_right_slope : motion.mapping_left_slope;
    for (int32_t at = 0; at < motion.mapping_extra_count && at < REACH_MOTION_MAX_EXTRA_KNOTS; ++at)
        if (beyond(motion.mapping_extra_centers[at]))
            slope += motion.mapping_extra_deltas[at];
    return slope;
}

static bool reach_motion_spring_breaks(const reach_motion &motion, const reach_spring &spring,
                                       double begin, double end, double *times, size_t capacity,
                                       size_t *out_count)
{
    if (capacity < 2)
        return false;
    size_t count = 0;
    times[count++] = begin;
    if (!reach_spring_each_turn(spring, begin, end,
                                [&](double turn)
                                {
                                    if (count + 1 >= capacity)
                                        return false;
                                    times[count++] = turn;
                                    return true;
                                }))
        return false;
    times[count++] = end;
    reach_motion_boundaries points = {};
    if (!reach_motion_raw_boundaries(motion, points))
        return false;
    size_t monotone_count = count;
    for (size_t index = 1; index < monotone_count; ++index)
    {
        double left = times[index - 1], right = times[index];
        double first = spring.to + reach_spring_offset(spring, left);
        double last = spring.to + reach_spring_offset(spring, right);
        for (size_t at = 0; at < points.count; ++at)
        {
            double value = points.values[at];
            if (value <= fmin(first, last) || value >= fmax(first, last))
                continue;
            double low = left, high = right;
            for (int step = 0; step < 80; ++step)
            {
                double middle = (low + high) * .5;
                double raw = spring.to + reach_spring_offset(spring, middle);
                if (last > first ? raw < value : raw > value)
                    low = middle;
                else
                    high = middle;
            }
            if (count == capacity)
                return false;
            times[count++] = high;
        }
    }
    for (size_t index = 1; index < count; ++index)
    {
        double value = times[index];
        size_t position = index;
        while (position > 0 && times[position - 1] > value)
        {
            times[position] = times[position - 1];
            --position;
        }
        times[position] = value;
    }
    size_t unique_count = 1;
    for (size_t index = 1; index + 1 < count; ++index)
        if (times[index] > times[unique_count - 1] + 1e-7 && times[index] < end - 1e-7)
            times[unique_count++] = times[index];
    times[unique_count++] = end;
    *out_count = unique_count;
    return end > begin;
}

static double reach_motion_spring_tail_spread(const reach_motion &motion,
                                              const reach_spring &spring, double begin, double end)
{
    if (motion.has_mapping != 0 && motion.has_mapping != 1)
        return INFINITY;
    double envelope = reach_spring_bound(spring, 0, begin, end);
    double low = spring.to - envelope, high = spring.to + envelope;
    float least = INFINITY, most = -INFINITY;
    auto include = [&](double raw)
    {
        float value = reach_motion_limit(&motion, (float)raw);
        least = fminf(least, value);
        most = fmaxf(most, value);
    };
    include(low);
    include(spring.to);
    include(high);
    if (motion.has_mapping)
    {
        if (motion.mapping_center > low && motion.mapping_center < high)
            include(motion.mapping_center);
        for (int32_t at = 0; at < motion.mapping_extra_count && at < REACH_MOTION_MAX_EXTRA_KNOTS;
             ++at)
            if (motion.mapping_extra_centers[at] > low && motion.mapping_extra_centers[at] < high)
                include(motion.mapping_extra_centers[at]);
    }
    double spread = (double)most - least;
    return isfinite(envelope) && isfinite(spread) ? spread : INFINITY;
}

static size_t reach_motion_compile_spring(const reach_motion &motion, double elapsed,
                                          reach_motion_segment segments[REACH_MOTION_MAX_SEGMENTS])
{
    reach_spring spring = reach_spring_make(motion);
    double duration = motion.duration_seconds;
    double times[REACH_MOTION_MAX_SEGMENTS + 1] = {};
    size_t time_count = 0;
    float minimum = 0, maximum = 0;
    if (!reach_motion_spring_breaks(motion, spring, elapsed, duration, times,
                                    REACH_MOTION_MAX_SEGMENTS + 1, &time_count) ||
        reach_motion_range(&motion, &minimum, &maximum) != REACH_OK)
        return 0;
    reach_motion unbounded = motion;
    unbounded.has_limits = 0;
    float final_value = reach_motion_limit(&motion, motion.to);
    auto raw_at = [&](double t) { return spring.to + reach_spring_offset(spring, t); };
    auto output = [&](double t)
    { return t >= duration ? final_value : reach_motion_limit(&motion, (float)raw_at(t)); };
    double gap = fabs((double)reach_motion_limit(&motion, (float)raw_at(duration)) - final_value);
    double tolerance = fmax(1e-6, fmin(.09, 1e-3 * ((double)maximum - minimum)));
    if (!(gap < tolerance))
        return 0;
    double tail = INFINITY;
    for (size_t index = 0; index + 1 < time_count; ++index)
        if (reach_motion_spring_tail_spread(motion, spring, times[index], duration) <=
            fmin(.01, tolerance))
        {
            tail = times[index];
            times[index + 1] = duration;
            time_count = index + 2;
            break;
        }
    bool smooth = motion.has_mapping == REACH_MOTION_MAPPING_QUADRATIC ||
                  motion.has_mapping == REACH_MOTION_MAPPING_RATIO;
    size_t count = 0;
    for (size_t index = 1; index < time_count; ++index)
    {
        double at = times[index - 1], end = times[index];
        double middle_raw = raw_at((at + end) * .5);
        float mapped = reach_motion_limit(&unbounded, (float)middle_raw);
        bool constant =
            motion.has_mapping == REACH_MOTION_MAPPING_STEPS ||
            (motion.has_limits && (mapped <= motion.minimum || mapped >= motion.maximum));
        bool forward = raw_at(end) >= raw_at(at);
        double slope = reach_motion_map_slope(motion, middle_raw, forward);
        auto error = [&](double begin, double finish)
        {
            double b1 = reach_spring_bound(spring, 1, begin, finish);
            double b2 = reach_spring_bound(spring, 2, begin, finish);
            double b3 = reach_spring_bound(spring, 3, begin, finish);
            double b4 = reach_spring_bound(spring, 4, begin, finish);
            double first_raw = raw_at(begin), last_raw = raw_at(finish);
            double fourth = fabs(slope) * b4;
            if (motion.has_mapping == REACH_MOTION_MAPPING_QUADRATIC)
                fourth = fmax(fabs(reach_motion_map_slope(motion, first_raw, forward)),
                              fabs(reach_motion_map_slope(motion, last_raw, forward))) *
                             b4 +
                         fabs(motion.mapping_right_slope) * (8 * b1 * b3 + 6 * b2 * b2);
            else if (motion.has_mapping == REACH_MOTION_MAPPING_RATIO)
            {
                double factor = fabs(motion.mapping_right_slope);
                double denominator =
                    fmin(motion.mapping_center + motion.mapping_right_slope * first_raw,
                         motion.mapping_center + motion.mapping_right_slope * last_raw);
                double determinant = (double)motion.mapping_left_slope * motion.mapping_center -
                                     (double)motion.mapping_value * motion.mapping_right_slope;
                double d1 = b1 * factor, d2 = b2 * factor, d3 = b3 * factor, d4 = b4 * factor;
                fourth = factor == 0 ? fabs(motion.mapping_left_slope / motion.mapping_center) * b4
                                     : fabs(determinant / motion.mapping_right_slope) *
                                           (24 * pow(d1, 4) / pow(denominator, 5) +
                                            36 * d1 * d1 * d2 / pow(denominator, 4) +
                                            (6 * d2 * d2 + 8 * d1 * d3) / pow(denominator, 3) +
                                            d4 / (denominator * denominator));
            }
            double width = finish - begin;
            return fourth * width * width * width * width / 384;
        };
        double allowance = end >= duration ? tolerance - gap : tolerance;
        while (at < end)
        {
            if (count == REACH_MOTION_MAX_SEGMENTS)
                return 0;
            double next = end;
            if (!constant && at != tail && !(error(at, end) <= allowance))
            {
                double low = at, high = end;
                for (int step = 0; step < 48; ++step)
                {
                    double middle = (low + high) * .5;
                    if (error(at, middle) <= allowance)
                        low = middle;
                    else
                        high = middle;
                }
                next = low;
                if (end - next < 1e-7)
                    next = at + (end - at) * .5;
            }
            if (!(next > at + 1e-7))
                return 0;
            reach_motion_segment segment = {};
            segment.start = at - elapsed;
            segment.end = next - elapsed;
            segment.value = output(at);
            segment.end_value = output(next);
            if (constant)
            {
                if (motion.has_mapping == REACH_MOTION_MAPPING_STEPS)
                    segment.value = reach_motion_limit(&motion, (float)middle_raw);
            }
            else if (at == tail)
            {
                double dt = next - at;
                double chord = ((double)segment.end_value - segment.value) / dt;
                segment.quadratic = (float)(3 * chord / dt);
                segment.cubic = (float)(-2 * chord / (dt * dt));
            }
            else
            {
                double dt = next - at;
                double first_slope =
                    smooth ? reach_motion_map_slope(motion, raw_at(at), forward) : slope;
                double last_slope =
                    smooth ? reach_motion_map_slope(motion, raw_at(next), forward) : slope;
                double first_derivative = first_slope * reach_spring_rate(spring, at);
                double last_derivative = last_slope * reach_spring_rate(spring, next);
                double chord = ((double)segment.end_value - segment.value) / dt;
                segment.linear = (float)first_derivative;
                segment.quadratic =
                    (float)((3 * chord - 2 * first_derivative - last_derivative) / dt);
                segment.cubic =
                    (float)((first_derivative + last_derivative - 2 * chord) / (dt * dt));
            }
            segments[count++] = segment;
            at = next;
        }
    }
    if (motion.has_mapping == REACH_MOTION_MAPPING_STEPS)
        for (size_t index = 0; index + 1 < count; ++index)
            segments[index].end_value = segments[index + 1].value;
    return count;
}

static double reach_motion_spring_time_to_value(const reach_motion &motion, float target_value,
                                                double now_seconds)
{
    double delay = fmax(0.0, motion.started_seconds - now_seconds);
    double elapsed = fmax(0.0, now_seconds - motion.started_seconds);
    double duration = motion.duration_seconds;
    double remaining = delay + fmax(0.0, duration - elapsed);
    if (!isfinite(target_value) || !reach_spring_valid(motion))
        return remaining;
    if (reach_motion_value_at(&motion, now_seconds) == target_value)
        return 0;
    reach_spring spring = reach_spring_make(motion);
    double times[512] = {};
    size_t count = 0;
    if (!reach_motion_spring_breaks(motion, spring, elapsed, duration, times, 512, &count))
        return remaining;
    auto output = [&](double t)
    { return reach_motion_limit(&motion, (float)(spring.to + reach_spring_offset(spring, t))); };
    for (size_t index = 1; index < count; ++index)
    {
        double left = times[index - 1], right = times[index];
        float first = output(left), last = output(right), middle = output((left + right) * .5);
        if (motion.has_mapping == REACH_MOTION_MAPPING_STEPS || (first == last && first == middle))
        {
            if (middle == target_value)
                return delay + left - elapsed;
            continue;
        }
        if (target_value < fminf(first, last) || target_value > fmaxf(first, last))
            continue;
        double low = left, high = right;
        for (int step = 0; step < 64; ++step)
        {
            double at = (low + high) * .5;
            float value = output(at);
            if (last > first ? value >= target_value : value <= target_value)
                high = at;
            else
                low = at;
        }
        return delay + high - elapsed;
    }
    return remaining;
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
    if (motion->easing == REACH_EASING_SPRING)
        return reach_motion_compile_spring(*motion, elapsed, segments);
    double boundaries[REACH_MOTION_MAX_SEGMENTS + 1] = {elapsed, duration};
    size_t boundary_count = 2;
    double split = duration * 0.5;
    if (motion->easing != REACH_EASING_EASE_IN && motion->easing != REACH_EASING_EASE_OUT &&
        motion->easing != REACH_EASING_EXPONENTIAL_OUT && split > elapsed)
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
    size_t unique_count = 1;
    for (size_t index = 1; index < boundary_count; ++index)
        if (boundaries[index] > boundaries[unique_count - 1])
            boundaries[unique_count++] = boundaries[index];
    boundary_count = unique_count;
    size_t count = 0;
    if (motion->has_mapping == REACH_MOTION_MAPPING_RATIO ||
        motion->has_mapping == REACH_MOTION_MAPPING_QUADRATIC)
    {
        float minimum = 0, maximum = 0;
        if (reach_motion_range(motion, &minimum, &maximum) != REACH_OK)
            return 0;
        double tolerance = 2e-7 * fmax(1e-4, fmax(fabs(minimum), fabs(maximum)));
        for (size_t index = 1; index < boundary_count; ++index)
        {
            double begin = boundaries[index - 1] / duration;
            double end = boundaries[index] / duration;
            while (begin < end)
            {
                if (count == REACH_MOTION_MAX_SEGMENTS)
                    return 0;
                reach_motion_segment candidate = {};
                size_t candidate_count = 0;
                double next = end;
                if (!reach_motion_nonlinear_piece(*motion, begin, next, elapsed, tolerance,
                                                  &candidate, &candidate_count, 24))
                {
                    double low = begin, high = end;
                    for (int iteration = 0; iteration < 20; ++iteration)
                    {
                        double middle = (low + high) * .5;
                        reach_motion_segment piece = {};
                        size_t piece_count = 0;
                        if (reach_motion_nonlinear_piece(*motion, begin, middle, elapsed, tolerance,
                                                         &piece, &piece_count, 24))
                        {
                            low = middle;
                            candidate = piece;
                        }
                        else
                            high = middle;
                    }
                    next = low;
                }
                if (next <= begin)
                    return 0;
                segments[count++] = candidate;
                begin = next;
            }
        }
        return count;
    }
    if (motion->easing == REACH_EASING_EXPONENTIAL_OUT)
    {
        double normalization = -expm1(-12.0);
        for (size_t index = 1; index < boundary_count; ++index)
        {
            double at = boundaries[index - 1], end = boundaries[index];
            while (at < end)
            {
                double middle = (at + end) * .5 / duration;
                double multiplier = 1, offset = 0;
                double middle_value = motion->from + span * -expm1(-12 * middle) / normalization;
                reach_motion_line(*motion, middle_value, multiplier, offset);
                middle_value = middle_value * multiplier + offset;
                bool constant = motion->has_mapping == REACH_MOTION_MAPPING_STEPS ||
                                (motion->has_limits && (middle_value <= motion->minimum ||
                                                        middle_value >= motion->maximum));
                double fourth = constant
                                    ? 0
                                    : fabs(multiplier * span) * 20736 * exp(-12 * at / duration) /
                                          (normalization * pow(duration, 4));
                double next =
                    constant ? end
                             : fmin(end, at + fmin(duration / 6, fourth > 0 ? pow(24 / fourth, .25)
                                                                            : duration / 6));
                if (count >= REACH_MOTION_MAX_SEGMENTS || next <= at || !isfinite(next))
                    return 0;
                reach_motion_segment segment = {};
                segment.start = at - elapsed;
                segment.end = next - elapsed;
                float first =
                    (float)(motion->from + span * -expm1(-12 * at / duration) / normalization);
                float last = next == duration
                                 ? motion->to
                                 : (float)(motion->from +
                                           span * -expm1(-12 * next / duration) / normalization);
                segment.value = reach_motion_limit(motion, first);
                segment.end_value = reach_motion_limit(motion, last);
                if (constant)
                {
                    if (motion->has_mapping == REACH_MOTION_MAPPING_STEPS)
                        segment.value = (float)middle_value;
                }
                else
                {
                    double dt = next - at;
                    double first_derivative = multiplier * span * 12 * exp(-12 * at / duration) /
                                              (normalization * duration);
                    double last_derivative = multiplier * span * 12 * exp(-12 * next / duration) /
                                             (normalization * duration);
                    double slope = ((double)segment.end_value - segment.value) / dt;
                    segment.linear = (float)first_derivative;
                    segment.quadratic =
                        (float)((3 * slope - 2 * first_derivative - last_derivative) / dt);
                    segment.cubic =
                        (float)((first_derivative + last_derivative - 2 * slope) / (dt * dt));
                }
                segments[count++] = segment;
                at = next;
            }
        }
        if (motion->has_mapping == REACH_MOTION_MAPPING_STEPS)
            for (size_t index = 0; index + 1 < count; ++index)
                segments[index].end_value = segments[index + 1].value;
        return count;
    }
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
        if (motion->has_mapping == REACH_MOTION_MAPPING_STEPS)
            segment.value = (float)middle_value;
        else if (motion->has_limits &&
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
    if (motion->has_mapping == REACH_MOTION_MAPPING_STEPS)
        for (size_t index = 0; index + 1 < count; ++index)
            segments[index].end_value = segments[index + 1].value;
    return count;
}

void reach_motion_repeat_at(reach_motion *motion, float from, float to, double travel_seconds,
                            double hold_start_seconds, double hold_end_seconds, reach_easing easing,
                            double now_seconds)
{
    if (motion == nullptr)
        return;
    double cycle = travel_seconds * 2 + hold_start_seconds + hold_end_seconds;
    if (easing == REACH_EASING_SPRING || !isfinite(travel_seconds) || travel_seconds < 0 ||
        !isfinite(hold_start_seconds) || hold_start_seconds < 0 || !isfinite(hold_end_seconds) ||
        hold_end_seconds < 0 || !isfinite(cycle) || cycle <= 0 || !isfinite(now_seconds) ||
        !isfinite(from) || !isfinite(to))
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
    if (motion->easing == REACH_EASING_SPRING)
    {
        if (!reach_spring_valid(*motion))
            return reach_motion_limit(motion, motion->to);
        return reach_motion_limit(
            motion,
            (float)(motion->to + reach_spring_offset(reach_spring_make(*motion),
                                                     now_seconds - motion->started_seconds)));
    }
    float progress = (float)((now_seconds - motion->started_seconds) / motion->duration_seconds);
    if (motion->easing == REACH_EASING_EXPONENTIAL_OUT)
        return reach_motion_limit(
            motion,
            (float)((double)motion->from + ((double)motion->to - motion->from) *
                                               reach_easing_apply(progress, motion->easing)));
    return reach_motion_limit(motion,
                              motion->from + (motion->to - motion->from) *
                                                 reach_easing_apply(progress, motion->easing));
}

float reach_motion_value(const reach_motion *motion)
{
    return reach_motion_value_at(motion, reach_motion_now_seconds());
}

static double reach_easing_rate(double progress, reach_easing easing)
{
    double t = fmin(1.0, fmax(0.0, progress));
    if (easing == REACH_EASING_EASE_IN)
        return 3 * t * t;
    if (easing == REACH_EASING_EASE_OUT)
        return 3 * (1 - t) * (1 - t);
    if (easing == REACH_EASING_EXPONENTIAL_OUT)
        return 12 * exp(-12 * t) / -expm1(-12.0);
    return t < .5 ? 12 * t * t : 12 * (1 - t) * (1 - t);
}

static void reach_motion_raw_rate(const reach_motion &motion, double now_seconds, double &raw,
                                  double &rate)
{
    double span = (double)motion.to - motion.from;
    raw = motion.to;
    rate = 0;
    if (motion.repeating)
    {
        double cycle = reach_motion_cycle_seconds(&motion);
        raw = motion.from;
        if (cycle <= 0 || now_seconds <= motion.started_seconds || motion.duration_seconds == 0)
            return;
        double phase = reach_motion_repeat_phase(&motion, now_seconds, cycle);
        if (phase <= motion.hold_start_seconds)
            return;
        phase -= motion.hold_start_seconds;
        if (phase <= motion.duration_seconds)
        {
            double progress = phase / motion.duration_seconds;
            raw = motion.from + span * reach_easing_apply((float)progress, motion.easing);
            rate = span * reach_easing_rate(progress, motion.easing) / motion.duration_seconds;
            return;
        }
        phase -= motion.duration_seconds;
        raw = motion.to;
        if (phase <= motion.hold_end_seconds)
            return;
        phase -= motion.hold_end_seconds;
        double progress = phase / motion.duration_seconds;
        raw = motion.to - span * reach_easing_apply((float)progress, motion.easing);
        rate = -span * reach_easing_rate(progress, motion.easing) / motion.duration_seconds;
        return;
    }
    if (motion.duration_seconds <= 0.0 || now_seconds >= reach_motion_deadline(&motion))
        return;
    raw = motion.from;
    if (now_seconds < motion.started_seconds)
        return;
    double elapsed = now_seconds - motion.started_seconds;
    if (motion.easing == REACH_EASING_SPRING)
    {
        if (!reach_spring_valid(motion))
            return;
        reach_spring spring = reach_spring_make(motion);
        raw = spring.to + reach_spring_offset(spring, elapsed);
        rate = reach_spring_rate(spring, elapsed);
        return;
    }
    double progress = elapsed / motion.duration_seconds;
    raw = motion.from + span * reach_easing_apply((float)progress, motion.easing);
    rate = span * reach_easing_rate(progress, motion.easing) / motion.duration_seconds;
}

float reach_motion_velocity_at(const reach_motion *motion, double now_seconds)
{
    if (motion == nullptr || !isfinite(now_seconds) ||
        motion->has_mapping == REACH_MOTION_MAPPING_STEPS)
        return 0.0f;
    double raw = 0, rate = 0;
    reach_motion_raw_rate(*motion, now_seconds, raw, rate);
    if (rate == 0 || !isfinite(rate))
        return 0.0f;
    double velocity = reach_motion_map_slope(*motion, raw, rate > 0) * rate;
    if (motion->has_limits)
    {
        reach_motion unbounded = *motion;
        unbounded.has_limits = 0;
        float mapped = reach_motion_limit(&unbounded, (float)raw);
        if (mapped < motion->minimum || mapped > motion->maximum ||
            (mapped == motion->minimum && velocity < 0) ||
            (mapped == motion->maximum && velocity > 0))
            return 0.0f;
    }
    return isfinite(velocity) ? (float)velocity : 0.0f;
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
    if (!reach_motion_active_at(motion, now_seconds) || !reach_motion_moves(*motion))
        return 0.0;
    if (motion->easing == REACH_EASING_SPRING)
        return reach_motion_spring_time_to_value(*motion, target_value, now_seconds);
    if (motion->has_mapping == REACH_MOTION_MAPPING_QUADRATIC)
    {
        if (!isfinite(target_value) || reach_motion_value_at(motion, now_seconds) == target_value)
            return 0;
        double remaining = fmax(0.0, reach_motion_deadline(motion) - now_seconds);
        if (motion->has_limits &&
            (target_value < motion->minimum || target_value > motion->maximum))
            return remaining;
        double roots[2] = {};
        size_t count =
            reach_motion_quadratic_roots(motion->mapping_right_slope, motion->mapping_left_slope,
                                         (double)motion->mapping_value - target_value, roots);
        for (size_t at = 0; at < count; ++at)
        {
            double progress = (roots[at] - motion->from) / ((double)motion->to - motion->from);
            if (progress < 0 || progress > 1)
                continue;
            double seconds = motion->started_seconds +
                             reach_motion_crossing_progress(progress, motion->easing) *
                                 motion->duration_seconds -
                             now_seconds;
            if (seconds >= 0)
                remaining = fmin(remaining, seconds);
        }
        return remaining;
    }
    if (motion->has_mapping == REACH_MOTION_MAPPING_RATIO)
    {
        float current = reach_motion_value_at(motion, now_seconds);
        float endpoint = reach_motion_limit(motion, motion->to);
        if (endpoint >= current ? target_value <= current : target_value >= current)
            return 0;
        if (endpoint >= current ? target_value > endpoint : target_value < endpoint)
            return fmax(0.0, reach_motion_deadline(motion) - now_seconds);
        double divisor = motion->mapping_left_slope - target_value * motion->mapping_right_slope;
        if (divisor == 0)
            return fmax(0.0, reach_motion_deadline(motion) - now_seconds);
        double raw = (target_value * motion->mapping_center - motion->mapping_value) / divisor;
        double progress = (raw - motion->from) / ((double)motion->to - motion->from);
        return fmax(0.0, motion->started_seconds +
                             reach_motion_crossing_progress(progress, motion->easing) *
                                 motion->duration_seconds -
                             now_seconds);
    }
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
            if (motion->has_mapping == REACH_MOTION_MAPPING_STEPS)
            {
                if (piece.value == target_value)
                    return fmax(0.0, motion->started_seconds - now_seconds) + piece.start;
                if (index + 1 == count && piece.end_value == target_value)
                    return fmax(0.0, motion->started_seconds - now_seconds) + piece.end;
                continue;
            }
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
        return motion->repeating || motion->has_mapping == REACH_MOTION_MAPPING_STEPS
                   ? INFINITY
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
