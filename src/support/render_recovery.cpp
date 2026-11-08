#include "reach/support/render_recovery.h"

static const uint32_t REACH_RENDER_RECOVERY_REPEAT_LOSS_MS[REACH_RENDER_RECOVERY_BACKOFF_COUNT] = {
    0, 100, 500, 1000, 2000, 4000};

static uint32_t reach_render_recovery_repeat_loss_ms(uint32_t loss_index)
{
    uint32_t index = loss_index < REACH_RENDER_RECOVERY_BACKOFF_COUNT
                         ? loss_index
                         : REACH_RENDER_RECOVERY_BACKOFF_COUNT - 1;
    return REACH_RENDER_RECOVERY_REPEAT_LOSS_MS[index];
}

static uint32_t reach_render_recovery_retry_ms(uint64_t elapsed_ms)
{
    if (elapsed_ms < REACH_RENDER_RECOVERY_DENSE_WINDOW_MS)
    {
        return REACH_RENDER_RECOVERY_DENSE_INTERVAL_MS;
    }
    if (elapsed_ms < REACH_RENDER_RECOVERY_SLOW_WINDOW_MS)
    {
        return REACH_RENDER_RECOVERY_SLOW_INTERVAL_MS;
    }
    return REACH_RENDER_RECOVERY_LATE_INTERVAL_MS;
}

static int32_t reach_render_recovery_use_warp(const reach_render_recovery *recovery,
                                              uint64_t now_ms)
{
    return recovery->hardware_unusable ||
           (now_ms >= recovery->started_ms &&
            now_ms - recovery->started_ms >= REACH_RENDER_RECOVERY_HARDWARE_WINDOW_MS);
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
    recovery->next_attempt_ms =
        now_ms + reach_render_recovery_repeat_loss_ms(recovery->loss_count - 1);
    recovery->failed_attempts = 0;
    recovery->warp_failures = 0;
    recovery->hardware_unusable = 0;
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

    if (recovery->warp_failures > 0)
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
    step.use_warp = reach_render_recovery_use_warp(recovery, now_ms);
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
    if (used_warp)
    {
        recovery->warp_failures += 1;
        return;
    }

    uint64_t elapsed = now_ms >= recovery->started_ms ? now_ms - recovery->started_ms : 0;
    uint64_t next = now_ms + reach_render_recovery_retry_ms(elapsed);
    uint64_t window_end = recovery->started_ms + REACH_RENDER_RECOVERY_HARDWARE_WINDOW_MS;
    recovery->next_attempt_ms = next < window_end ? next : window_end;
}

void reach_render_recovery_hardware_unusable(reach_render_recovery *recovery, uint64_t now_ms)
{
    if (recovery == nullptr || !recovery->recovering)
    {
        return;
    }

    recovery->hardware_unusable = 1;
    recovery->next_attempt_ms = now_ms;
}

void reach_render_recovery_attempt_now(reach_render_recovery *recovery, uint64_t now_ms)
{
    if (recovery == nullptr || !recovery->recovering)
    {
        return;
    }

    if (recovery->next_attempt_ms > now_ms)
    {
        recovery->next_attempt_ms = now_ms;
    }
}

void reach_render_recovery_succeeded(reach_render_recovery *recovery)
{
    if (recovery == nullptr)
    {
        return;
    }

    recovery->recovering = 0;
    recovery->failed_attempts = 0;
    recovery->warp_failures = 0;
    recovery->hardware_unusable = 0;
}

int32_t reach_render_recovery_should_leave_warp(int32_t warp_active, int32_t lost,
                                                int32_t recovering)
{
    return warp_active && !lost && !recovering;
}
