#ifndef REACH_CRASH_LOOP_H
#define REACH_CRASH_LOOP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_CRASH_LOOP_STARTS 3u
#define REACH_CRASH_LOOP_WINDOW_MS 60000ull

    // Start times in wall-clock milliseconds, oldest first; 0 marks an empty slot.
    typedef struct reach_crash_loop
    {
        uint64_t start_ms[REACH_CRASH_LOOP_STARTS];
    } reach_crash_loop;

    // Records a start and reports whether the last REACH_CRASH_LOOP_STARTS starts all fall
    // inside REACH_CRASH_LOOP_WINDOW_MS, meaning the runs between them died young.
    int32_t reach_crash_loop_record_start(reach_crash_loop *loop, uint64_t now_ms);

#ifdef __cplusplus
}
#endif

#endif
