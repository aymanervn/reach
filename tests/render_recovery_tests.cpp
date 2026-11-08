#include "reach/support/render_recovery.h"

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

static void test_idle_without_a_loss(void)
{
    reach_render_recovery recovery = {};
    reach_render_recovery_step step = reach_render_recovery_step_at(&recovery, 1000);
    expect(step.action == REACH_RENDER_RECOVERY_IDLE, "no recovery is idle");
}

static void test_first_attempt_is_immediate_on_hardware(void)
{
    reach_render_recovery recovery = {};
    expect(reach_render_recovery_begin(&recovery, 1000) == 1, "the first loss is tolerated");
    reach_render_recovery_step step = reach_render_recovery_step_at(&recovery, 1000);
    expect(step.action == REACH_RENDER_RECOVERY_ATTEMPT && !step.use_warp,
           "the first attempt is immediate on hardware");
}

static void test_hardware_retries_are_dense_then_back_off(void)
{
    reach_render_recovery recovery = {};
    reach_render_recovery_begin(&recovery, 0);
    uint64_t now = 0;
    uint32_t dense_attempts = 0;
    while (now < REACH_RENDER_RECOVERY_DENSE_WINDOW_MS)
    {
        reach_render_recovery_attempt_failed(&recovery, now, 0);
        reach_render_recovery_step step = reach_render_recovery_step_at(&recovery, now);
        expect(step.action == REACH_RENDER_RECOVERY_WAIT && step.wait_ms == 250,
               "retries inside the driver-reset window are 250 ms apart");
        now += step.wait_ms;
        dense_attempts += 1;
    }
    expect(dense_attempts == 20, "the dense window holds twenty retries");

    reach_render_recovery_attempt_failed(&recovery, now, 0);
    reach_render_recovery_step step = reach_render_recovery_step_at(&recovery, now);
    expect(step.action == REACH_RENDER_RECOVERY_WAIT && step.wait_ms == 2000,
           "after the dense window retries back off to two seconds");

    now = REACH_RENDER_RECOVERY_SLOW_WINDOW_MS;
    reach_render_recovery_attempt_failed(&recovery, now, 0);
    step = reach_render_recovery_step_at(&recovery, now);
    expect(step.action == REACH_RENDER_RECOVERY_WAIT && step.wait_ms == 4000,
           "late retries are four seconds apart");
}

static void test_wait_shrinks_as_time_passes(void)
{
    reach_render_recovery recovery = {};
    reach_render_recovery_begin(&recovery, 0);
    reach_render_recovery_attempt_failed(&recovery, 0, 0);
    reach_render_recovery_step step = reach_render_recovery_step_at(&recovery, 40);
    expect(step.action == REACH_RENDER_RECOVERY_WAIT && step.wait_ms == 210,
           "the remaining wait counts down");
}

static void test_hardware_only_until_the_window_ends(void)
{
    reach_render_recovery recovery = {};
    reach_render_recovery_begin(&recovery, 1000);
    uint64_t now = 1000;
    uint64_t window_end = 1000 + REACH_RENDER_RECOVERY_HARDWARE_WINDOW_MS;
    reach_render_recovery_step step = reach_render_recovery_step_at(&recovery, now);
    while (now < window_end)
    {
        if (step.action == REACH_RENDER_RECOVERY_WAIT)
        {
            expect(now + step.wait_ms <= window_end, "no hardware wait runs past the window");
            now += step.wait_ms;
        }
        else
        {
            expect(step.action == REACH_RENDER_RECOVERY_ATTEMPT && !step.use_warp,
                   "hardware is the only adapter inside the window");
            reach_render_recovery_attempt_failed(&recovery, now, 0);
        }
        step = reach_render_recovery_step_at(&recovery, now);
    }
    expect(step.action == REACH_RENDER_RECOVERY_ATTEMPT && step.use_warp,
           "the end of the window selects WARP rather than giving up");
}

static void test_gives_up_only_when_warp_also_fails(void)
{
    reach_render_recovery recovery = {};
    reach_render_recovery_begin(&recovery, 5000);
    reach_render_recovery_attempt_failed(&recovery, 5000, 0);
    reach_render_recovery_step step = reach_render_recovery_step_at(&recovery, 35000);
    expect(step.action == REACH_RENDER_RECOVERY_ATTEMPT && step.use_warp,
           "WARP is tried before giving up");
    reach_render_recovery_attempt_failed(&recovery, 35000, 1);
    step = reach_render_recovery_step_at(&recovery, 35000);
    expect(step.action == REACH_RENDER_RECOVERY_GIVE_UP, "a failed WARP attempt gives up");
}

static void test_unusable_hardware_goes_straight_to_warp(void)
{
    reach_render_recovery recovery = {};
    reach_render_recovery_begin(&recovery, 100);
    reach_render_recovery_attempt_failed(&recovery, 100, 0);
    reach_render_recovery_hardware_unusable(&recovery, 100);
    reach_render_recovery_step step = reach_render_recovery_step_at(&recovery, 100);
    expect(step.action == REACH_RENDER_RECOVERY_ATTEMPT && step.use_warp,
           "missing hardware selects WARP at once");
    reach_render_recovery_succeeded(&recovery);
    reach_render_recovery_begin(&recovery, 100000);
    step = reach_render_recovery_step_at(&recovery, 100000);
    expect(step.action == REACH_RENDER_RECOVERY_ATTEMPT && !step.use_warp,
           "unusable hardware is forgotten after a recovery");
}

static void test_a_display_change_forces_an_immediate_attempt(void)
{
    reach_render_recovery recovery = {};
    reach_render_recovery_begin(&recovery, 0);
    reach_render_recovery_attempt_failed(&recovery, 20000, 0);
    reach_render_recovery_step step = reach_render_recovery_step_at(&recovery, 20100);
    expect(step.action == REACH_RENDER_RECOVERY_WAIT, "a slow retry is pending");
    reach_render_recovery_attempt_now(&recovery, 20100);
    step = reach_render_recovery_step_at(&recovery, 20100);
    expect(step.action == REACH_RENDER_RECOVERY_ATTEMPT && !step.use_warp,
           "the notification makes the hardware attempt due now");
    reach_render_recovery idle = {};
    reach_render_recovery_attempt_now(&idle, 5);
    expect(reach_render_recovery_step_at(&idle, 5).action == REACH_RENDER_RECOVERY_IDLE,
           "a notification outside recovery does nothing");
}

static void test_success_clears_the_attempt_state(void)
{
    reach_render_recovery recovery = {};
    reach_render_recovery_begin(&recovery, 0);
    reach_render_recovery_attempt_failed(&recovery, 0, 0);
    reach_render_recovery_attempt_failed(&recovery, 250, 0);
    reach_render_recovery_succeeded(&recovery);
    expect(reach_render_recovery_step_at(&recovery, 200).action == REACH_RENDER_RECOVERY_IDLE,
           "success returns to idle");
    reach_render_recovery_begin(&recovery, 100000);
    reach_render_recovery_step step = reach_render_recovery_step_at(&recovery, 100000);
    expect(step.action == REACH_RENDER_RECOVERY_ATTEMPT && !step.use_warp,
           "a later loss starts on hardware again");
}

static void test_loss_rate_limit(void)
{
    reach_render_recovery recovery = {};
    for (uint32_t index = 0; index < REACH_RENDER_RECOVERY_LOSS_LIMIT; ++index)
    {
        expect(reach_render_recovery_begin(&recovery, 1000 + index * 1000) == 1,
               "five losses in a minute are tolerated");
        reach_render_recovery_succeeded(&recovery);
    }
    expect(reach_render_recovery_begin(&recovery, 7000) == 0,
           "the sixth loss inside a minute gives up");
}

static void test_old_losses_expire(void)
{
    reach_render_recovery recovery = {};
    for (uint32_t index = 0; index < REACH_RENDER_RECOVERY_LOSS_LIMIT; ++index)
    {
        reach_render_recovery_begin(&recovery, 1000 + index * 1000);
        reach_render_recovery_succeeded(&recovery);
    }
    expect(reach_render_recovery_begin(&recovery, 70000) == 1,
           "losses older than a minute no longer count");
}

static void test_repeated_loss_backs_off_the_first_attempt(void)
{
    reach_render_recovery recovery = {};
    const uint32_t expected[] = {0, 100, 500, 1000, 2000};
    uint64_t now = 1000;
    for (size_t index = 0; index < sizeof(expected) / sizeof(expected[0]); ++index)
    {
        reach_render_recovery_begin(&recovery, now);
        reach_render_recovery_step step = reach_render_recovery_step_at(&recovery, now);
        if (expected[index] == 0)
        {
            expect(step.action == REACH_RENDER_RECOVERY_ATTEMPT, "the first loss is immediate");
        }
        else
        {
            expect(step.action == REACH_RENDER_RECOVERY_WAIT && step.wait_ms == expected[index],
                   "a re-loss waits the next backoff step before the first attempt");
            now += expected[index];
            step = reach_render_recovery_step_at(&recovery, now);
            expect(step.action == REACH_RENDER_RECOVERY_ATTEMPT && !step.use_warp,
                   "the delayed first attempt is due on hardware");
        }
        reach_render_recovery_succeeded(&recovery);
    }
}

static void test_flapping_device_cannot_exhaust_the_budget_instantly(void)
{
    reach_render_recovery recovery = {};
    uint64_t now = 0;
    int32_t tolerated = 1;
    for (uint32_t index = 0; index < REACH_RENDER_RECOVERY_LOSS_LIMIT && tolerated; ++index)
    {
        tolerated = reach_render_recovery_begin(&recovery, now);
        reach_render_recovery_step step = reach_render_recovery_step_at(&recovery, now);
        if (step.action == REACH_RENDER_RECOVERY_WAIT)
        {
            now += step.wait_ms;
        }
        reach_render_recovery_succeeded(&recovery);
    }
    expect(tolerated == 1, "five losses are tolerated");
    expect(now >= 3600, "five back-to-back losses take at least their accumulated backoff");
}

static void test_leaves_warp_only_when_healthy_and_idle(void)
{
    expect(reach_render_recovery_should_leave_warp(1, 0, 0) == 1,
           "a healthy WARP device returns to hardware");
    expect(reach_render_recovery_should_leave_warp(0, 0, 0) == 0,
           "a hardware device has nothing to leave");
    expect(reach_render_recovery_should_leave_warp(1, 1, 0) == 0,
           "a lost device recovers through the loss path");
    expect(reach_render_recovery_should_leave_warp(1, 0, 1) == 0,
           "an active recovery is not interrupted");
}

static void test_begin_is_idempotent_while_recovering(void)
{
    reach_render_recovery recovery = {};
    reach_render_recovery_begin(&recovery, 0);
    reach_render_recovery_begin(&recovery, 50);
    expect(recovery.loss_count == 1, "a loss during recovery is not counted twice");
}

int main(void)
{
    test_idle_without_a_loss();
    test_first_attempt_is_immediate_on_hardware();
    test_hardware_retries_are_dense_then_back_off();
    test_wait_shrinks_as_time_passes();
    test_hardware_only_until_the_window_ends();
    test_gives_up_only_when_warp_also_fails();
    test_unusable_hardware_goes_straight_to_warp();
    test_a_display_change_forces_an_immediate_attempt();
    test_success_clears_the_attempt_state();
    test_loss_rate_limit();
    test_old_losses_expire();
    test_repeated_loss_backs_off_the_first_attempt();
    test_flapping_device_cannot_exhaust_the_budget_instantly();
    test_leaves_warp_only_when_healthy_and_idle();
    test_begin_is_idempotent_while_recovering();
    return failed == 0 ? 0 : 1;
}
