#include "reach/support/crash_loop.h"

static void reach_crash_loop_drop_expired(reach_crash_loop *loop, uint64_t now_ms)
{
    uint32_t kept = 0;
    for (uint32_t index = 0; index < loop->count; ++index)
    {
        uint64_t exit_ms = loop->exit_ms[index];
        if (now_ms >= exit_ms && now_ms - exit_ms <= REACH_CRASH_LOOP_WINDOW_MS)
        {
            loop->exit_ms[kept++] = exit_ms;
        }
    }
    loop->count = kept;
}

reach_crash_loop_decision reach_crash_loop_record_exit(reach_crash_loop *loop, uint64_t now_ms)
{
    reach_crash_loop_decision decision = {REACH_CRASH_LOOP_RESTART, 0, 0};
    if (loop == nullptr)
    {
        decision.action = REACH_CRASH_LOOP_FALLBACK;
        return decision;
    }

    reach_crash_loop_drop_expired(loop, now_ms);
    if (loop->count == REACH_CRASH_LOOP_FALLBACK_COUNT)
    {
        for (uint32_t index = 1; index < loop->count; ++index)
        {
            loop->exit_ms[index - 1] = loop->exit_ms[index];
        }
        loop->count -= 1;
    }
    loop->exit_ms[loop->count++] = now_ms;

    decision.window_count = loop->count;
    if (loop->count >= REACH_CRASH_LOOP_FALLBACK_COUNT)
    {
        decision.action = REACH_CRASH_LOOP_FALLBACK;
        return decision;
    }

    decision.delay_ms = loop->count >= 2 ? REACH_CRASH_LOOP_SECOND_RESTART_DELAY_MS : 0;
    return decision;
}
