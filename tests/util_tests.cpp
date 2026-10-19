#include "reach/support/util.h"
#include "reach/support/motion.h"

#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include <initializer_list>

static int failed = 0;

static void expect(int condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        failed += 1;
    }
}

static void test_motion_retarget_uses_one_event_time(void)
{
    reach_motion motion = {};
    reach_motion_start_at(&motion, 20.0f, 80.0f, 1.0, REACH_EASING_EASE_IN_OUT, 10.0);
    float interrupted = reach_motion_value_at(&motion, 10.3);
    uint64_t generation = motion.generation;
    reach_motion_animate_to_at(&motion, 0.0f, 0.5, REACH_EASING_EASE_OUT, 10.3);
    expect(motion.from == interrupted && motion.started_seconds == 10.3 &&
               motion.generation == generation + 1,
           "retarget samples and submits at exactly one event time");
    expect(reach_motion_active_at(&motion, 10.79) && !reach_motion_active_at(&motion, 10.8),
           "explicit motion sampling has a deterministic completion boundary");
    expect(reach_motion_value_at(&motion, 11.0) == 0.0f,
           "retarget reaches its final value without accumulated ticks");
    reach_motion_set(&motion, 7.0f);
    expect(!reach_motion_active_at(&motion, 10.4) && motion.generation == generation + 2 &&
               reach_motion_value_at(&motion, 10.4) == 7.0f,
           "reset cancels the previous absolute-time generation");
}

static void test_motion_affine_preserves_the_physical_timeline(void)
{
    for (int easing = REACH_EASING_EASE_IN; easing <= REACH_EASING_EASE_IN_OUT; ++easing)
    {
        reach_motion motion = {};
        reach_motion_start_at(&motion, 3.0f, 20.0f, 2.0, (reach_easing)easing, 10.0);
        reach_motion edge = reach_motion_affine(&motion, -1.0f, 80.0f);
        expect(edge.started_seconds == motion.started_seconds &&
                   edge.duration_seconds == motion.duration_seconds &&
                   edge.generation == motion.generation && edge.easing == motion.easing,
               "derived geometry preserves event time, easing and generation");
        for (int sample = 0; sample <= 10; ++sample)
        {
            double time = 10.0 + 0.2 * sample;
            expect(fabsf(reach_motion_value_at(&edge, time) -
                         (80.0f - reach_motion_value_at(&motion, time))) < 0.0001f,
                   "native edge mapping follows the same physical motion as its inset");
        }
    }
}

static void test_motion_crossing_preserves_endpoint_deadlines(void)
{
    for (int easing = REACH_EASING_EASE_IN; easing <= REACH_EASING_EASE_IN_OUT; ++easing)
    {
        reach_motion motion = {};
        reach_motion_start_at(&motion, 80.0f, 20.0f, 2.0, (reach_easing)easing, 10.0);
        double crossing = reach_motion_time_to_value_at(&motion, 35.0f, 10.1);
        expect(crossing > 0.0 &&
                   fabsf(reach_motion_value_at(&motion, 10.1 + crossing) - 35.0f) < 0.001f,
               "crossing solver handles descending motion for every easing");
        expect(fabs(reach_motion_time_to_value_at(&motion, 20.0f, 10.1) - 1.9) < 0.000001,
               "endpoint crossing uses the full deadline rather than float rounding");
        expect(reach_motion_time_to_value_at(&motion, 80.0f, 10.1) == 0.0 &&
                   reach_motion_time_to_value_at(&motion, 35.0f, 12.1) == 0.0,
               "already crossed and completed motion need no additional wait");
    }
}

static void test_bounded_motion_preserves_reveal_and_retarget(void)
{
    reach_motion progress = {};
    reach_motion_start_at(&progress, 0.0f, 1.0f, 2.0, REACH_EASING_EASE_OUT, 10.0);
    reach_motion fade = reach_motion_affine(&progress, 1.0f / 0.65f, -0.10f / 0.65f);
    fade = reach_motion_clamp(&fade, 0.0f, 1.0f);
    reach_motion offset = reach_motion_affine(&fade, -4.0f, 4.0f);
    expect(fade.started_seconds == progress.started_seconds &&
               fade.duration_seconds == progress.duration_seconds &&
               fade.generation == progress.generation && offset.minimum == 0.0f &&
               offset.maximum == 4.0f,
           "bounded reveal and negative affine offset share the original timeline");
    for (int sample = 0; sample <= 100; ++sample)
    {
        double now = 10.0 + sample * 0.02;
        float expected =
            fminf(1.0f, fmaxf(0.0f, (reach_motion_value_at(&progress, now) - 0.10f) / 0.65f));
        expect(fabsf(reach_motion_value_at(&fade, now) - expected) < 0.000001f &&
                   fabsf(reach_motion_value_at(&offset, now) - (1.0f - expected) * 4.0f) <
                       0.000002f,
               "bounded reveal preserves its delayed fade, early saturation and slide");
    }
    double crossing = reach_motion_time_to_value_at(&fade, 1.0f, 10.0);
    expect(crossing > 0.0 && crossing < 2.0 &&
               fabsf(reach_motion_value_at(&fade, 10.0 + crossing) - 1.0f) < 0.000001f &&
               reach_motion_time_to_value_at(&fade, 1.0f, 11.9) == 0.0 &&
               reach_motion_deadline(&fade) == reach_motion_deadline(&progress),
           "visual saturation does not shorten the owner completion deadline");
    float interrupted = reach_motion_value_at(&fade, 10.4);
    reach_motion_animate_to_at(&fade, 0.0f, 0.3, REACH_EASING_EASE_IN_OUT, 10.4);
    expect(fade.from == interrupted && !fade.has_limits &&
               reach_motion_value_at(&fade, 10.4) == interrupted,
           "retarget starts from the bounded visible value and clears derived limits");
    reach_motion nested = reach_motion_clamp(&offset, 8.0f, 10.0f);
    expect(reach_motion_value_at(&nested, 10.5) == 8.0f && nested.minimum == 8.0f &&
               nested.maximum == 8.0f,
           "successive disjoint clamps collapse to the visible constant");
}

static void test_motion_compilation_matches_cpu_sampling(void)
{
    for (int easing = REACH_EASING_EASE_IN; easing <= REACH_EASING_EASE_IN_OUT; ++easing)
    {
        for (int direction = 0; direction < 2; ++direction)
        {
            reach_motion source = {};
            reach_motion_start_at(&source, direction ? 1.4f : -0.2f, direction ? -0.2f : 1.4f, 2.0,
                                  (reach_easing)easing, 10.0);
            reach_motion curves[3] = {source, reach_motion_clamp(&source, 0.0f, 1.0f),
                                      reach_motion_clamp(&source, 0.5f, 0.5f)};
            curves[1] = reach_motion_affine(&curves[1], -4.0f, 4.0f);
            for (const reach_motion &curve : curves)
            {
                const double submission_times[] = {0.0, 0.3, 1.0, 1.9};
                for (double elapsed : submission_times)
                {
                    reach_motion_segment segments[REACH_MOTION_MAX_SEGMENTS] = {};
                    size_t count = reach_motion_compile_at(&curve, 10.0 + elapsed, segments);
                    expect(count > 0 && count <= REACH_MOTION_MAX_SEGMENTS &&
                               segments[0].start == 0.0 &&
                               fabs(segments[count - 1].end - (2.0 - elapsed)) < 0.000001,
                           "native polynomial segments preserve remaining owner duration");
                    for (size_t index = 0; index < count; ++index)
                    {
                        const reach_motion_segment &segment = segments[index];
                        expect(index == 0 || segment.start == segments[index - 1].end,
                               "compiled segments have no timeline gaps");
                        for (int sample = 0; sample <= 20; ++sample)
                        {
                            double dt = (segment.end - segment.start) * sample / 20.0;
                            double actual = segment.value + segment.linear * dt +
                                            segment.quadratic * dt * dt +
                                            segment.cubic * dt * dt * dt;
                            float expected =
                                reach_motion_value_at(&curve, 10.0 + elapsed + segment.start + dt);
                            expect(fabs(actual - expected) < 0.000003,
                                   "native polynomial samples match bounded CPU motion");
                        }
                        expect(fabsf(segment.end_value -
                                     reach_motion_value_at(&curve, 10.0 + elapsed + segment.end)) <
                                   0.000003f,
                               "native keyframes use the bounded segment endpoint");
                    }
                    expect(segments[count - 1].end_value == reach_motion_value_at(&curve, 12.0),
                           "native final keyframe is the exact bounded endpoint");
                }
            }
        }
    }
    reach_motion motion = {};
    reach_motion_set(&motion, 1.0f);
    reach_motion_segment segments[REACH_MOTION_MAX_SEGMENTS] = {};
    expect(reach_motion_compile_at(&motion, 0.0, segments) == 0 &&
               reach_motion_compile_at(nullptr, 0.0, segments) == 0,
           "static and absent motion submit no native curve");
}

static void test_exponential_motion_compilation_and_projection(void)
{
    for (float distance : {72.f, 4000.f, 16384.f, 65536.f})
        for (int reverse : {0, 1})
        {
            reach_motion motion = {};
            reach_motion_start_at(&motion, reverse ? distance : 0, reverse ? 0 : distance, .56,
                                  REACH_EASING_EXPONENTIAL_OUT, 10);
            reach_motion curves[] = {motion, reach_motion_clamp(&motion, 100, 300),
                                     reach_motion_affine(&motion, -.5f, 100)};
            for (const auto &curve : curves)
                for (double elapsed : {0., .05, .2, .5})
                {
                    reach_motion_segment pieces[REACH_MOTION_MAX_SEGMENTS] = {};
                    size_t count = reach_motion_compile_at(&curve, 10 + elapsed, pieces);
                    expect(count > 0 && count <= REACH_MOTION_MAX_SEGMENTS,
                           "exponential projection stays within native segment capacity");
                    for (size_t index = 0; index < count; ++index)
                    {
                        const auto &piece = pieces[index];
                        double dt = piece.end - piece.start;
                        expect(index == 0 || piece.start == pieces[index - 1].end,
                               "exponential native segments have no gaps");
                        expect(index == 0 || piece.value == pieces[index - 1].end_value,
                               "exponential native pieces retain exact junction values");
                        for (int sample = 0; sample <= 100; ++sample)
                        {
                            double at = dt * sample / 100;
                            double value = piece.value + piece.linear * at +
                                           piece.quadratic * at * at + piece.cubic * at * at * at;
                            float expected =
                                reach_motion_value_at(&curve, 10 + elapsed + piece.start + at);
                            expect(fabs(value - expected) <= .075,
                                   "exponential native cubic error remains below a tenth pixel");
                        }
                    }
                    expect(count == 0 ||
                               pieces[count - 1].end_value == reach_motion_value_at(&curve, 10.56),
                           "exponential final keyframe is the exact canonical endpoint");
                }
            double crossing = reach_motion_time_to_value_at(&motion, distance * .5f, 10);
            expect(crossing > 0 && crossing < .56 &&
                       fabsf(reach_motion_value_at(&motion, 10 + crossing) - distance * .5f) < .01f,
                   "exponential source turnover crossing uses canonical event time");
        }
    reach_motion oversized = {};
    reach_motion_start_at(&oversized, -3e38f, 3e38f, .56, REACH_EASING_EXPONENTIAL_OUT, 10);
    reach_motion_segment pieces[REACH_MOTION_MAX_SEGMENTS] = {};
    expect(reach_motion_compile_at(&oversized, 10, pieces) == 0 &&
               isfinite(reach_motion_value_at(&oversized, 10.2)) &&
               !reach_motion_active_at(&oversized, 10.56),
           "unrepresentable native curves preserve finite canonical fallback and deadline");
}

static void test_peak_motion_compilation_and_crossings(void)
{
    for (int easing = REACH_EASING_EASE_IN; easing <= REACH_EASING_EASE_IN_OUT; ++easing)
        for (int reverse = 0; reverse < 2; ++reverse)
        {
            reach_motion source = {}, peak = {};
            reach_motion_start_at(&source, reverse ? 3.4f : -1.4f, reverse ? -1.4f : 3.4f, 2,
                                  (reach_easing)easing, 10);
            expect(reach_motion_peak(&source, 1, 1, &peak) == REACH_OK,
                   "peak derives a bounded nonmonotonic mapping from canonical intent");
            expect(peak.generation == source.generation &&
                       peak.started_seconds == source.started_seconds,
                   "peak mapping preserves the owner's clock and generation");
            reach_motion mapped = reach_motion_affine(&peak, -0.7f, 0.9f);
            mapped = reach_motion_clamp(&mapped, 0.3f, 0.8f);
            for (double elapsed : {0.0, 0.3, 1.0, 1.9})
            {
                reach_motion_segment pieces[REACH_MOTION_MAX_SEGMENTS] = {};
                size_t count = reach_motion_compile_at(&mapped, 10 + elapsed, pieces);
                expect(count > 0 && count <= REACH_MOTION_MAX_SEGMENTS,
                       "peak compiler has bounded capacity after affine mapping and clamp");
                for (size_t index = 0; index < count; ++index)
                    for (int sample = 0; sample <= 20; ++sample)
                    {
                        auto piece = pieces[index];
                        double t = (piece.end - piece.start) * sample / 20;
                        double actual = piece.value + piece.linear * t + piece.quadratic * t * t +
                                        piece.cubic * t * t * t;
                        double now = 10 + elapsed + piece.start + t;
                        float expected = fminf(
                            0.8f,
                            fmaxf(0.3f, 0.9f - 0.7f * fmaxf(0, 1 - fabsf(reach_motion_value_at(
                                                                             &source, now) -
                                                                         1))));
                        expect(fabs(actual - expected) < 0.000004 &&
                                   fabsf(reach_motion_value_at(&mapped, now) - expected) <
                                       0.000004f,
                               "compiled peak pieces and CPU sampling preserve both slopes and "
                               "plateaus");
                    }
            }
            double crossing = reach_motion_time_to_value_at(&peak, 0.5f, 10);
            expect(crossing > 0 && crossing < 2 &&
                       fabsf(reach_motion_value_at(&peak, 10 + crossing) - 0.5f) < 0.00001f,
                   "peak crossing finds its first future ascending or descending boundary");
            double second = reach_motion_time_to_value_at(&peak, 0.5f, 10 + crossing + 0.01);
            expect(second > 0 && fabsf(reach_motion_value_at(&peak, 10 + crossing + 0.01 + second) -
                                       0.5f) < 0.00001f,
                   "peak crossing also resolves the opposite slope after the first crossing");
            expect(reach_motion_value_at(&peak, 12) == 0 && reach_motion_deadline(&peak) == 12,
                   "equal visible endpoints retain the intervening peak and owner deadline");
        }
    reach_motion source = {}, peak = {};
    reach_motion_repeat_at(&source, 0, 2, 1, 0.2, 0.3, REACH_EASING_EASE_OUT, 10);
    expect(reach_motion_peak(&source, 1, 1, &peak) == REACH_OK && reach_motion_active_at(&peak, 10),
           "periodic peak is active even when its visible endpoints are equal");
    reach_motion_segment pieces[REACH_MOTION_MAX_REPEAT_SEGMENTS] = {};
    for (double now : {10.0, 10.4, 11.5, 12.3, 13.2})
    {
        size_t count = reach_motion_compile_repeat_at(&peak, now, pieces);
        expect(count > 0 && count <= REACH_MOTION_MAX_REPEAT_SEGMENTS,
               "rotated periodic peak fits its bounded cycle compiler");
        for (size_t index = 0; index < count; ++index)
            for (int sample = 0; sample <= 20; ++sample)
            {
                auto piece = pieces[index];
                double t = (piece.end - piece.start) * sample / 20;
                double actual = piece.value + piece.linear * t + piece.quadratic * t * t +
                                piece.cubic * t * t * t;
                expect(fabs(actual - reach_motion_value_at(&peak, now + piece.start + t)) <
                           0.000004,
                       "periodic peak holds and both travel legs agree with CPU sampling");
            }
    }
    reach_motion bounded = reach_motion_clamp(&source, 0, 2);
    reach_motion bounded_peak = {};
    expect(reach_motion_peak(&bounded, 1, 1, &bounded_peak) == REACH_OK,
           "input limits enclosing peak support preserve the exact mapped curve");
    for (double now : {10.0, 10.4, 11.5, 12.3})
        expect(reach_motion_value_at(&bounded_peak, now) == reach_motion_value_at(&peak, now),
               "enclosing input bounds do not alter the peak or its holds");
    bounded = reach_motion_clamp(&source, 0.5f, 1.5f);
    expect(reach_motion_peak(&bounded, 1, 1, &bounded_peak) == REACH_INVALID_ARGUMENT,
           "a clamp inside peak support is rejected rather than silently changing the curve");
    expect(reach_motion_peak(&peak, 1, 1, &source) == REACH_INVALID_ARGUMENT &&
               reach_motion_peak(&source, 1, 0, &peak) == REACH_INVALID_ARGUMENT,
           "unsupported nested mapping and invalid peak radius are rejected");
}

static void test_hinge_motion_and_same_epoch_combination(void)
{
    for (int easing = REACH_EASING_EASE_IN; easing <= REACH_EASING_EASE_IN_OUT; ++easing)
        for (int reverse = 0; reverse < 2; ++reverse)
        {
            reach_motion width = {}, focus = {}, shoulder = {};
            reach_motion_start_at(&width, reverse ? 600 : 180, reverse ? 180 : 600, 2,
                                  (reach_easing)easing, 10);
            expect(reach_motion_hinge(&width, 320, 160, 0.5f, 0, &focus) == REACH_OK &&
                       reach_motion_combine(&width, 1, &focus, -1, -30, &shoulder) == REACH_OK,
                   "fixed focus and stretch shoulder derive from one width epoch");
            expect(shoulder.generation == width.generation &&
                       shoulder.started_seconds == width.started_seconds &&
                       shoulder.duration_seconds == width.duration_seconds,
                   "combination preserves original generation and physical timeline");
            float minimum = 0, maximum = 0;
            expect(reach_motion_range(&shoulder, &minimum, &maximum) == REACH_OK && minimum == 60 &&
                       maximum == 410,
                   "derived resize range covers both sides of the focus breakpoint");
            reach_motion limited = reach_motion_clamp(&shoulder, 90, 350);
            for (double elapsed : {0.0, 0.3, 1.0, 1.9})
            {
                reach_motion_segment pieces[REACH_MOTION_MAX_SEGMENTS] = {};
                size_t count = reach_motion_compile_at(&limited, 10 + elapsed, pieces);
                expect(count > 0 && count <= REACH_MOTION_MAX_SEGMENTS,
                       "hinge and output clamps fit the existing bounded compiler");
                for (size_t index = 0; index < count; ++index)
                    for (int sample = 0; sample <= 20; ++sample)
                    {
                        auto piece = pieces[index];
                        double t = (piece.end - piece.start) * sample / 20;
                        double now = 10 + elapsed + piece.start + t;
                        float w = reach_motion_value_at(&width, now);
                        float expected = fminf(350, fmaxf(90, w - fminf(w / 2, 160) - 30));
                        double actual = piece.value + piece.linear * t + piece.quadratic * t * t +
                                        piece.cubic * t * t * t;
                        expect(fabs(actual - expected) < 0.0002 &&
                                   fabsf(reach_motion_value_at(&limited, now) - expected) < 0.0002f,
                               "native resize pieces match independent geometry on both slopes");
                    }
            }
            reach_motion doubled = {};
            expect(reach_motion_combine(&focus, 2, &focus, -1, 4, &doubled) == REACH_OK,
                   "matching mapped properties combine exactly");
            for (double now : {10.0, 10.4, 11.0, 11.9, 12.0})
                expect(fabsf(reach_motion_value_at(&doubled, now) -
                             reach_motion_value_at(&focus, now) - 4) < 0.0001f,
                       "mapped combination preserves pointwise values");
        }
    reach_motion width = {}, focus = {}, other = {}, result = {};
    reach_motion_start_at(&width, 180, 600, 2, REACH_EASING_EASE_IN_OUT, 10);
    expect(reach_motion_hinge(&width, 320, 160, 0.5f, 0, &focus) == REACH_OK,
           "valid hinge construction");
    other = width;
    other.started_seconds += 0.01;
    expect(reach_motion_combine(&focus, 1, &other, 1, 0, &result) == REACH_INVALID_ARGUMENT,
           "different clocks are rejected instead of approximated");
    other = reach_motion_clamp(&width, 200, 500);
    expect(reach_motion_hinge(&other, 320, 160, 0.5f, 0, &result) == REACH_INVALID_ARGUMENT &&
               reach_motion_combine(&focus, 1, &other, 1, 0, &result) == REACH_OK,
           "paired output clamps compose exactly while nested input mapping stays unsupported");
    other = reach_motion_clamp(&width, 100, 700);
    expect(reach_motion_hinge(&other, 320, 160, 0.5f, 0, &result) == REACH_OK,
           "inactive input clamps can be removed exactly");
    expect(reach_motion_hinge(&width, 400, 200, 0.5f, 0, &other) == REACH_OK &&
               reach_motion_combine(&focus, 1, &other, 1, 0, &result) == REACH_OK &&
               result.mapping_extra_count == 1,
           "different breakpoints compose through bounded canonical mapping");
    reach_motion_set(&other, 7);
    expect(reach_motion_combine(&focus, 1, &other, 2, 3, &result) == REACH_OK &&
               reach_motion_value_at(&result, 11) == reach_motion_value_at(&focus, 11) + 17,
           "constant terms do not require clock ownership");
    float minimum = 0, maximum = 0;
    expect(reach_motion_hinge(&width, 320, 0, -1, 1, &other) == REACH_OK &&
               reach_motion_range(&other, &minimum, &maximum) == REACH_OK && minimum == 0 &&
               maximum == 280,
           "range includes interior extrema rather than just endpoints");
    other.mapping_value = NAN;
    expect(reach_motion_range(&other, &minimum, &maximum) == REACH_INVALID_ARGUMENT,
           "malformed mappings cannot pass geometry validation through masked NaNs");
}

static void test_composed_fitting_motion_preserves_constraints(void)
{
    for (int easing = REACH_EASING_EASE_IN; easing <= REACH_EASING_EASE_IN_OUT; ++easing)
        for (int reverse = 0; reverse < 2; ++reverse)
        {
            reach_motion progress = {}, body = {};
            reach_motion_start_at(&progress, reverse ? 1 : 0, reverse ? 0 : 1, 2,
                                  (reach_easing)easing, 10);
            reach_motion width = reach_motion_affine(&progress, 644, 96);
            expect(reach_motion_hinge(&width, 148, 736, -0.5f, -1, &body) == REACH_OK,
                   "monitor edge placement derives from the width epoch");
            body = reach_motion_clamp(&body, 116, 884);
            reach_motion fit = reach_motion_affine(&width, 1.0f / 700, -26.0f / 700);
            fit = reach_motion_clamp(&fit, 0, 1);
            reach_motion reflow = reach_motion_affine(&progress, -360, 360);
            reach_motion position = {}, upper = {};
            expect(reach_motion_combine(&body, 1, &fit, 180, 13, &position) == REACH_OK &&
                       reach_motion_combine(&position, 1, &reflow, 1, 0, &position) == REACH_OK &&
                       reach_motion_combine(&body, 1, &width, 1, -13, &upper) == REACH_OK &&
                       reach_motion_combine(&upper, 1, &fit, -220, 0, &upper) == REACH_OK,
                   "placement fitting and reflow share one exact descriptor");
            reach_motion lower = reach_motion_affine(&body, 1, 13);
            expect(reach_motion_max(&position, &lower, &position) == REACH_OK &&
                       reach_motion_min(&position, &upper, &position) == REACH_OK,
                   "moving viewport bounds constrain reflow without a sampled frame loop");
            for (double elapsed : {0.0, 0.3, 1.0, 1.9})
            {
                reach_motion_segment pieces[REACH_MOTION_MAX_SEGMENTS] = {};
                size_t count = reach_motion_compile_at(&position, 10 + elapsed, pieces);
                expect(count > 0 && count <= REACH_MOTION_MAX_SEGMENTS,
                       "composed fitting compiles to bounded native polynomials");
                for (size_t at = 0; at < count; ++at)
                    for (int sample = 0; sample <= 20; ++sample)
                    {
                        auto piece = pieces[at];
                        double t = (piece.end - piece.start) * sample / 20;
                        double now = 10 + elapsed + piece.start + t;
                        float p = reach_motion_value_at(&progress, now), w = 96 + 644 * p;
                        float x = fmaxf(116, fminf(884, w <= 148 ? 810 - w * 0.5f : 884 - w));
                        float scale = fminf(1, fmaxf(0, (w - 26) / 700));
                        float expected = fminf(x + w - 13 - 220 * scale,
                                               fmaxf(x + 13, x + 13 + 180 * scale + 360 * (1 - p)));
                        double native = piece.value + piece.linear * t + piece.quadratic * t * t +
                                        piece.cubic * t * t * t;
                        expect(fabs(native - expected) < 0.001 &&
                                   fabsf(reach_motion_value_at(&position, now) - expected) < 0.001f,
                               "GPU polynomial and CPU projection match independent fitting math");
                    }
            }
        }
}

static void test_composed_periodic_motion_preserves_phase(void)
{
    for (int easing = REACH_EASING_EASE_IN; easing <= REACH_EASING_EASE_IN_OUT; ++easing)
    {
        reach_motion progress = {}, first = {}, second = {}, combined = {};
        reach_motion_repeat_at(&progress, 0, 1, 1, 0.1, 0.2, (reach_easing)easing, 10);
        expect(reach_motion_peak(&progress, 0.25f, 0.2f, &first) == REACH_OK &&
                   reach_motion_peak(&progress, 0.7f, 0.2f, &second) == REACH_OK &&
                   reach_motion_combine(&first, 1, &second, 1, 0, &combined) == REACH_OK,
               "multiple periodic peaks share one canonical phase");
        expect(combined.mapping_extra_count > 0 && reach_motion_active_at(&combined, 10) &&
                   reach_motion_value_at(&combined, 10) == 0,
               "equal visible endpoints retain interior multi-knot activity");
        for (double elapsed : {0.0, 0.37, 1.1, 1.23, 1.9, 2.29, 9.7})
        {
            reach_motion_segment pieces[REACH_MOTION_MAX_REPEAT_SEGMENTS] = {};
            size_t count = reach_motion_compile_repeat_at(&combined, 10 + elapsed, pieces);
            expect(count > 0 && count <= REACH_MOTION_MAX_REPEAT_SEGMENTS,
                   "rotated multi-knot cycles stay within compiler capacity");
            for (size_t at = 0; at < count; ++at)
                for (int sample = 0; sample <= 10; ++sample)
                {
                    auto piece = pieces[at];
                    double t = (piece.end - piece.start) * sample / 10;
                    double now = 10 + elapsed + piece.start + t;
                    float p = reach_motion_value_at(&progress, now);
                    float expected = fmaxf(0, 1 - fabsf(p - 0.25f) / 0.2f) +
                                     fmaxf(0, 1 - fabsf(p - 0.7f) / 0.2f);
                    double native = piece.value + piece.linear * t + piece.quadratic * t * t +
                                    piece.cubic * t * t * t;
                    expect(fabs(native - expected) < 0.00001 &&
                               fabsf(reach_motion_value_at(&combined, now) - expected) < 0.00001f,
                           "periodic GPU pieces preserve both peaks and endpoint holds");
                }
        }
        double wait = reach_motion_time_to_value_at(&combined, 0.5f, 10);
        expect(wait > 0 && wait < 1.1 &&
                   fabsf(reach_motion_value_at(&combined, 10 + wait) - 0.5f) < 0.00001f,
               "crossing queries find the first future composed peak");
        float interrupted = reach_motion_value_at(&combined, 10.4);
        reach_motion_animate_to_at(&combined, 0, 0.2, REACH_EASING_EASE_OUT, 10.4);
        expect(combined.from == interrupted && !combined.repeating && !combined.has_mapping &&
                   combined.mapping_extra_count == 0,
               "retarget retires composed repetition continuously");
    }
}

static void test_composed_motion_capacity_and_identity(void)
{
    reach_motion source = {}, combined = {}, hinge = {};
    reach_motion_start_at(&source, 0, 1, 1, REACH_EASING_EASE_IN_OUT, 10);
    reach_motion_set(&combined, 0);
    for (int knot = 1; knot <= REACH_MOTION_MAX_EXTRA_KNOTS + 1; ++knot)
    {
        expect(reach_motion_hinge(&source, knot * 0.1f, 0, 0, 1, &hinge) == REACH_OK &&
                   reach_motion_combine(&combined, 1, &hinge, 1, 0, &combined) == REACH_OK,
               "bounded knot capacity accepts exact in-place composition");
    }
    expect(combined.mapping_extra_count == REACH_MOTION_MAX_EXTRA_KNOTS,
           "full mapping capacity is represented explicitly");
    reach_motion unchanged = combined;
    expect(reach_motion_hinge(&source, 0.9f, 0, 0, 1, &hinge) == REACH_OK &&
               reach_motion_combine(&combined, 1, &hinge, 1, 0, &combined) ==
                   REACH_INVALID_ARGUMENT &&
               reach_motion_equal(&combined, &unchanged),
           "excess knots reject without changing aliased output");
    reach_motion_segment pieces[REACH_MOTION_MAX_SEGMENTS] = {};
    expect(reach_motion_compile_at(&combined, 10, pieces) > 0,
           "full mapping capacity compiles within the finite bound");
    unchanged.mapping_extra_deltas[0] += 1;
    expect(!reach_motion_equal(&combined, &unchanged),
           "active extra knots participate in retained intent identity");
    unchanged = source;
    unchanged.mapping_extra_centers[0] = NAN;
    unchanged.mapping_extra_deltas[0] = NAN;
    expect(reach_motion_equal(&source, &unchanged),
           "unused mapping storage does not invalidate unchanged retained intent");
    float low = 0, high = 0;
    for (int invalid = 0; invalid < 3; ++invalid)
    {
        unchanged = combined;
        if (invalid == 0)
            unchanged.mapping_extra_count = REACH_MOTION_MAX_EXTRA_KNOTS + 1;
        else if (invalid == 1)
            unchanged.mapping_extra_centers[0] = unchanged.mapping_center;
        else
            unchanged.mapping_extra_deltas[0] = NAN;
        expect(reach_motion_range(&unchanged, &low, &high) == REACH_INVALID_ARGUMENT &&
                   reach_motion_compile_at(&unchanged, 10, pieces) == 0,
               "malformed compound metadata is rejected before compilation");
        unchanged.repeating = 1;
        reach_motion_segment repeated[REACH_MOTION_MAX_REPEAT_SEGMENTS] = {};
        expect(reach_motion_compile_repeat_at(&unchanged, 10, repeated) == 0,
               "malformed cycles cannot compile only their endpoint holds");
    }
    reach_motion independent = source;
    unchanged = source;
    unchanged.mapping_extra_count = 1;
    expect(reach_motion_hinge(&unchanged, 0.5f, 0, 0, 1, &hinge) == REACH_INVALID_ARGUMENT &&
               reach_motion_peak(&unchanged, 0.5f, 0.2f, &hinge) == REACH_INVALID_ARGUMENT,
           "mapping constructors reject orphaned compound metadata");
    independent.started_seconds += 0.01;
    expect(reach_motion_min(&combined, &independent, &unchanged) == REACH_INVALID_ARGUMENT &&
               reach_motion_max(&combined, &independent, &unchanged) == REACH_INVALID_ARGUMENT,
           "moving constraints cannot silently mix animation epochs");
}

static void test_motion_equality_preserves_identity_without_padding(void)
{
    reach_motion first = {};
    reach_motion_start_at(&first, 0, 1, 0.3, REACH_EASING_EASE_OUT, 10);
    reach_motion second = first;
    unsigned char *bytes = reinterpret_cast<unsigned char *>(&second);
    for (size_t at = offsetof(reach_motion, easing) + sizeof(second.easing);
         at < offsetof(reach_motion, generation); ++at)
        bytes[at] = 0xa5;
    for (size_t at = offsetof(reach_motion, maximum) + sizeof(second.maximum);
         at < offsetof(reach_motion, repeating); ++at)
        bytes[at] = 0xa5;
    expect(reach_motion_equal(&first, &second), "motion equality ignores representation padding");
    second.repeating = 1;
    expect(!reach_motion_equal(&first, &second), "repetition is motion intent");
    second = first;
    second.hold_start_seconds = 1;
    expect(!reach_motion_equal(&first, &second), "repetition holds preserve motion identity");
    second = first;
    second.generation++;
    expect(!reach_motion_equal(&first, &second), "a new generation retains distinct ownership");
    second = first;
    second.started_seconds += 0.01;
    expect(!reach_motion_equal(&first, &second), "a changed physical timeline remains distinct");
    expect(reach_motion_peak(&first, 0.5f, 1, &second) == REACH_OK &&
               !reach_motion_equal(&first, &second),
           "peak mapping remains part of canonical intent identity");
    first = second;
    second.mapping_center += 0.1f;
    expect(!reach_motion_equal(&first, &second),
           "changed peak geometry invalidates retained properties");
    second = reach_motion_clamp(&first, 0.2f, 0.8f);
    expect(!reach_motion_equal(&first, &second), "visible motion limits remain part of identity");
}

static void test_lookup_motion_sampling_compilation_and_retarget(void)
{
    const float thresholds[] = {25, 60, 90};
    const float values[] = {0, -24, -48, 0};
    for (int easing = REACH_EASING_EASE_IN; easing <= REACH_EASING_EASE_IN_OUT; ++easing)
        for (int direction = 0; direction < 2; ++direction)
        {
            reach_motion source = {}, steps = {};
            reach_motion_start_at(&source, direction ? 100 : 0, direction ? 0 : 100, 2,
                                  (reach_easing)easing, 10);
            expect(reach_motion_lookup(&source, thresholds, values, 3, &steps) == REACH_OK &&
                       steps.generation == source.generation &&
                       steps.started_seconds == source.started_seconds &&
                       steps.duration_seconds == source.duration_seconds,
                   "finite lookup preserves its source epoch without another motion owner");
            for (double elapsed : {0.0, 0.27, 0.9, 1.7})
            {
                reach_motion_segment pieces[REACH_MOTION_MAX_SEGMENTS] = {};
                size_t count = reach_motion_compile_at(&steps, 10 + elapsed, pieces);
                expect(count > 0 && count <= REACH_MOTION_MAX_SEGMENTS,
                       "lookup compiles through the existing bounded native segment contract");
                for (size_t index = 0; index < count; ++index)
                {
                    const auto &piece = pieces[index];
                    expect(piece.linear == 0 && piece.quadratic == 0 && piece.cubic == 0 &&
                               (index == 0 || piece.start == pieces[index - 1].end) &&
                               (index + 1 == count || piece.end_value == pieces[index + 1].value),
                           "lookup segments hold exact values and jump at shared boundaries");
                    for (double fraction : {0.01, 0.25, 0.5, 0.75, 0.99})
                    {
                        double now =
                            10 + elapsed + piece.start + (piece.end - piece.start) * fraction;
                        float width = reach_motion_value_at(&source, now);
                        size_t state = width < 25 ? 0 : width < 60 ? 1 : width < 90 ? 2 : 3;
                        expect(piece.value == values[state] &&
                                   reach_motion_value_at(&steps, now) == values[state],
                               "native and CPU lookup agree with source thresholds in both "
                               "directions");
                    }
                }
                expect(pieces[count - 1].end_value == reach_motion_value_at(&steps, 12),
                       "finite lookup ends at its exact final state");
            }
            float low = 0, high = 0;
            expect(reach_motion_range(&steps, &low, &high) == REACH_OK && low == -48 && high == 0,
                   "lookup range includes intermediate states even with equal endpoint values");
            expect(isinf(reach_motion_time_to_value_at(&steps, -12, 10)),
                   "lookup never reports a crossing through an unvisited intermediate value");
            double crossing = reach_motion_time_to_value_at(&steps, -24, 10);
            expect(crossing > 0 && crossing < 2 &&
                       reach_motion_value_at(&steps, 10 + crossing + 0.0001) == -24,
                   "lookup crossing identifies the actual first visited state");
            auto mapped = reach_motion_affine(&steps, -2, 1);
            expect(reach_motion_value_at(&mapped, 10.7) ==
                       1 - 2 * reach_motion_value_at(&steps, 10.7),
                   "affine lookup changes values without moving threshold times");
            float current = reach_motion_value_at(&steps, 10.7);
            reach_motion_animate_to_at(&steps, 10, .2, REACH_EASING_EASE_OUT, 10.7);
            expect(steps.from == current && !steps.has_mapping &&
                       steps.generation == source.generation + 1,
                   "retargeting retires lookup from its currently visible discrete value");
        }
}

static void test_lookup_motion_preflight_and_mapped_sources(void)
{
    reach_motion source = {}, mapped = {}, steps = {};
    reach_motion_start_at(&source, 0, 100, 2, REACH_EASING_EASE_IN_OUT, 10);
    expect(reach_motion_hinge(&source, 50, 50, .5f, 2, &mapped) == REACH_OK,
           "lookup source can use existing piecewise fitting");
    mapped = reach_motion_clamp(&mapped, 30, 120);
    const float thresholds[] = {20, 40, 90, 130};
    const float values[] = {0, 1, 2, 3, 4};
    expect(reach_motion_lookup(&mapped, thresholds, values, 4, &steps) == REACH_OK,
           "lookup resolves thresholds through monotone mapping and active bounds");
    for (int sample = 0; sample <= 200; ++sample)
    {
        double now = 10 + sample * .01;
        float value = reach_motion_value_at(&mapped, now);
        size_t state = value < 20 ? 0 : value < 40 ? 1 : value < 90 ? 2 : value < 130 ? 3 : 4;
        expect(reach_motion_value_at(&steps, now) == values[state],
               "mapped lookup preserves existing clamp and fitting thresholds");
    }
    reach_motion previous = steps;
    const float unordered[] = {40, 20};
    expect(reach_motion_lookup(&source, unordered, values, 2, &steps) == REACH_INVALID_ARGUMENT &&
               reach_motion_equal(&previous, &steps),
           "invalid lookup leaves output intent untouched");
    reach_motion negative = {};
    reach_motion_hinge(&source, 50, 50, -1, -1, &negative);
    expect(reach_motion_lookup(&negative, thresholds, values, 4, &steps) == REACH_INVALID_ARGUMENT,
           "lookup rejects decreasing source maps instead of approximating them");
    expect(reach_motion_combine(&previous, 1, &source, 1, 0, &steps) == REACH_INVALID_ARGUMENT,
           "unsupported continuous and discrete combinations reject rather than interpolate jumps");
    reach_motion_repeat_at(&source, 0, 100, .2, .4, .4, REACH_EASING_EASE_IN_OUT, 10);
    expect(reach_motion_lookup(&source, thresholds, values, 4, &steps) == REACH_INVALID_ARGUMENT,
           "finite lookup cannot silently become a periodic driver");
    const float one[] = {7};
    expect(reach_motion_lookup(&mapped, nullptr, one, 0, &steps) == REACH_OK &&
               reach_motion_value_at(&steps, 11) == 7 && !reach_motion_active_at(&steps, 11),
           "constant lookup owns no unnecessary motion activity");
    const float maximum_thresholds[] = {10, 20, 30, 40, 50, 60, 70, 80};
    const float maximum_values[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    reach_motion_start_at(&source, 0, 100, 2, REACH_EASING_EASE_IN_OUT, 10);
    expect(reach_motion_lookup(&source, maximum_thresholds, maximum_values, 8, &steps) ==
                   REACH_OK &&
               steps.mapping_extra_count == REACH_MOTION_MAX_EXTRA_KNOTS,
           "lookup uses the existing bounded mapping capacity without descriptor growth");
    const float midpoint[] = {50};
    const float binary[] = {0, 1};
    for (int direction = 0; direction < 2; ++direction)
    {
        reach_motion_start_at(&source, direction ? 100 : 0, direction ? 0 : 100, 2,
                              REACH_EASING_EASE_IN_OUT, 10);
        expect(reach_motion_lookup(&source, midpoint, binary, 1, &steps) == REACH_OK &&
                   reach_motion_value_at(&steps, 11) == (direction ? 0 : 1),
               "an exact lookup crossing selects the following traversal state");
        reach_motion_segment pieces[REACH_MOTION_MAX_SEGMENTS] = {};
        expect(reach_motion_compile_at(&steps, 11, pieces) > 0 &&
                   pieces[0].value == reach_motion_value_at(&steps, 11),
               "submission at an exact lookup crossing agrees with CPU state");
    }
}

static void test_ratio_motion_sampling_compilation_and_interruption(void)
{
    for (reach_easing easing :
         {REACH_EASING_EASE_IN, REACH_EASING_EASE_OUT, REACH_EASING_EASE_IN_OUT})
        for (bool reverse : {false, true})
        {
            reach_motion denominator = {}, numerator = {}, ratio = {};
            reach_motion_start_at(&denominator, reverse ? 2000 : 1800, reverse ? 1800 : 2000, .25,
                                  easing, 10);
            reach_motion_set(&numerator, 1920);
            expect(reach_motion_ratio(&numerator, &denominator, &ratio) == REACH_OK &&
                       ratio.generation == denominator.generation &&
                       ratio.started_seconds == denominator.started_seconds &&
                       ratio.duration_seconds == denominator.duration_seconds,
                   "ratio fitting preserves the canonical clock and generation");
            ratio = reach_motion_clamp(&ratio, 0, 1);
            auto height = reach_motion_affine(&ratio, -128, 128);
            for (double elapsed : {0.0, .07, .19})
            {
                reach_motion_segment pieces[REACH_MOTION_MAX_SEGMENTS] = {};
                size_t count = reach_motion_compile_at(&height, 10 + elapsed, pieces);
                expect(count > 0 && count <= REACH_MOTION_MAX_SEGMENTS,
                       "ordinary capacity ratio compiles within the existing native budget");
                for (size_t index = 0; index < count; ++index)
                    for (int step = 0; step <= 32; ++step)
                    {
                        const auto &piece = pieces[index];
                        double t = (piece.end - piece.start) * step / 32;
                        double now = 10 + elapsed + piece.start + t;
                        double compiled =
                            piece.value +
                            t * (piece.linear + t * (piece.quadratic + t * piece.cubic));
                        expect(fabs(compiled - reach_motion_value_at(&height, now)) < .00015,
                               "ratio native cubic follows analytic capacity fitting");
                    }
            }
            for (int step = 0; step <= 1000; ++step)
            {
                double now = 10 + .25 * step / 1000;
                float expected = fminf(1, 1920 / reach_motion_value_at(&denominator, now));
                expect(fabsf(reach_motion_value_at(&ratio, now) - expected) < .000001f,
                       "CPU ratio and clamping preserve exact fitting semantics");
            }
            double crossing = reach_motion_time_to_value_at(&ratio, .98f, 10);
            expect(crossing >= 0 && crossing <= .25 &&
                       fabsf(reach_motion_value_at(&ratio, 10 + crossing) - .98f) < .000001f,
                   "ratio crossings use the analytic timeline");
            float current = reach_motion_value_at(&ratio, 10.12);
            reach_motion_animate_to_at(&ratio, .7f, .2, easing, 10.12);
            expect(ratio.from == current && !ratio.has_mapping && ratio.started_seconds == 10.12 &&
                       ratio.duration_seconds == .2,
                   "ratio interruption starts ordinary intent at the exact visible sample");
        }
}

static void test_ratio_motion_affine_cancellation_compilation(void)
{
    for (bool reverse : {false, true})
    {
        reach_motion width = {}, available = {}, scale = {};
        reach_motion_start_at(&width, reverse ? 480 : 404.4f, reverse ? 404.4f : 480, .25,
                              REACH_EASING_EASE_IN_OUT, 10);
        reach_motion_set(&available, 384);
        expect(reach_motion_ratio(&available, &width, &scale) == REACH_OK,
               "narrow capacity fitting retains its exact reciprocal");
        auto position = reach_motion_affine(&scale, -96, 96);
        for (double elapsed : {0.0, .07, .19})
        {
            reach_motion_segment pieces[REACH_MOTION_MAX_SEGMENTS] = {};
            size_t count = reach_motion_compile_at(&position, 10 + elapsed, pieces);
            expect(count > 0 && count <= REACH_MOTION_MAX_SEGMENTS,
                   "bottom-anchor cancellation fits the existing native segment budget");
            for (size_t index = 0; index < count; ++index)
                for (int step = 0; step <= 32; ++step)
                {
                    const auto &piece = pieces[index];
                    double t = (piece.end - piece.start) * step / 32;
                    double now = 10 + elapsed + piece.start + t;
                    double compiled =
                        piece.value + t * (piece.linear + t * (piece.quadratic + t * piece.cubic));
                    expect(fabs(compiled - reach_motion_value_at(&position, now)) < .00015,
                           "bottom-anchor native pieces preserve analytic ratio precision");
                }
        }
    }
}

static void test_ratio_motion_preflight_and_identity(void)
{
    reach_motion clock = {}, numerator = {}, ratio = {}, previous = {}, other = {};
    reach_motion_start_at(&clock, 10800, 10856, .25, REACH_EASING_EASE_IN_OUT, 10);
    reach_motion_set(&numerator, 1920);
    expect(reach_motion_ratio(&numerator, &clock, &ratio) == REACH_OK,
           "192 item capacity fitting derives one reciprocal descriptor");
    float minimum = 0, maximum = 0;
    expect(reach_motion_range(&ratio, &minimum, &maximum) == REACH_OK &&
               fabsf(minimum - 1920.f / 10856) < .000001f &&
               fabsf(maximum - 1920.f / 10800) < .000001f,
           "ratio range includes the exact endpoint capacity bounds");
    for (reach_easing easing : {REACH_EASING_EASE_IN, REACH_EASING_EASE_OUT,
                                REACH_EASING_EASE_IN_OUT, REACH_EASING_EXPONENTIAL_OUT})
    {
        auto ordinary = clock;
        ordinary.easing = easing;
        reach_motion fitting = {};
        expect(reach_motion_ratio(&numerator, &ordinary, &fitting) == REACH_OK,
               "ordinary maximum-membership fitting keeps analytic ratio intent");
        auto height = reach_motion_affine(&fitting, -128, 128);
        reach_motion_segment pieces[REACH_MOTION_MAX_SEGMENTS] = {};
        expect(reach_motion_compile_at(&fitting, 10, pieces) > 0 &&
                   reach_motion_compile_at(&height, 10, pieces) > 0,
               "ordinary 192-item scale and height fit the native piece budget for every easing");
    }
    previous = ratio;
    auto active_clamp = reach_motion_clamp(&clock, 10810, 10850);
    expect(reach_motion_ratio(&numerator, &active_clamp, &ratio) == REACH_INVALID_ARGUMENT &&
               reach_motion_equal(&ratio, &previous),
           "ratio rejects active input clamps without mutating output");
    expect(reach_motion_ratio(&previous, &clock, &ratio) == REACH_INVALID_ARGUMENT &&
               reach_motion_equal(&ratio, &previous),
           "ratio rejects nested nonlinear mappings without mutation");
    reach_motion_start_at(&other, 1, 2, .25, REACH_EASING_EASE_IN_OUT, 10.01);
    expect(reach_motion_ratio(&other, &clock, &ratio) == REACH_INVALID_ARGUMENT &&
               reach_motion_equal(&ratio, &previous),
           "ratio rejects incompatible clocks without retargeting timing");
    expect(reach_motion_combine(&previous, 1, &clock, 1, 0, &ratio) == REACH_INVALID_ARGUMENT &&
               reach_motion_equal(&ratio, &previous),
           "unsupported mixed nonlinear composition rejects before mutation");
    auto invalid = clock;
    invalid.from = -1;
    expect(reach_motion_ratio(&numerator, &invalid, &ratio) == REACH_INVALID_ARGUMENT &&
               reach_motion_equal(&ratio, &previous),
           "ratio rejects a denominator crossing zero before mutation");
    auto changed = previous;
    changed.mapping_right_slope += 1;
    expect(!reach_motion_equal(&changed, &previous),
           "ratio denominator coefficients participate in retained intent identity");
}

static void test_quadratic_motion_geometry_and_compilation(void)
{
    for (reach_easing easing :
         {REACH_EASING_EASE_IN, REACH_EASING_EASE_OUT, REACH_EASING_EASE_IN_OUT})
        for (bool reverse : {false, true})
        {
            reach_motion extent = {}, reveal = {}, product = {}, x = {};
            reach_motion_start_at(&extent, reverse ? 600 : 200, reverse ? 200 : 600, .18, easing,
                                  10);
            reveal =
                reach_motion_affine(&extent, reverse ? -.0003f : .0003f, reverse ? 1.06f : .82f);
            expect(reach_motion_multiply(&extent, &reveal, &product) == REACH_OK &&
                       product.generation == extent.generation && product.started_seconds == 10,
                   "same-epoch multiplication preserves canonical geometry timing");
            auto position = reach_motion_affine(&extent, .5f, 30);
            expect(reach_motion_combine(&position, 1, &product, -.5f, 0, &x) == REACH_OK,
                   "quadratic extent and affine position combine without a second timeline");
            for (int step = 0; step <= 1000; ++step)
            {
                double now = 10 + .18 * step / 1000;
                float width = reach_motion_value_at(&extent, now);
                float scale = reach_motion_value_at(&reveal, now);
                expect(fabsf(reach_motion_value_at(&x, now) - (30 + .5f * width * (1 - scale))) <
                           .0001f,
                       "quadratic motion preserves centered reveal geometry");
            }
            for (double elapsed : {0.0, .03, .11})
            {
                reach_motion_segment pieces[REACH_MOTION_MAX_SEGMENTS] = {};
                size_t count = reach_motion_compile_at(&product, 10 + elapsed, pieces);
                expect(count > 0 && count <= REACH_MOTION_MAX_SEGMENTS,
                       "ordinary insertion fits the existing native segment budget");
                for (size_t at = 0; at < count; ++at)
                    for (int step = 0; step <= 32; ++step)
                    {
                        const auto &piece = pieces[at];
                        double t = (piece.end - piece.start) * step / 32;
                        double value = piece.value +
                                       t * (piece.linear + t * (piece.quadratic + t * piece.cubic));
                        expect(fabs(value - reach_motion_value_at(
                                                &product, 10 + elapsed + piece.start + t)) < .0003,
                               "native quadratic approximation agrees with analytic sampling");
                    }
            }
        }
}

static void test_quadratic_motion_extrema_crossings_and_rejection(void)
{
    reach_motion clock = {}, falling = {}, peak = {};
    reach_motion_start_at(&clock, 0, 1, 1, REACH_EASING_EASE_IN_OUT, 10);
    falling = reach_motion_affine(&clock, -1, 1);
    expect(reach_motion_multiply(&clock, &falling, &peak) == REACH_OK,
           "opposing affine inputs produce a nonmonotonic canonical motion");
    float minimum = 0, maximum = 0;
    expect(reach_motion_range(&peak, &minimum, &maximum) == REACH_OK && minimum == 0 &&
               maximum == .25f,
           "quadratic range includes the internal extremum");
    auto bounded = reach_motion_clamp(&peak, .1f, .2f);
    expect(reach_motion_range(&bounded, &minimum, &maximum) == REACH_OK && minimum == .1f &&
               maximum == .2f,
           "quadratic limits include both sides of the internal extremum");
    double first = reach_motion_time_to_value_at(&peak, .1875f, 10);
    double second = reach_motion_time_to_value_at(&peak, .1875f, 10.5);
    expect(first > 0 && first < .5 && second > 0 && second < .5 &&
               fabsf(reach_motion_value_at(&peak, 10 + first) - .1875f) < .000001f &&
               fabsf(reach_motion_value_at(&peak, 10.5 + second) - .1875f) < .000001f,
           "quadratic inverse selects the earliest future crossing on either side");
    reach_motion_segment pieces[REACH_MOTION_MAX_SEGMENTS] = {};
    expect(reach_motion_compile_at(&bounded, 10, pieces) > 0,
           "clamped nonmonotonic quadratic compiles both extrema and limit crossings");
    reach_motion previous = peak, other = clock;
    other.started_seconds += .01;
    expect(reach_motion_multiply(&clock, &other, &peak) == REACH_INVALID_ARGUMENT &&
               reach_motion_equal(&peak, &previous),
           "independent-epoch products reject without mutation");
    expect(reach_motion_multiply(&previous, &clock, &peak) == REACH_INVALID_ARGUMENT &&
               reach_motion_equal(&peak, &previous),
           "nested products reject degree growth without mutation");
    auto active_clamp = reach_motion_clamp(&clock, .2f, .8f);
    expect(reach_motion_multiply(&active_clamp, &clock, &peak) == REACH_INVALID_ARGUMENT &&
               reach_motion_equal(&peak, &previous),
           "products reject active input limits without mutation");
    reach_motion constant = {};
    reach_motion_set(&constant, 3);
    expect(reach_motion_multiply(&constant, &previous, &peak) == REACH_OK &&
               fabsf(reach_motion_value_at(&peak, 10.5) - .75f) < .000001f,
           "constant products preserve existing nonlinear intent through affine mapping");
}

int main(void)
{
    test_quadratic_motion_geometry_and_compilation();
    test_quadratic_motion_extrema_crossings_and_rejection();
    test_ratio_motion_sampling_compilation_and_interruption();
    test_ratio_motion_preflight_and_identity();
    test_ratio_motion_affine_cancellation_compilation();
    test_lookup_motion_sampling_compilation_and_retarget();
    test_lookup_motion_preflight_and_mapped_sources();
    test_composed_periodic_motion_preserves_phase();
    test_composed_motion_capacity_and_identity();
    test_composed_fitting_motion_preserves_constraints();
    test_hinge_motion_and_same_epoch_combination();
    test_peak_motion_compilation_and_crossings();
    test_motion_equality_preserves_identity_without_padding();
    test_motion_retarget_uses_one_event_time();
    test_motion_affine_preserves_the_physical_timeline();
    test_motion_crossing_preserves_endpoint_deadlines();
    test_bounded_motion_preserves_reveal_and_retarget();
    test_motion_compilation_matches_cpu_sampling();
    test_exponential_motion_compilation_and_projection();
    const uint16_t empty[] = {0};
    const uint16_t alpha[] = {'A', 'l', 'p', 'h', 'a', 0};
    const uint16_t alpha_copy[] = {'A', 'l', 'p', 'h', 'a', 0};
    const uint16_t alpha_lower[] = {'a', 'l', 'p', 'h', 'a', 0};
    const uint16_t alpha_short[] = {'A', 'l', 'p', 'h', 0};
    const uint16_t non_ascii[] = {0x00C9, 0};
    const uint16_t non_ascii_lower[] = {0x00E9, 0};

    expect(reach_strlen_utf16(nullptr) == 0, "null length");
    expect(reach_strlen_utf16(empty) == 0, "empty length");
    expect(reach_strlen_utf16(alpha) == 5, "text length");
    expect(reach_utf16_equal(nullptr, nullptr), "null equality");
    expect(!reach_utf16_equal(nullptr, empty), "one null inequality");
    expect(reach_utf16_equal(alpha, alpha_copy), "exact equality");
    expect(!reach_utf16_equal(alpha, alpha_lower), "exact case inequality");
    expect(!reach_utf16_equal(alpha, alpha_short), "exact length inequality");
    expect(reach_utf16_equal_ascii_case_insensitive(alpha, alpha_lower),
           "ASCII case-insensitive equality");
    expect(!reach_utf16_equal_ascii_case_insensitive(alpha, alpha_short),
           "ASCII case-insensitive length inequality");
    expect(!reach_utf16_equal_ascii_case_insensitive(non_ascii, non_ascii_lower),
           "non-ASCII remains exact");

    const uint16_t path_a[] = {'C', ':', '\\', 'A', 'p', 'p', '\\', 'X', '.', 'e', 'x', 'e', 0};
    const uint16_t path_b[] = {'c', ':', '/', 'a', 'P', 'P', '/', 'x', '.', 'E', 'X', 'E', 0};
    const uint16_t path_short[] = {'C', ':', '\\', 'A', 'p', 'p', 0};
    expect(reach_path_equals(path_a, path_b), "path case and separator equality");
    expect(!reach_path_equals(path_a, path_short), "path length inequality");
    expect(reach_path_equals(nullptr, nullptr), "null path equality");
    expect(!reach_path_equals(nullptr, path_a), "one null path inequality");

    uint16_t copy[6] = {};
    expect(reach_copy_utf16(copy, 6, alpha) == REACH_OK && reach_utf16_equal(copy, alpha),
           "UTF-16 exact copy");
    expect(reach_copy_utf16(copy, 5, alpha) == REACH_ERROR && copy[4] == 0,
           "UTF-16 truncated copy");
    expect(reach_copy_utf16(copy, 1, alpha) == REACH_ERROR && copy[0] == 0, "UTF-16 one-unit copy");
    expect(reach_copy_utf16(nullptr, 6, alpha) == REACH_INVALID_ARGUMENT,
           "UTF-16 invalid destination");

    expect(reach_copy_ascii_to_utf16(copy, 6, "Alpha") == REACH_OK &&
               reach_utf16_equal(copy, alpha),
           "ASCII exact copy");
    expect(reach_copy_ascii_to_utf16(copy, 5, "Alpha") == REACH_ERROR && copy[4] == 0,
           "ASCII truncated copy");
    expect(reach_copy_ascii_to_utf16(copy, 1, "Alpha") == REACH_ERROR && copy[0] == 0,
           "ASCII one-unit copy");
    expect(reach_copy_ascii_to_utf16(copy, 6, nullptr) == REACH_INVALID_ARGUMENT,
           "ASCII invalid source");

    return failed == 0 ? 0 : 1;
}
