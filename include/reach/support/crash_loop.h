#ifndef REACH_CRASH_LOOP_H
#define REACH_CRASH_LOOP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_CRASH_LOOP_WINDOW_MS 180000ull
#define REACH_CRASH_LOOP_FALLBACK_COUNT 3u
#define REACH_CRASH_LOOP_SECOND_RESTART_DELAY_MS 1000u

    typedef enum reach_crash_loop_action
    {
        REACH_CRASH_LOOP_RESTART = 0,
        REACH_CRASH_LOOP_FALLBACK = 1
    } reach_crash_loop_action;

    typedef struct reach_crash_loop
    {
        uint64_t exit_ms[REACH_CRASH_LOOP_FALLBACK_COUNT];
        uint32_t count;
    } reach_crash_loop;

    typedef struct reach_crash_loop_decision
    {
        reach_crash_loop_action action;
        uint32_t delay_ms;
        uint32_t window_count;
    } reach_crash_loop_decision;

    typedef enum reach_exit_disposition
    {
        REACH_EXIT_DISPOSITION_RECORD = 0,
        REACH_EXIT_DISPOSITION_ATTACH = 1,
        REACH_EXIT_DISPOSITION_RETURN = 2
    } reach_exit_disposition;

    reach_exit_disposition reach_crash_loop_classify_exit(uint32_t exit_code, int32_t shutting_down,
                                                          int32_t other_reach_alive);

    reach_crash_loop_decision reach_crash_loop_record_exit(reach_crash_loop *loop, uint64_t now_ms);

#ifdef __cplusplus
}
#endif

#endif
