#include "reach/core/frame_clock.h"

#include <cmath>
#include <stdio.h>

static int failures;

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        ++failures;
        printf("FAIL: %s\n", message);
    }
}

static void test_frame_rate_policy(void)
{
    expect_true(reach_frame_rate_resolve(1, 240, 0) == 240, "adaptive has no 120 Hz cap");
    expect_true(reach_frame_rate_resolve(1, 240, 1) == 60,
                "battery saver overrides adaptive refresh");
    expect_true(reach_frame_rate_resolve(1, 240, 0) == 240,
                "leaving battery saver restores adaptive refresh");
    expect_true(reach_frame_rate_resolve(0, 240, 0) == 60 &&
                    reach_frame_rate_resolve(0, 240, 1) == 60,
                "adaptive off is always 60 Hz");
    expect_true(reach_frame_rate_resolve(1, 0, 0) == 60 && reach_frame_rate_resolve(1, -1, 0) == 60,
                "missing or invalid display refresh falls back to 60 Hz");
    expect_true(reach_frame_rate_resolve(1, 30, 0) == 30, "adaptive follows slower displays too");
}

static void test_deadlines_do_not_drift(void)
{
    const int32_t rates[] = {60, 144, 240};
    for (int32_t rate : rates)
    {
        reach_frame_clock clock = {};
        double now = 0.0;
        expect_true(reach_frame_clock_wait_ms(&clock, now, rate, 1) == 0,
                    "the first active frame is immediate");
        for (int frame = 0; frame < rate * 10; ++frame)
        {
            reach_frame_clock_presented(&clock, now);
            uint32_t wait = reach_frame_clock_wait_ms(&clock, now, rate, 1);
            expect_true(wait > 0, "animation pacing never spins at zero milliseconds");
            now += (double)wait / 1000.0;
        }
        expect_true(std::fabs(now - 10.0) < 0.002,
                    "rounded waits preserve the long-term target rate");
    }
}

static void test_rate_changes_stalls_and_idle(void)
{
    reach_frame_clock clock = {};
    reach_frame_clock_wait_ms(&clock, 0.0, 240, 1);
    reach_frame_clock_presented(&clock, 0.0);
    expect_true(reach_frame_clock_wait_ms(&clock, 0.001, 60, 1) == 17,
                "saver transition replaces the high-refresh deadline");
    double deadline = clock.next_frame_seconds;
    reach_frame_clock_presented(&clock, 0.002);
    expect_true(clock.next_frame_seconds == deadline,
                "early input-driven frames do not postpone animation deadlines");
    reach_frame_clock_presented(&clock, 5.0);
    expect_true(clock.next_frame_seconds > 5.0 && clock.next_frame_seconds < 5.017,
                "missed frames are skipped without a catch-up burst");
    expect_true(reach_frame_clock_wait_ms(&clock, 5.0, 240, 1) == 5,
                "leaving saver restores the display-rate deadline");
    expect_true(reach_frame_clock_wait_ms(&clock, 5.0, 240, 0) == UINT32_MAX && !clock.active,
                "idle resets the clock and permits indefinite sleep");
    expect_true(reach_frame_clock_wait_ms(&clock, 1000.0, 144, 1) == 0,
                "waking from idle starts a fresh immediate frame");
}

int main(void)
{
    test_frame_rate_policy();
    test_deadlines_do_not_drift();
    test_rate_changes_stalls_and_idle();
    return failures != 0;
}
