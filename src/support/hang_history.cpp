#include "reach/support/hang_history.h"

int32_t reach_hang_history_allow_restart(reach_hang_history *history, uint64_t now_ms)
{
    if (history == nullptr)
    {
        return 1;
    }

    uint64_t oldest = history->restart_ms[0];
    if (oldest != 0 && now_ms >= oldest && now_ms - oldest <= REACH_HANG_HISTORY_WINDOW_MS)
    {
        return 0;
    }

    for (uint32_t index = 1; index < REACH_HANG_HISTORY_RESTARTS; ++index)
    {
        history->restart_ms[index - 1] = history->restart_ms[index];
    }
    history->restart_ms[REACH_HANG_HISTORY_RESTARTS - 1] = now_ms;
    return 1;
}
