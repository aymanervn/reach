#ifndef REACH_ADAPTERS_WINDOWS_FRAME_WAIT_H
#define REACH_ADAPTERS_WINDOWS_FRAME_WAIT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    void *reach_windows_frame_timer_create(void);
    void reach_windows_frame_timer_destroy(void *timer);
    uint32_t reach_windows_frame_wait(void *timer, void *const *handles, uint32_t handle_count,
                                      uint32_t timeout_ms, double frame_deadline_seconds);

#ifdef __cplusplus
}
#endif

#endif
