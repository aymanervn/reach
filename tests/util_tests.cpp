#include "reach/core/theme.h"
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

static void test_lookup_range_preserves_first_threshold_extrema(void)
{
    const float thresholds[] = {10, 20, 30};
    for (int direction : {0, 1})
        for (float peak : {-1.f, 1.f})
        {
            reach_motion source = {}, steps = {};
            reach_motion_start_at(&source, direction ? 35.f : 5.f, direction ? 5.f : 35.f, 1,
                                  REACH_EASING_EASE_OUT, 10);
            const float values[] = {0, peak, 0, peak * .5f};
            expect(reach_motion_lookup(&source, thresholds, values, 3, &steps) == REACH_OK,
                   "range regression constructs first-threshold peak");
            float minimum = 71, maximum = 72;
            expect(reach_motion_range(&steps, &minimum, &maximum) == REACH_OK &&
                       minimum == fminf(0, peak) && maximum == fmaxf(0, peak),
                   "step range includes both sides of first threshold in either direction");
            auto clamped = reach_motion_clamp(&steps, -.25f, .25f);
            expect(reach_motion_range(&clamped, &minimum, &maximum) == REACH_OK &&
                       minimum == fmaxf(-.25f, fminf(0, peak)) &&
                       maximum == fminf(.25f, fmaxf(0, peak)),
                   "step range preserves first-threshold extrema through output limits");
            steps.from = direction ? 20.f : 10.f;
            steps.to = direction ? 10.f : 20.f;
            expect(reach_motion_range(&steps, &minimum, &maximum) == REACH_OK &&
                       minimum == fminf(0, peak) && maximum == fmaxf(0, peak),
                   "step range includes interior state when endpoints are threshold equalities");
            steps.mapping_extra_count = REACH_MOTION_MAX_EXTRA_KNOTS + 1;
            minimum = 71;
            maximum = 72;
            expect(reach_motion_range(&steps, &minimum, &maximum) == REACH_INVALID_ARGUMENT &&
                       minimum == 71 && maximum == 72,
                   "invalid step range leaves both outputs untouched");
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

static double spring_offset(double displacement, double velocity, double response, double damping,
                            double t)
{
    double omega = 2 * 3.14159265358979323846 / response;
    if (damping == 1)
        return (displacement + (velocity + omega * displacement) * t) * exp(-omega * t);
    double decay = damping * omega, frequency = omega * sqrt(1 - damping * damping);
    return exp(-decay * t) * (displacement * cos(frequency * t) +
                              (velocity + decay * displacement) / frequency * sin(frequency * t));
}

static void expect_spring_compiles_like_cpu(const reach_motion &motion, double now,
                                            double tolerance, const char *message)
{
    reach_motion_segment pieces[REACH_MOTION_MAX_SEGMENTS] = {};
    size_t count = reach_motion_compile_at(&motion, now, pieces);
    expect(count > 0 && count <= REACH_MOTION_MAX_SEGMENTS && pieces[0].start == 0 &&
               fabs(now + pieces[count - 1].end - reach_motion_deadline(&motion)) < 1e-9 &&
               pieces[count - 1].end_value ==
                   reach_motion_value_at(&motion, reach_motion_deadline(&motion)),
           message);
    for (size_t index = 0; index < count; ++index)
    {
        const reach_motion_segment &piece = pieces[index];
        expect(index == 0 || (piece.start == pieces[index - 1].end &&
                              piece.value == pieces[index - 1].end_value),
               message);
        double dt = piece.end - piece.start;
        for (int sample = 0; sample < 40; ++sample)
        {
            double at = dt * sample / 40;
            double value = piece.value + piece.linear * at + piece.quadratic * at * at +
                           piece.cubic * at * at * at;
            expect(fabs(value - reach_motion_value_at(&motion, now + piece.start + at)) <=
                       tolerance,
                   message);
        }
    }
}

static void test_spring_sampling_and_settle(void)
{
    const float dampings[] = {.5f, .7f, 1.f};
    for (float damping : dampings)
        for (float velocity : {0.f, 900.f, -400.f})
        {
            reach_motion motion = {};
            reach_motion_spring_start_at(&motion, 20, 120, velocity, .4f, damping, 10);
            expect(motion.easing == REACH_EASING_SPRING && motion.from == 20 && motion.to == 120 &&
                       motion.spring_velocity == velocity && motion.duration_seconds > 0 &&
                       reach_motion_value_at(&motion, 10) == 20 &&
                       reach_motion_velocity_at(&motion, 10) == velocity,
                   "spring starts at its event time with explicit value and velocity");
            for (double t : {.01, .05, .1, .2, .35, .5})
            {
                if (t >= motion.duration_seconds)
                    continue;
                double expected = 120 + spring_offset(-100, velocity, .4, damping, t);
                double rate = (spring_offset(-100, velocity, .4, damping, t + 1e-6) -
                               spring_offset(-100, velocity, .4, damping, t - 1e-6)) /
                              2e-6;
                expect(fabs(reach_motion_value_at(&motion, 10 + t) - expected) < 1e-3 &&
                           fabs(reach_motion_velocity_at(&motion, 10 + t) - rate) <
                               1e-3 * fabs(rate) + 1e-2,
                       "spring samples analytic position and velocity");
            }
            double deadline = reach_motion_deadline(&motion);
            double tolerance = .05;
            expect(deadline == 10 + motion.duration_seconds &&
                       reach_motion_active_at(&motion, deadline - 1e-6) &&
                       !reach_motion_active_at(&motion, deadline) &&
                       reach_motion_value_at(&motion, deadline) == 120.0f &&
                       reach_motion_velocity_at(&motion, deadline) == 0,
                   "spring settles at a finite deadline on the exact target");
            for (int sample = 0; sample <= 400; ++sample)
            {
                double t = motion.duration_seconds + sample * .005;
                expect(fabs(spring_offset(-100, velocity, .4, damping, t)) <= tolerance + 1e-9,
                       "spring residual after the deadline stays visually nil");
            }
        }
    reach_motion critical = {};
    reach_motion_spring_start_at(&critical, 0, 4000, 0, .3f, 1, 0);
    reach_motion bouncy = {};
    reach_motion_spring_start_at(&bouncy, 0, 4000, 0, .3f, .6f, 0);
    expect(critical.duration_seconds < .9 && bouncy.duration_seconds < 1.5 &&
               fabs(spring_offset(-4000, 0, .3, 1, critical.duration_seconds)) <= .05 + 1e-9,
           "large spans settle below a twentieth of a unit without an unbounded tail");
    reach_motion invalid = critical;
    uint64_t generation = invalid.generation;
    reach_motion_spring_start_at(&invalid, 0, 50, 0, .3f, .4f, 1);
    expect(invalid.easing != REACH_EASING_SPRING && reach_motion_value_at(&invalid, 1) == 50 &&
               invalid.generation == generation + 1,
           "unsupported spring damping settles immediately like any invalid timing");
    reach_motion_repeat_at(&invalid, 0, 10, .3, .1, .1, REACH_EASING_SPRING, 2);
    expect(!invalid.repeating && reach_motion_value_at(&invalid, 2.2) == 10,
           "repeating motion rejects springs");
}

static void test_spring_without_travel_moves_and_settles(void)
{
    reach_motion motion = {};
    reach_motion_spring_start_at(&motion, 50, 50, 600, .35f, .8f, 3);
    float minimum = 0, maximum = 0;
    expect(reach_motion_active_at(&motion, 3.05) && reach_motion_value_at(&motion, 3.05) > 55 &&
               reach_motion_range(&motion, &minimum, &maximum) == REACH_OK && maximum > 60 &&
               minimum < 50 && reach_motion_value_at(&motion, reach_motion_deadline(&motion)) == 50,
           "equal endpoints with velocity still travel, overshoot and settle exactly");
    reach_motion_spring_start_at(&motion, 50, 50, 0, .35f, .8f, 3);
    expect(!reach_motion_active_at(&motion, 3) && reach_motion_value_at(&motion, 3) == 50,
           "equal endpoints without velocity are static");
    reach_motion_spring_start_at(&motion, 50, 50, -600, .35f, 1, 3);
    expect_spring_compiles_like_cpu(motion, 3, .1, "zero-span spring compiles like CPU");
    reach_motion hinge = {};
    expect(reach_motion_hinge(&motion, 45, 0, -1, 2, &hinge) == REACH_OK &&
               reach_motion_active_at(&hinge, 3.05) &&
               fabsf(reach_motion_value_at(&hinge, 3.05) -
                     fmaxf(0, -(reach_motion_value_at(&motion, 3.05) - 45))) < 1e-4f,
           "zero-span springs keep moving through piecewise mappings");
    expect_spring_compiles_like_cpu(hinge, 3.01, .1, "mapped zero-span spring compiles");
}

static void test_spring_retarget_preserves_velocity(void)
{
    for (int easing = REACH_EASING_EASE_IN; easing <= REACH_EASING_SPRING; ++easing)
    {
        reach_motion source = {};
        if (easing == REACH_EASING_SPRING)
            reach_motion_spring_start_at(&source, 0, 300, 0, .45f, .55f, 5);
        else
            reach_motion_start_at(&source, 0, 300, .8, (reach_easing)easing, 5);
        reach_motion bounded = reach_motion_clamp(&source, 0, 280);
        reach_motion mapped = {};
        expect(reach_motion_hinge(&source, 150, 150, .5f, 2, &mapped) == REACH_OK,
               "retarget source maps");
        for (const reach_motion &current : {source, bounded, mapped})
            for (double t : {5.05, 5.2, 5.37})
            {
                float value = reach_motion_value_at(&current, t);
                float velocity = reach_motion_velocity_at(&current, t);
                double slope = (reach_motion_value_at(&current, t + 1e-4) -
                                reach_motion_value_at(&current, t - 1e-4)) /
                               2e-4;
                expect(fabs(velocity - slope) < .01 * fabs(slope) + 2,
                       "velocity matches the sampled slope for every curve");
                reach_motion next = current;
                uint64_t generation = next.generation;
                reach_motion_spring_to_at(&next, -40, .3f, .7f, t);
                expect(next.easing == REACH_EASING_SPRING && next.from == value &&
                           next.spring_velocity == velocity && next.started_seconds == t &&
                           next.generation == generation + 1 && !next.has_mapping &&
                           !next.has_limits && reach_motion_value_at(&next, t) == value &&
                           reach_motion_velocity_at(&next, t) == velocity,
                       "spring retarget preserves value and velocity at one event time");
                double after = (reach_motion_value_at(&next, t + 3e-5) - value) / 3e-5;
                expect(fabs(after - velocity) < .01 * fabs(velocity) + 5,
                       "retargeted motion leaves with the interrupted velocity");
            }
    }
    reach_motion spec_motion = {};
    reach_motion_start_spec_at(&spec_motion, 0, 10, 40, reach_motion_spec_spring(.3f, .8f), 2);
    reach_motion direct = {};
    reach_motion_spring_start_at(&direct, 0, 10, 40, .3f, .8f, 2);
    direct.generation = spec_motion.generation;
    expect(reach_motion_equal(&spec_motion, &direct), "spring specs start canonical springs");
    reach_motion_animate_spec_to_at(&spec_motion, 4,
                                    reach_motion_spec_curve(.2, REACH_EASING_EASE_OUT), 2.1);
    expect(spec_motion.easing == REACH_EASING_EASE_OUT && spec_motion.duration_seconds == .2 &&
               spec_motion.to == 4,
           "curve specs keep legacy retargeting");
    direct.spring_velocity += 1;
    expect(!reach_motion_equal(&spec_motion, &direct), "equality compares spring fields");
}

static void test_spring_range_and_first_future_crossing(void)
{
    reach_motion motion = {};
    reach_motion_spring_start_at(&motion, 0, 100, 0, .4f, .5f, 0);
    double omega = 2 * 3.14159265358979323846 / .4, decay = .5 * omega;
    double frequency = omega * sqrt(.75);
    float minimum = 0, maximum = 0;
    double overshoot = 100 + 100 * exp(-decay * 3.14159265358979323846 / frequency);
    expect(reach_motion_range(&motion, &minimum, &maximum) == REACH_OK && minimum == 0 &&
               fabs(maximum - overshoot) < 1e-3,
           "range includes the analytic overshoot extremum");
    reach_motion critical = {};
    reach_motion_spring_start_at(&critical, 0, 100, 0, .4f, 1, 0);
    expect(reach_motion_range(&critical, &minimum, &maximum) == REACH_OK && minimum == 0 &&
               maximum == 100,
           "critically damped range stays between its endpoints");
    double first = (3.14159265358979323846 - atan(frequency / decay)) / frequency;
    double second = first + 3.14159265358979323846 / frequency;
    expect(fabs(reach_motion_time_to_value_at(&motion, 100, 0) - first) < 1e-6,
           "crossing query finds the first overshooting arrival");
    double peak = 3.14159265358979323846 / frequency;
    expect(fabs(reach_motion_time_to_value_at(&motion, 100, peak) - (second - peak)) < 1e-6 &&
               fabs(reach_motion_time_to_value_at(&motion, 50, .01) -
                    reach_motion_time_to_value_at(&motion, 50, 0) + .01) < 1e-6,
           "crossing query returns the first future crossing after a turn");
    double crossing = reach_motion_time_to_value_at(&motion, 108, peak * .5);
    expect(crossing > 0 && fabsf(reach_motion_value_at(&motion, peak * .5 + crossing) - 108) < 1e-3,
           "overshoot values are reachable crossings");
    expect(fabs(reach_motion_time_to_value_at(&motion, 140, 0) - motion.duration_seconds) < 1e-12,
           "unreached values wait for the spring deadline");
    reach_motion clamped = reach_motion_clamp(&motion, 0, 100);
    expect(reach_motion_range(&clamped, &minimum, &maximum) == REACH_OK && maximum == 100 &&
               fabs(reach_motion_time_to_value_at(&clamped, 100, 0) - first) < 1e-6,
           "clamped springs saturate at the first crossing");
}

static void test_spring_derived_motion(void)
{
    reach_motion spring = {};
    reach_motion_spring_start_at(&spring, 10, 410, 1500, .35f, .55f, 20);
    reach_motion width = reach_motion_affine(&spring, -.5f, 300);
    reach_motion clamped = reach_motion_clamp(&spring, 50, 400);
    reach_motion hinge = {}, peak = {}, combined = {}, smaller = {}, product = {}, quotient = {},
                 lookup = {};
    float thresholds[] = {100, 405};
    float values[] = {0, 1, 2};
    reach_motion constant = {};
    reach_motion_set(&constant, 3);
    reach_motion progress = reach_motion_affine(&spring, .0025f, 0);
    reach_motion mixed = {};
    reach_motion share = reach_motion_affine(&width, 1 / 300.f, 0);
    expect(reach_motion_hinge(&spring, 200, 20, .5f, 1.5f, &hinge) == REACH_OK &&
               reach_motion_peak(&spring, 400, 30, &peak) == REACH_OK &&
               reach_motion_combine(&spring, 1, &width, 1, 5, &combined) == REACH_OK &&
               reach_motion_min(&spring, &width, &smaller) == REACH_OK &&
               reach_motion_multiply(&progress, &share, &product) == REACH_OK &&
               reach_motion_ratio(&constant, &width, &quotient) == REACH_OK &&
               reach_motion_lookup(&spring, thresholds, values, 2, &lookup) == REACH_OK &&
               reach_motion_combine(&spring, 1, &product, 100, 0, &mixed) == REACH_OK,
           "bouncing springs support every canonical derivation");
    for (double t : {20.03, 20.11, 20.24, 20.4, 20.7})
    {
        float a = reach_motion_value_at(&spring, t), b = reach_motion_value_at(&width, t);
        expect(fabsf(b - (300 - .5f * a)) < 1e-3f &&
                   fabsf(reach_motion_value_at(&clamped, t) - fminf(400, fmaxf(50, a))) < 1e-3f &&
                   fabsf(reach_motion_value_at(&combined, t) - (a + b + 5)) < 1e-2f &&
                   fabsf(reach_motion_value_at(&smaller, t) - fminf(a, b)) < 1e-2f &&
                   fabsf(reach_motion_value_at(&product, t) - a * .0025f * b / 300) < 1e-5f &&
                   fabsf(reach_motion_value_at(&quotient, t) - 3 / b) < 1e-5f &&
                   fabsf(reach_motion_value_at(&mixed, t) -
                         (a + 100 * reach_motion_value_at(&product, t))) < 1e-2f &&
                   reach_motion_value_at(&lookup, t) == (a >= 405   ? 2
                                                         : a >= 100 ? 1
                                                                    : 0) &&
                   fabsf(reach_motion_value_at(&peak, t) - fmaxf(0, 1 - fabsf(a - 400) / 30)) <
                       1e-4f,
               "derived springs sample the same overshooting source");
    }
    float minimum = 0, maximum = 0;
    expect(reach_motion_range(&lookup, &minimum, &maximum) == REACH_OK && maximum == 2 &&
               reach_motion_range(&peak, &minimum, &maximum) == REACH_OK && maximum == 1,
           "derived ranges include values visited only by overshoot");
    const reach_motion derived[] = {width,   clamped, hinge,    peak,  combined,
                                    smaller, product, quotient, lookup};
    const double tolerances[] = {.1, .1, .1, .005, .1, .1, .005, 1e-4, 1.01};
    for (size_t index = 0; index < sizeof(derived) / sizeof(derived[0]); ++index)
        expect_spring_compiles_like_cpu(derived[index], 20.02, tolerances[index],
                                        "derived spring native curves follow CPU sampling");
    double first = reach_motion_time_to_value_at(&lookup, 2, 20);
    expect(first > 0 && reach_motion_value_at(&lookup, 20 + first + 1e-6) == 2 &&
               reach_motion_value_at(&lookup, 20 + first - 1e-4) == 1,
           "discrete springs cross their overshoot threshold once it is reached");
}

static void test_spring_rejection_preserves_output(void)
{
    reach_motion spring = {}, other = {}, sentinel = {}, output = {};
    reach_motion_spring_start_at(&spring, 0, 100, 0, .3f, .7f, 1);
    other = spring;
    other.spring_damping = .9f;
    reach_motion_set(&sentinel, 77);
    output = sentinel;
    expect(reach_motion_combine(&spring, 1, &other, 1, 0, &output) == REACH_INVALID_ARGUMENT &&
               reach_motion_min(&spring, &other, &output) == REACH_INVALID_ARGUMENT &&
               reach_motion_equal(&output, &sentinel),
           "incompatible spring epochs reject without output mutation");
    reach_motion still = {};
    reach_motion_spring_start_at(&still, 40, 40, 300, .3f, .7f, 1);
    reach_motion moving = still;
    moving.from = 20;
    moving.spring_velocity = 0;
    expect(reach_motion_multiply(&still, &still, &output) == REACH_INVALID_ARGUMENT &&
               reach_motion_ratio(&still, &still, &output) == REACH_INVALID_ARGUMENT &&
               reach_motion_combine(&still, 1, &moving, 1, 0, &output) == REACH_INVALID_ARGUMENT &&
               reach_motion_equal(&output, &sentinel),
           "zero-span springs reject normalized products without output mutation");
    reach_motion broken = spring;
    broken.spring_damping = .2f;
    float minimum = -1, maximum = -1;
    expect(reach_motion_hinge(&broken, 50, 0, 1, 1, &output) == REACH_INVALID_ARGUMENT &&
               reach_motion_range(&broken, &minimum, &maximum) == REACH_INVALID_ARGUMENT &&
               minimum == -1 && reach_motion_equal(&output, &sentinel),
           "invalid spring descriptors reject without output mutation");
    broken = spring;
    broken.repeating = 1;
    reach_motion_segment pieces[REACH_MOTION_MAX_REPEAT_SEGMENTS] = {};
    expect(reach_motion_peak(&broken, 50, 10, &output) == REACH_INVALID_ARGUMENT &&
               reach_motion_compile_repeat_at(&broken, 1, pieces) == 0 &&
               reach_motion_equal(&output, &sentinel),
           "repeating springs are rejected");
}

static void test_spring_settle_is_relative_to_amplitude(void)
{
    reach_motion unit = {};
    reach_motion_spring_start_at(&unit, 0, 1, 0, .3f, .7f, 4);
    reach_motion pixels = reach_motion_affine(&unit, 1000, 0);
    double deadline = reach_motion_deadline(&pixels);
    expect(fabsf(reach_motion_value_at(&pixels, deadline - 1e-9) - 1000) <= .1f &&
               reach_motion_value_at(&pixels, deadline) == 1000,
           "amplified springs snap at most a tenth of a unit at the deadline");
    expect_spring_compiles_like_cpu(pixels, 4, .1, "amplified springs compile within a tenth");
    for (float damping : {1.f, .85f, .6f})
        for (float velocity : {0.f, 2.f})
        {
            reach_motion near = {}, far = {};
            reach_motion_spring_start_at(&near, 0, 300, velocity * 300, .4f, damping, 7);
            reach_motion_spring_start_at(&far, 0, 3000, velocity * 3000, .4f, damping, 7);
            expect(near.duration_seconds > 0 && near.duration_seconds == far.duration_seconds,
                   "proportional springs share one settle duration across spans");
        }
    reach_motion small = {}, large = {}, output = {};
    reach_motion_spring_start_at(&small, 0, 300, 0, .35f, 1, 2);
    reach_motion_spring_start_at(&large, 0, 3000, 0, .35f, 1, 2);
    expect(reach_motion_combine(&small, 1, &large, -.1f, 5, &output) == REACH_OK &&
               fabsf(reach_motion_value_at(&output, 2.1) -
                     (reach_motion_value_at(&small, 2.1) -
                      .1f * reach_motion_value_at(&large, 2.1) + 5)) < 1e-3f &&
               reach_motion_min(&small, &large, &output) == REACH_OK &&
               reach_motion_value_at(&output, 2.1) == reach_motion_value_at(&small, 2.1) &&
               reach_motion_max(&small, &large, &output) == REACH_OK &&
               reach_motion_value_at(&output, 2.1) == reach_motion_value_at(&large, 2.1),
           "critically damped springs with different spans combine as one epoch");
}

static void test_spring_epochs_ignore_velocity_rounding(void)
{
    for (float response : {.15f, .3f, .45f, .7f})
        for (float damping : {.5f, .75f, .85f, 1.f})
            for (float ratio : {.4f, -.7f, 1.f})
            {
                float omega = 6.2831853f / response;
                reach_motion near = {}, far = {}, output = {};
                reach_motion_spring_start_at(&near, 0, 300, ratio * 300 * omega, response, damping,
                                             2);
                reach_motion_spring_start_at(&far, 0, 3000, ratio * 3000 * omega, response, damping,
                                             2);
                double t = 2 + response * .3;
                float a = reach_motion_value_at(&near, t), b = reach_motion_value_at(&far, t);
                expect(near.duration_seconds == far.duration_seconds &&
                           reach_motion_combine(&near, 1, &far, -.1f, 5, &output) == REACH_OK &&
                           fabsf(reach_motion_value_at(&output, t) - (a - .1f * b + 5)) < 1e-2f &&
                           reach_motion_min(&near, &far, &output) == REACH_OK &&
                           fabsf(reach_motion_value_at(&output, t) - fminf(a, b)) < 1e-2f &&
                           reach_motion_max(&near, &far, &output) == REACH_OK &&
                           fabsf(reach_motion_value_at(&output, t) - fmaxf(a, b)) < 1e-2f,
                       "springs with float-proportional velocities combine as one epoch");
            }
    reach_motion small = {}, large = {}, output = {};
    reach_motion_start_at(&small, 0, 100, .5, REACH_EASING_EASE_OUT, 1);
    reach_motion_start_at(&large, 0, 1000, .5, REACH_EASING_EASE_OUT, 1);
    reach_motion_spring_to_at(&small, 0, .35f, 1, 1.17);
    reach_motion_spring_to_at(&large, 0, .35f, 1, 1.17);
    for (double t : {1.2, 1.4, 1.7})
    {
        float a = reach_motion_value_at(&small, t), b = reach_motion_value_at(&large, t);
        expect(small.duration_seconds == large.duration_seconds &&
                   reach_motion_combine(&small, 1, &large, 1, 0, &output) == REACH_OK &&
                   fabsf(reach_motion_value_at(&output, t) - (a + b)) < 1e-2f &&
                   reach_motion_min(&small, &large, &output) == REACH_OK &&
                   fabsf(reach_motion_value_at(&output, t) - fminf(a, b)) < 1e-2f &&
                   reach_motion_max(&small, &large, &output) == REACH_OK &&
                   fabsf(reach_motion_value_at(&output, t) - fmaxf(a, b)) < 1e-2f,
               "same-time spring retargets of different ease-outs combine");
    }
}

static void test_spring_epochs_tolerate_coordinate_rounding(void)
{
    uint32_t seed = 12345;
    auto next = [&seed](double low, double high)
    {
        seed = seed * 1664525u + 1013904223u;
        return low + (high - low) * (seed >> 8) / 16777216.0;
    };
    size_t edges = 0, pairs = 0;
    for (int index = 0; index < 4000; ++index)
    {
        float x0 = (float)next(1700, 3800), width = (float)next(-1500, 1500);
        float dx = (float)next(-400, 400);
        double duration = next(.2, .6), at = 3 + duration * next(0, .02);
        float target = index & 1 ? 0 : dx + (float)next(-50, 50);
        reach_motion left = {}, right = {}, output = {};
        reach_motion_start_at(&left, x0, x0 + dx, duration, REACH_EASING_EASE_OUT, 3);
        reach_motion_start_at(&right, x0 + width, x0 + width + dx, duration, REACH_EASING_EASE_OUT,
                              3);
        reach_motion_spring_to_at(&left, x0 + target, .35f, 1, at);
        reach_motion_spring_to_at(&right, x0 + width + target, .35f, 1, at);
        double t = at + .05;
        float a = reach_motion_value_at(&left, t), b = reach_motion_value_at(&right, t);
        edges += reach_motion_combine(&left, 1, &right, -1, 0, &output) == REACH_OK &&
                 fabsf(reach_motion_value_at(&output, t) - (a - b)) < 1e-2f &&
                 reach_motion_min(&left, &right, &output) == REACH_OK &&
                 fabsf(reach_motion_value_at(&output, t) - fminf(a, b)) < 1e-2f &&
                 reach_motion_max(&left, &right, &output) == REACH_OK &&
                 fabsf(reach_motion_value_at(&output, t) - fmaxf(a, b)) < 1e-2f;
    }
    expect(edges == 4000, "translated edge-pair spring retargets combine at large coordinates");
    for (int index = 0; index < 4000; ++index)
    {
        float response = (float)next(.15, .7), span = (float)next(-10, 10);
        float velocity = (float)(next(-1, 1) * span * 6.2831853 / response);
        float first = (float)next(-4000, 4000), second = (float)next(-4000, 4000);
        reach_motion a = {}, b = {}, output = {};
        reach_motion_spring_start_at(&a, first, first + span, velocity, response, 1, 5);
        reach_motion_spring_start_at(&b, second, second + span, velocity, response, 1, 5);
        double t = 5 + response * .4;
        float av = reach_motion_value_at(&a, t), bv = reach_motion_value_at(&b, t);
        pairs += reach_motion_combine(&a, 1, &b, -1, 0, &output) == REACH_OK &&
                 reach_motion_min(&a, &b, &output) == REACH_OK &&
                 fabsf(reach_motion_value_at(&output, t) - fminf(av, bv)) < 1e-2f &&
                 reach_motion_max(&a, &b, &output) == REACH_OK &&
                 fabsf(reach_motion_value_at(&output, t) - fmaxf(av, bv)) < 1e-2f;
    }
    expect(pairs == 4000, "critically damped pairs at large bases with small spans combine");
    for (float span : {5.f, 500.f})
        for (float base : {3000.f, -3999.f})
        {
            reach_motion a = {}, b = {}, sentinel = {}, output = {};
            reach_motion_spring_start_at(&a, base, base + span, span * 10, .35f, 1, 5);
            reach_motion_spring_start_at(&b, base + 7, base + 7 + span, span * 15, .35f, 1, 5);
            reach_motion_set(&sentinel, 9);
            output = sentinel;
            expect(reach_motion_combine(&a, 1, &b, -1, 0, &output) == REACH_INVALID_ARGUMENT &&
                       reach_motion_min(&a, &b, &output) == REACH_INVALID_ARGUMENT &&
                       reach_motion_equal(&output, &sentinel),
                   "springs with genuinely different shapes still reject");
        }
}

static void test_spring_native_compile_proof(void)
{
    size_t peak = 0;
    double worst = 0;
    for (float span : {1.f, 72.f, 1000.f, 4000.f})
        for (float response : {.15f, .25f, .35f, .5f, .7f})
            for (float damping : {.5f, .6f, .75f, .9f, 1.f})
                for (int reverse : {0, 1})
                    for (float boost : {0.f, .5f})
                    {
                        float from = reverse ? span : 0, to = reverse ? 0 : span;
                        reach_motion motion = {};
                        reach_motion_spring_start_at(&motion, from, to,
                                                     boost * (to - from) * 6.2831853f / response,
                                                     response, damping, 10);
                        for (double elapsed : {0., .03})
                        {
                            reach_motion_segment pieces[REACH_MOTION_MAX_SEGMENTS] = {};
                            size_t count = reach_motion_compile_at(&motion, 10 + elapsed, pieces);
                            expect(count > 0 && count <= REACH_MOTION_MAX_SEGMENTS &&
                                       pieces[count - 1].end_value == to &&
                                       fabs(10 + elapsed + pieces[count - 1].end -
                                            reach_motion_deadline(&motion)) < 1e-9,
                                   "spring native compile fits the supported grid");
                            peak = count > peak ? count : peak;
                            for (size_t index = 0; index < count; ++index)
                            {
                                const reach_motion_segment &piece = pieces[index];
                                expect(index == 0 || (piece.start == pieces[index - 1].end &&
                                                      piece.value == pieces[index - 1].end_value),
                                       "spring native pieces have consistent junctions");
                                double dt = piece.end - piece.start;
                                double direction = piece.end_value - piece.value;
                                double previous = piece.value;
                                for (int sample = 0; sample <= 48; ++sample)
                                {
                                    double at = dt * sample / 48;
                                    double value = piece.value + piece.linear * at +
                                                   piece.quadratic * at * at +
                                                   piece.cubic * at * at * at;
                                    double cpu = reach_motion_value_at(
                                        &motion, 10 + elapsed + piece.start + at -
                                                     (sample == 48 ? 1e-9 : 0));
                                    worst = fmax(worst, fabs(value - cpu));
                                    expect((direction >= 0 ? previous - value : value - previous) <
                                               1e-3,
                                           "spring native pieces are monotone");
                                    previous = value;
                                }
                            }
                        }
                    }
    expect(peak <= REACH_MOTION_MAX_SEGMENTS && worst <= .1,
           "spring native compile stays within capacity and a tenth of a unit");
    reach_motion sliver = {};
    reach_motion_spring_start_at(&sliver, 0, 1000, -9666.44f, .325f, .875f, 10);
    expect_spring_compiles_like_cpu(sliver, 10.2, .1, "sliver pieces never force the CPU fallback");
    reach_motion wide = {};
    reach_motion_spring_start_at(&wide, 0, 4000, -400000, .2f, .5f, 10);
    reach_motion oversized = {};
    reach_motion_spring_start_at(&oversized, -3e38f, 3e38f, 0, .3f, .8f, 10);
    reach_motion_segment pieces[REACH_MOTION_MAX_SEGMENTS] = {};
    expect(reach_motion_compile_at(&wide, 10, pieces) == 0 &&
               isfinite(reach_motion_value_at(&wide, 10.2)) && reach_motion_deadline(&wide) > 10 &&
               isfinite(reach_motion_deadline(&wide)) &&
               reach_motion_compile_at(&oversized, 10, pieces) == 0 &&
               isfinite(reach_motion_value_at(&oversized, 10.1)),
           "exhausted spring budgets keep the finite CPU fallback");
}

static void test_theme_motion_presets(void)
{
    struct expected_preset
    {
        reach_motion_preset preset;
        reach_easing easing;
        float response;
        float damping;
        double duration;
    };
    const expected_preset table[] = {
        {REACH_MOTION_PRESET_SNAPPY, REACH_EASING_SPRING, .24f, 1, 0},
        {REACH_MOTION_PRESET_SMOOTH, REACH_EASING_SPRING, .38f, 1, 0},
        {REACH_MOTION_PRESET_BOUNCY, REACH_EASING_SPRING, .34f, .78f, 0},
        {REACH_MOTION_PRESET_PRESS, REACH_EASING_SPRING, .16f, 1, 0},
        {REACH_MOTION_PRESET_RELEASE, REACH_EASING_SPRING, .30f, .72f, 0},
        {REACH_MOTION_PRESET_FADE_IN, REACH_EASING_EASE_OUT, 0, 0, .14},
        {REACH_MOTION_PRESET_FADE_OUT, REACH_EASING_EASE_OUT, 0, 0, .11},
        {REACH_MOTION_PRESET_MENU_FADE, REACH_EASING_EASE_OUT, 0, 0, .09},
    };
    expect(sizeof(table) / sizeof(table[0]) == REACH_MOTION_PRESET_COUNT,
           "every motion preset has an expected value");
    for (const expected_preset &entry : table)
        for (reach_theme_mode mode : {REACH_THEME_MODE_DARK, REACH_THEME_MODE_LIGHT})
        {
            reach_motion_spec spec =
                reach_theme_motion_preset(reach_theme_for_mode(mode), entry.preset);
            expect(spec.easing == entry.easing && spec.response_seconds == entry.response &&
                       spec.damping == entry.damping && spec.duration_seconds == entry.duration,
                   "theme motion presets resolve to their named spec");
        }
    reach_motion_spec smooth =
        reach_theme_motion_preset(reach_theme_default(), REACH_MOTION_PRESET_SMOOTH);
    reach_motion_spec fallback = reach_theme_motion_preset(nullptr, REACH_MOTION_PRESET_COUNT);
    expect(fallback.easing == smooth.easing &&
               fallback.response_seconds == smooth.response_seconds &&
               fallback.damping == smooth.damping,
           "unknown presets and a missing theme fall back to smooth");
}

static void test_motion_preset_scaling(void)
{
    const reach_theme *theme = reach_theme_default();
    reach_motion_spec bouncy = reach_theme_motion_preset(theme, REACH_MOTION_PRESET_BOUNCY);
    reach_motion_spec fade = reach_theme_motion_preset(theme, REACH_MOTION_PRESET_FADE_OUT);
    reach_motion_spec slow = reach_motion_spec_scaled(bouncy, 2);
    reach_motion_spec quick = reach_motion_spec_scaled(fade, .5f);
    expect(slow.easing == REACH_EASING_SPRING &&
               slow.response_seconds == bouncy.response_seconds * 2 &&
               slow.damping == bouncy.damping && quick.easing == fade.easing &&
               quick.duration_seconds == fade.duration_seconds * .5f,
           "time scale multiplies spring response and fade duration only");
    for (float invalid : {0.f, -1.f, NAN, INFINITY})
    {
        reach_motion_spec same = reach_motion_spec_scaled(bouncy, invalid);
        expect(same.response_seconds == bouncy.response_seconds && same.damping == bouncy.damping,
               "invalid time scales leave presets unchanged");
    }
    reach_motion base = {}, scaled = {};
    reach_motion_start_spec_at(&base, 0, 100, 0, bouncy, 1);
    reach_motion_start_spec_at(&scaled, 0, 100, 0, slow, 1);
    expect(fabs(scaled.duration_seconds - 2 * base.duration_seconds) <=
               1e-6 * base.duration_seconds,
           "scaled spring settle time stays derived by the engine");
    expect(reach_theme_stage_motion_scale(theme, theme->stage_motion_reference_seconds) == 1 &&
               fabsf(reach_theme_stage_motion_scale(theme, .345f) - .345f / .28f) < 1e-6f &&
               reach_theme_stage_motion_scale(theme, 0) == 1 &&
               reach_theme_stage_motion_scale(theme, -.2f) == 1 &&
               reach_theme_stage_motion_scale(nullptr, .56f) == 2,
           "the persisted Stage duration scales presets relative to its reference");
}

static void test_motion_presets_compile_natively(void)
{
    const reach_theme *theme = reach_theme_default();
    size_t failures = 0, peak = 0;
    for (int index = 0; index < REACH_MOTION_PRESET_COUNT; ++index)
        for (float scale : {.5f, 1.f, .345f / .28f, 2.f})
        {
            reach_motion_spec spec = reach_motion_spec_scaled(
                reach_theme_motion_preset(theme, (reach_motion_preset)index), scale);
            for (float span : {1.f, 10.f, 72.f, 300.f, 1000.f, 2500.f, 4000.f})
                for (int reverse : {0, 1})
                    for (float boost : {0.f, .5f})
                    {
                        float from = reverse ? span : 0, to = reverse ? 0 : span;
                        float velocity =
                            spec.easing == REACH_EASING_SPRING
                                ? boost * (to - from) * 6.2831853f / spec.response_seconds
                                : 0;
                        reach_motion motion = {};
                        reach_motion_start_spec_at(&motion, from, to, velocity, spec, 10);
                        for (double elapsed : {0., .016})
                        {
                            reach_motion_segment pieces[REACH_MOTION_MAX_SEGMENTS] = {};
                            size_t count = reach_motion_compile_at(&motion, 10 + elapsed, pieces);
                            peak = count > peak ? count : peak;
                            failures += !(count > 0 && count <= REACH_MOTION_MAX_SEGMENTS &&
                                          pieces[count - 1].end_value == to);
                        }
                    }
        }
    expect(failures == 0 && peak <= REACH_MOTION_MAX_SEGMENTS,
           "every scaled motion preset compiles natively up to a 4000 span");
}

int main(void)
{
    test_spring_sampling_and_settle();
    test_spring_without_travel_moves_and_settles();
    test_spring_retarget_preserves_velocity();
    test_spring_range_and_first_future_crossing();
    test_spring_derived_motion();
    test_spring_rejection_preserves_output();
    test_spring_settle_is_relative_to_amplitude();
    test_spring_epochs_ignore_velocity_rounding();
    test_spring_epochs_tolerate_coordinate_rounding();
    test_spring_native_compile_proof();
    test_theme_motion_presets();
    test_motion_preset_scaling();
    test_motion_presets_compile_natively();
    test_quadratic_motion_geometry_and_compilation();
    test_quadratic_motion_extrema_crossings_and_rejection();
    test_ratio_motion_sampling_compilation_and_interruption();
    test_ratio_motion_preflight_and_identity();
    test_ratio_motion_affine_cancellation_compilation();
    test_lookup_motion_sampling_compilation_and_retarget();
    test_lookup_motion_preflight_and_mapped_sources();
    test_lookup_range_preserves_first_threshold_extrema();
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
