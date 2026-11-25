#ifndef REACH_HANG_HISTORY_H
#define REACH_HANG_HISTORY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_HANG_HISTORY_RESTARTS 3u
#define REACH_HANG_HISTORY_WINDOW_MS 300000ull

    // Hang restart times in wall-clock milliseconds, oldest first; 0 marks an empty slot.
    typedef struct reach_hang_history
    {
        uint64_t restart_ms[REACH_HANG_HISTORY_RESTARTS];
    } reach_hang_history;

    // Records a hang restart at now_ms and returns 1, unless REACH_HANG_HISTORY_RESTARTS restarts
    // already fall inside REACH_HANG_HISTORY_WINDOW_MS; then it records nothing and returns 0.
    int32_t reach_hang_history_allow_restart(reach_hang_history *history, uint64_t now_ms);

#ifdef __cplusplus
}
#endif

#endif
