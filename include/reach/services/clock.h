#ifndef REACH_SERVICES_CLOCK_H
#define REACH_SERVICES_CLOCK_H

#include <stdint.h>

#include "reach/ports/clock.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_CLOCK_WAIT_FOREVER 0xFFFFFFFFu

    typedef struct reach_clock reach_clock;

    typedef struct reach_clock_snapshot
    {
        int32_t year;
        int32_t month;
        int32_t day;
        int32_t weekday;
        int32_t hour;
        int32_t minute;
        int32_t valid;
    } reach_clock_snapshot;

    reach_result reach_clock_create(reach_clock_port source, reach_clock **out_service);
    void reach_clock_destroy(reach_clock *service);

    int32_t reach_clock_tick(reach_clock *service);
    int32_t reach_clock_minute_elapsed(reach_clock *service);
    uint32_t reach_clock_next_minute_delay_ms(reach_clock *service);
    void reach_clock_snapshot_take(const reach_clock *service, reach_clock_snapshot *out_snapshot);

#ifdef __cplusplus
}
#endif

#endif
