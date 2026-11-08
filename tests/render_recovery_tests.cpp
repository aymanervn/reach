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

static void test_first_attempt_is_immediate_and_backoff_grows(void)
{
    reach_render_recovery recovery = {};
    expect(reach_render_recovery_begin(&recovery, 1000) == 1, "the first loss is tolerated");
    reach_render_recovery_step step = reach_render_recovery_step_at(&recovery, 1000);
    expect(step.action == REACH_RENDER_RECOVERY_ATTEMPT && !step.use_warp,
           "the first attempt is immediate on hardware");

    const uint32_t expected[] = {100, 500, 1000, 2000, 4000, 4000, 4000};
    uint64_t now = 1000;
    for (size_t index = 0; index < sizeof(expected) / sizeof(expected[0]); ++index)
    {
        reach_render_recovery_attempt_failed(&recovery, now, 0);
        step = reach_render_recovery_step_at(&recovery, now);
        expect(step.action == REACH_RENDER_RECOVERY_WAIT && step.wait_ms == expected[index],
               "each failure waits the next backoff step");
        now += expected[index];
        step = reach_render_recovery_step_at(&recovery, now);
        expect(step.action == REACH_RENDER_RECOVERY_ATTEMPT, "the attempt is due after the wait");
    }
}

static void test_wait_shrinks_as_time_passes(void)
{
    reach_render_recovery recovery = {};
    reach_render_recovery_begin(&recovery, 0);
    reach_render_recovery_attempt_failed(&recovery, 0, 0);
    reach_render_recovery_step step = reach_render_recovery_step_at(&recovery, 40);
    expect(step.action == REACH_RENDER_RECOVERY_WAIT && step.wait_ms == 60,
           "the remaining wait counts down");
}

static void test_warp_after_two_failed_hardware_attempts(void)
{
    reach_render_recovery recovery = {};
    reach_render_recovery_begin(&recovery, 0);
    reach_render_recovery_attempt_failed(&recovery, 0, 0);
    reach_render_recovery_step step = reach_render_recovery_step_at(&recovery, 100);
    expect(step.action == REACH_RENDER_RECOVERY_ATTEMPT && !step.use_warp,
           "one hardware failure keeps trying hardware");
    reach_render_recovery_attempt_failed(&recovery, 100, 0);
    step = reach_render_recovery_step_at(&recovery, 600);
    expect(step.action == REACH_RENDER_RECOVERY_ATTEMPT && step.use_warp,
           "two hardware failures switch to WARP");
    reach_render_recovery_attempt_failed(&recovery, 600, 1);
    step = reach_render_recovery_step_at(&recovery, 1600);
    expect(step.action == REACH_RENDER_RECOVERY_ATTEMPT && step.use_warp,
           "WARP stays selected after a WARP failure");
}

static void test_gives_up_after_thirty_seconds_of_failure(void)
{
    reach_render_recovery recovery = {};
    reach_render_recovery_begin(&recovery, 5000);
    reach_render_recovery_step step = reach_render_recovery_step_at(&recovery, 5000);
    expect(step.action == REACH_RENDER_RECOVERY_ATTEMPT, "an untried recovery never gives up");
    reach_render_recovery_attempt_failed(&recovery, 5000, 0);
    step = reach_render_recovery_step_at(&recovery, 34999);
    expect(step.action != REACH_RENDER_RECOVERY_GIVE_UP, "just under thirty seconds keeps trying");
    step = reach_render_recovery_step_at(&recovery, 35000);
    expect(step.action == REACH_RENDER_RECOVERY_GIVE_UP, "thirty seconds of failure gives up");
}

static void test_success_clears_the_attempt_state(void)
{
    reach_render_recovery recovery = {};
    reach_render_recovery_begin(&recovery, 0);
    reach_render_recovery_attempt_failed(&recovery, 0, 0);
    reach_render_recovery_attempt_failed(&recovery, 100, 0);
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
    test_first_attempt_is_immediate_and_backoff_grows();
    test_wait_shrinks_as_time_passes();
    test_warp_after_two_failed_hardware_attempts();
    test_gives_up_after_thirty_seconds_of_failure();
    test_success_clears_the_attempt_state();
    test_loss_rate_limit();
    test_old_losses_expire();
    test_repeated_loss_backs_off_the_first_attempt();
    test_flapping_device_cannot_exhaust_the_budget_instantly();
    test_begin_is_idempotent_while_recovering();
    return failed == 0 ? 0 : 1;
}
