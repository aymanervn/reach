#ifndef REACH_PORTS_CLOCK_H
#define REACH_PORTS_CLOCK_H

#include <stdint.h>

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_clock_source reach_clock_source;

    typedef struct reach_clock_sample
    {
        int32_t year;
        int32_t month;
        int32_t day;
        int32_t weekday;
        int32_t hour;
        int32_t minute;
        int32_t second;
        int32_t millisecond;
    } reach_clock_sample;

    typedef struct reach_clock_ops
    {
        reach_result (*sample_local)(reach_clock_source *source, reach_clock_sample *out_sample);
        void (*destroy)(reach_clock_source *source);
    } reach_clock_ops;

    typedef struct reach_clock_port
    {
        reach_clock_source *source;
        reach_clock_ops ops;
    } reach_clock_port;

#ifdef __cplusplus
}
#endif

#endif
