#include "reach/services/idle_watch.h"

#include <stdio.h>

static int failures = 0;
static void expect_true(int value, const char *message)
{
    if (!value)
    {
        ++failures;
        printf("FAIL: %s\n", message);
    }
}

static const uint64_t POLL = 30000;
static const uint64_t MINUTE = 60000;

static reach_idle_watch_sample make_sample(uint64_t now, uint64_t input_idle,
                                           int32_t awake_required)
{
    reach_idle_watch_sample sample = {};
    sample.now_milliseconds = now;
    sample.input_idle_milliseconds = input_idle;
    sample.awake_required = awake_required;
    return sample;
}

static void test_fires_once_at_threshold(void)
{
    reach_idle_watch_config config = {};
    config.timeout_minutes[REACH_IDLE_WATCH_ACTION_LOCK] = 1;
    reach_idle_watch_state state = {};
    reach_idle_watch_state_init(&state, 0);

    uint64_t now = POLL;
    reach_idle_watch_sample sample = make_sample(now, now, 0);
    expect_true(reach_idle_watch_evaluate(&state, &config, &sample, POLL) == 0,
                "no action below the timeout");

    now = MINUTE;
    sample = make_sample(now, now, 0);
    expect_true(reach_idle_watch_evaluate(&state, &config, &sample, POLL) ==
                    (1u << REACH_IDLE_WATCH_ACTION_LOCK),
                "lock fires once the timeout elapses");

    now += POLL;
    sample = make_sample(now, now, 0);
    expect_true(reach_idle_watch_evaluate(&state, &config, &sample, POLL) == 0,
                "continued idleness does not refire");

    now += POLL;
    sample = make_sample(now, 1000, 0);
    expect_true(reach_idle_watch_evaluate(&state, &config, &sample, POLL) == 0,
                "fresh input rearms without firing");

    now += POLL;
    sample = make_sample(now, 1000 + POLL, 0);
    expect_true(reach_idle_watch_evaluate(&state, &config, &sample, POLL) == 0,
                "idleness rebuilds after input");
    now += POLL;
    sample = make_sample(now, 1000 + 2 * POLL, 0);
    expect_true(reach_idle_watch_evaluate(&state, &config, &sample, POLL) ==
                    (1u << REACH_IDLE_WATCH_ACTION_LOCK),
                "a second idle period fires again");
}

static void test_zero_timeout_never_fires(void)
{
    reach_idle_watch_config config = {};
    reach_idle_watch_state state = {};
    reach_idle_watch_state_init(&state, 0);
    reach_idle_watch_sample sample = make_sample(POLL, 100 * MINUTE, 0);
    expect_true(reach_idle_watch_evaluate(&state, &config, &sample, POLL) == 0,
                "timers set to never do not fire");
}

static void test_wait_for_awake_apps(void)
{
    reach_idle_watch_config config = {};
    config.timeout_minutes[REACH_IDLE_WATCH_ACTION_SHUTDOWN] = 1;
    config.wait_awake_apps[REACH_IDLE_WATCH_ACTION_SHUTDOWN] = 1;
    reach_idle_watch_state state = {};
    reach_idle_watch_state_init(&state, 0);

    uint64_t now = 0;
    uint32_t mask = 0;
    for (int step = 0; step < 4; ++step)
    {
        now += POLL;
        reach_idle_watch_sample sample = make_sample(now, now, 1);
        mask = reach_idle_watch_evaluate(&state, &config, &sample, POLL);
        expect_true(mask == 0, "shutdown holds while apps keep the system awake");
    }

    now += POLL;
    reach_idle_watch_sample sample = make_sample(now, now, 0);
    expect_true(reach_idle_watch_evaluate(&state, &config, &sample, POLL) ==
                    (1u << REACH_IDLE_WATCH_ACTION_SHUTDOWN),
                "shutdown fires once the awake requirement clears");
}

static void test_wait_flag_off_ignores_awake_apps(void)
{
    reach_idle_watch_config config = {};
    config.timeout_minutes[REACH_IDLE_WATCH_ACTION_SLEEP] = 1;
    reach_idle_watch_state state = {};
    reach_idle_watch_state_init(&state, 0);

    uint64_t now = POLL;
    reach_idle_watch_sample sample = make_sample(now, now, 1);
    expect_true(reach_idle_watch_evaluate(&state, &config, &sample, POLL) == 0,
                "no action below the timeout even with awake apps");
    now = MINUTE;
    sample = make_sample(now, now, 1);
    expect_true(reach_idle_watch_evaluate(&state, &config, &sample, POLL) ==
                    (1u << REACH_IDLE_WATCH_ACTION_SLEEP),
                "without the wait flag, awake apps do not hold the timer");
}

static void test_baseline_caps_stale_idle_at_start(void)
{
    reach_idle_watch_config config = {};
    config.timeout_minutes[REACH_IDLE_WATCH_ACTION_SHUTDOWN] = 1;
    reach_idle_watch_state state = {};
    reach_idle_watch_state_init(&state, 100 * MINUTE);

    reach_idle_watch_sample sample = make_sample(100 * MINUTE + POLL, 90 * MINUTE, 0);
    expect_true(reach_idle_watch_evaluate(&state, &config, &sample, POLL) == 0,
                "input idleness accrued before the watcher started does not count");
}

static void test_resume_resets_baseline(void)
{
    reach_idle_watch_config config = {};
    config.timeout_minutes[REACH_IDLE_WATCH_ACTION_SHUTDOWN] = 30;
    reach_idle_watch_state state = {};
    reach_idle_watch_state_init(&state, 0);

    uint64_t now = POLL;
    reach_idle_watch_sample sample = make_sample(now, now, 0);
    expect_true(reach_idle_watch_evaluate(&state, &config, &sample, POLL) == 0,
                "no action right after start");

    now += 120 * MINUTE;
    sample = make_sample(now, now, 0);
    expect_true(reach_idle_watch_evaluate(&state, &config, &sample, POLL) == 0,
                "a large time jump (resume from sleep) resets the idle baseline");

    uint64_t resume = now;
    int32_t early_fire = 0;
    uint32_t final_mask = 0;
    for (uint64_t at = resume + POLL; at <= resume + 30 * MINUTE; at += POLL)
    {
        sample = make_sample(at, at, 0);
        uint32_t mask = reach_idle_watch_evaluate(&state, &config, &sample, POLL);
        if (at < resume + 30 * MINUTE && mask != 0)
        {
            early_fire = 1;
        }
        if (at == resume + 30 * MINUTE)
        {
            final_mask = mask;
        }
    }
    expect_true(!early_fire, "post-resume idleness counts from the resume point");
    expect_true(final_mask == (1u << REACH_IDLE_WATCH_ACTION_SHUTDOWN),
                "the timer still fires after a full post-resume idle period");
}

static void test_multiple_actions_fire_together(void)
{
    reach_idle_watch_config config = {};
    config.timeout_minutes[REACH_IDLE_WATCH_ACTION_LOCK] = 1;
    config.timeout_minutes[REACH_IDLE_WATCH_ACTION_SLEEP] = 1;
    reach_idle_watch_state state = {};
    reach_idle_watch_state_init(&state, 0);

    reach_idle_watch_sample sample = make_sample(2 * MINUTE, 2 * MINUTE, 0);
    uint32_t mask = reach_idle_watch_evaluate(&state, &config, &sample, POLL * 5);
    expect_true(mask == ((1u << REACH_IDLE_WATCH_ACTION_LOCK) |
                         (1u << REACH_IDLE_WATCH_ACTION_SLEEP)),
                "actions sharing a timeout report together");
}

int main(void)
{
    test_fires_once_at_threshold();
    test_zero_timeout_never_fires();
    test_wait_for_awake_apps();
    test_wait_flag_off_ignores_awake_apps();
    test_baseline_caps_stale_idle_at_start();
    test_resume_resets_baseline();
    test_multiple_actions_fire_together();
    return failures == 0 ? 0 : 1;
}
