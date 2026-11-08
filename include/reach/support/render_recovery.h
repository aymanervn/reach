#ifndef REACH_RENDER_RECOVERY_H
#define REACH_RENDER_RECOVERY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_RENDER_RECOVERY_BACKOFF_COUNT 6u
#define REACH_RENDER_RECOVERY_WARP_AFTER_FAILURES 2u
#define REACH_RENDER_RECOVERY_GIVE_UP_MS 30000ull
#define REACH_RENDER_RECOVERY_LOSS_LIMIT 5u
#define REACH_RENDER_RECOVERY_LOSS_WINDOW_MS 60000ull

    typedef enum reach_render_recovery_action
    {
        REACH_RENDER_RECOVERY_IDLE = 0,
        REACH_RENDER_RECOVERY_WAIT = 1,
        REACH_RENDER_RECOVERY_ATTEMPT = 2,
        REACH_RENDER_RECOVERY_GIVE_UP = 3
    } reach_render_recovery_action;

    typedef struct reach_render_recovery
    {
        uint64_t loss_ms[REACH_RENDER_RECOVERY_LOSS_LIMIT + 1];
        uint32_t loss_count;
        int32_t recovering;
        uint64_t started_ms;
        uint64_t next_attempt_ms;
        uint32_t failed_attempts;
        uint32_t hardware_failures;
    } reach_render_recovery;

    typedef struct reach_render_recovery_step
    {
        reach_render_recovery_action action;
        uint32_t wait_ms;
        int32_t use_warp;
    } reach_render_recovery_step;

    int32_t reach_render_recovery_begin(reach_render_recovery *recovery, uint64_t now_ms);
    reach_render_recovery_step reach_render_recovery_step_at(const reach_render_recovery *recovery,
                                                             uint64_t now_ms);
    void reach_render_recovery_attempt_failed(reach_render_recovery *recovery, uint64_t now_ms,
                                              int32_t used_warp);
    void reach_render_recovery_succeeded(reach_render_recovery *recovery);

#ifdef __cplusplus
}
#endif

#endif
