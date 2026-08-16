#ifndef REACH_SERVICES_SYSTEM_STATS_H
#define REACH_SERVICES_SYSTEM_STATS_H

#include <stdint.h>

#include "reach/ports/system_stats.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_system_stats reach_system_stats;

    typedef struct reach_system_stats_snapshot
    {
        float cpu_percent;
        float memory_percent;
        uint64_t network_received_bytes_per_second;
        uint64_t network_sent_bytes_per_second;
        int32_t valid;
    } reach_system_stats_snapshot;

    reach_result reach_system_stats_create(reach_system_stats_port source,
                                           reach_system_stats **out_service);
    void reach_system_stats_destroy(reach_system_stats *service);

    int32_t reach_system_stats_tick(reach_system_stats *service, double delta_seconds);
    void reach_system_stats_snapshot_take(const reach_system_stats *service,
                                          reach_system_stats_snapshot *out_snapshot);

#ifdef __cplusplus
}
#endif

#endif
