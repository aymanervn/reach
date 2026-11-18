#ifndef REACH_PORTS_SYSTEM_STATS_H
#define REACH_PORTS_SYSTEM_STATS_H

#include <stddef.h>
#include <stdint.h>

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_system_stats_source reach_system_stats_source;

    typedef struct reach_system_stats_sample
    {
        uint64_t cpu_idle_time;
        uint64_t cpu_total_time;
        uint64_t memory_used_bytes;
        uint64_t memory_total_bytes;
        uint64_t network_received_bytes;
        uint64_t network_sent_bytes;
    } reach_system_stats_sample;

    typedef struct reach_system_stats_ops
    {
        reach_result (*sample)(reach_system_stats_source *source,
                               reach_system_stats_sample *out_sample);
        void (*destroy)(reach_system_stats_source *source);
    } reach_system_stats_ops;

    typedef struct reach_system_stats_port
    {
        reach_system_stats_source *source;
        reach_system_stats_ops ops;
    } reach_system_stats_port;

#ifdef __cplusplus
}
#endif

#endif
