#include "reach/support/crash_loop.h"

int32_t reach_crash_loop_record_start(reach_crash_loop *loop, uint64_t now_ms)
{
    if (loop == nullptr)
    {
        return 0;
    }

    for (uint32_t index = 1; index < REACH_CRASH_LOOP_STARTS; ++index)
    {
        loop->start_ms[index - 1] = loop->start_ms[index];
    }
    loop->start_ms[REACH_CRASH_LOOP_STARTS - 1] = now_ms;

    uint64_t oldest = loop->start_ms[0];
    return oldest != 0 && now_ms >= oldest && now_ms - oldest <= REACH_CRASH_LOOP_WINDOW_MS;
}
