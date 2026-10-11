#include "reach/support/util.h"
#include "reach/support/motion.h"

#include <math.h>
#include <stdio.h>

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

int main(void)
{
    test_motion_retarget_uses_one_event_time();
    test_motion_affine_preserves_the_physical_timeline();
    test_motion_crossing_preserves_endpoint_deadlines();
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
