#include "reach/core/frame_clock.h"
#include "reach/core/blink.h"

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

static void test_blink_deadlines_and_duty_cycle(void)
{
    reach_blink blink = {};
    reach_blink_reset_at(&blink, 0.55, 0.45, 100.0);
    expect_true(blink.visible && blink.deadline_seconds == 100.55,
                "focus starts with the original visible interval");
    expect_true(!reach_blink_update_at(&blink, 100.54) && blink.visible,
                "unrelated wakes before the deadline do not change the caret");
    expect_true(reach_blink_update_at(&blink, blink.deadline_seconds) && !blink.visible &&
                    blink.deadline_seconds == 101.0,
                "the exact boundary starts the original hidden interval");
    expect_true(reach_blink_update_at(&blink, 101.0) && blink.visible &&
                    blink.deadline_seconds == 101.55,
                "the cycle stays aligned to its focus epoch");
    reach_blink_reset_at(&blink, 0.53, 0.53, 100.0);
    expect_true(reach_blink_update_at(&blink, blink.deadline_seconds) && !blink.visible &&
                    std::fabs(blink.deadline_seconds - 101.06) < 0.000001,
                "symmetric Wi-Fi and Launcher timing is preserved");
}

static void test_blink_delayed_wake_reset_and_stop(void)
{
    reach_blink blink = {};
    reach_blink_reset_at(&blink, 0.55, 0.45, 100.0);
    expect_true(!reach_blink_update_at(&blink, 150.2) && blink.visible &&
                    blink.deadline_seconds == 150.55,
                "missed whole cycles need no catch-up frames");
    expect_true(reach_blink_update_at(&blink, 1000.8) && !blink.visible &&
                    blink.deadline_seconds == 1001.0,
                "a late wake samples the correct phase in constant time");
    reach_blink_reset_at(&blink, 0.55, 0.45, 1000.9);
    expect_true(blink.visible && std::fabs(blink.deadline_seconds - 1001.45) < 0.000001,
                "typing resets visibility and the epoch immediately");
    reach_blink_stop(&blink);
    expect_true(!blink.visible && blink.deadline_seconds == 0 &&
                    !reach_blink_update_at(&blink, 2000.0),
                "blur or hide retires the deadline");
    reach_blink_reset_at(&blink, blink.visible_seconds, blink.hidden_seconds, 2000.0);
    expect_true(blink.visible && blink.deadline_seconds == 2000.55,
                "resuming a visible editor reuses its configured duty cycle");
}

static void test_blink_invalid_or_unrepresentable_deadlines(void)
{
    reach_blink blink = {};
    reach_blink_reset_at(&blink, 0.55, 0.45, 100.0);
    double deadline = blink.deadline_seconds;
    expect_true(!reach_blink_update_at(&blink, NAN) && blink.deadline_seconds == deadline,
                "invalid samples do not change a live clock");
    reach_blink_reset_at(&blink, 0.0, 0.45, 100.0);
    expect_true(blink.deadline_seconds == 0, "invalid duration retires the clock");
    reach_blink_reset_at(&blink, 1e-30, 1e-30, 100.0);
    expect_true(blink.deadline_seconds == 0,
                "unrepresentable boundaries cannot create a busy deadline loop");
}

int main(void)
{
    test_frame_rate_policy();
    test_deadlines_do_not_drift();
    test_rate_changes_stalls_and_idle();
    test_blink_deadlines_and_duty_cycle();
    test_blink_delayed_wake_reset_and_stop();
    test_blink_invalid_or_unrepresentable_deadlines();
    return failures != 0;
}
