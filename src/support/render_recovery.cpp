#include "reach/support/render_recovery.h"

static const uint32_t REACH_RENDER_RECOVERY_BACKOFF_MS[REACH_RENDER_RECOVERY_BACKOFF_COUNT] = {
    0, 100, 500, 1000, 2000, 4000};

static uint32_t reach_render_recovery_backoff_ms(uint32_t failed_attempts)
{
    uint32_t index = failed_attempts < REACH_RENDER_RECOVERY_BACKOFF_COUNT
                         ? failed_attempts
                         : REACH_RENDER_RECOVERY_BACKOFF_COUNT - 1;
    return REACH_RENDER_RECOVERY_BACKOFF_MS[index];
}

static void reach_render_recovery_drop_old_losses(reach_render_recovery *recovery, uint64_t now_ms)
{
    uint32_t kept = 0;
    for (uint32_t index = 0; index < recovery->loss_count; ++index)
    {
        uint64_t loss_ms = recovery->loss_ms[index];
        if (now_ms >= loss_ms && now_ms - loss_ms <= REACH_RENDER_RECOVERY_LOSS_WINDOW_MS)
        {
            recovery->loss_ms[kept++] = loss_ms;
        }
    }
    recovery->loss_count = kept;
}

int32_t reach_render_recovery_begin(reach_render_recovery *recovery, uint64_t now_ms)
{
    if (recovery == nullptr)
    {
        return 0;
    }
    if (recovery->recovering)
    {
        return 1;
    }

    reach_render_recovery_drop_old_losses(recovery, now_ms);
    recovery->loss_ms[recovery->loss_count++] = now_ms;
    recovery->recovering = 1;
    recovery->started_ms = now_ms;
    recovery->next_attempt_ms = now_ms + reach_render_recovery_backoff_ms(recovery->loss_count - 1);
    recovery->failed_attempts = 0;
    recovery->hardware_failures = 0;
    return recovery->loss_count <= REACH_RENDER_RECOVERY_LOSS_LIMIT;
}

reach_render_recovery_step reach_render_recovery_step_at(const reach_render_recovery *recovery,
                                                         uint64_t now_ms)
{
    reach_render_recovery_step step = {REACH_RENDER_RECOVERY_IDLE, 0, 0};
    if (recovery == nullptr || !recovery->recovering)
    {
        return step;
    }

    if (recovery->failed_attempts > 0 && now_ms >= recovery->started_ms &&
        now_ms - recovery->started_ms >= REACH_RENDER_RECOVERY_GIVE_UP_MS)
    {
        step.action = REACH_RENDER_RECOVERY_GIVE_UP;
        return step;
    }

    if (now_ms < recovery->next_attempt_ms)
    {
        uint64_t wait = recovery->next_attempt_ms - now_ms;
        step.action = REACH_RENDER_RECOVERY_WAIT;
        step.wait_ms = wait > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)wait;
        return step;
    }

    step.action = REACH_RENDER_RECOVERY_ATTEMPT;
    step.use_warp = recovery->hardware_failures >= REACH_RENDER_RECOVERY_WARP_AFTER_FAILURES;
    return step;
}

void reach_render_recovery_attempt_failed(reach_render_recovery *recovery, uint64_t now_ms,
                                          int32_t used_warp)
{
    if (recovery == nullptr || !recovery->recovering)
    {
        return;
    }

    recovery->failed_attempts += 1;
    if (!used_warp)
    {
        recovery->hardware_failures += 1;
    }
    recovery->next_attempt_ms =
        now_ms + reach_render_recovery_backoff_ms(recovery->failed_attempts);
}

void reach_render_recovery_succeeded(reach_render_recovery *recovery)
{
    if (recovery == nullptr)
    {
        return;
    }

    recovery->recovering = 0;
    recovery->failed_attempts = 0;
    recovery->hardware_failures = 0;
}
