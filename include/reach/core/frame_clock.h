#ifndef REACH_CORE_FRAME_CLOCK_H
#define REACH_CORE_FRAME_CLOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_frame_clock
    {
        double next_frame_seconds;
        int32_t rate_hz;
        int32_t active;
    } reach_frame_clock;

    int32_t reach_frame_rate_resolve(int32_t adaptive, int32_t refresh_rate_hz,
                                     int32_t battery_saver_on);
    uint32_t reach_frame_clock_wait_ms(reach_frame_clock *clock, double now_seconds,
                                       int32_t rate_hz, int32_t active);
    void reach_frame_clock_presented(reach_frame_clock *clock, double now_seconds);

#ifdef __cplusplus
}
#endif

#endif
