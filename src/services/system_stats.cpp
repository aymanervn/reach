#include "reach/services/system_stats.h"

#include <new>

#define REACH_SYSTEM_STATS_INTERVAL_SECONDS 1.0

struct reach_system_stats
{
    reach_system_stats_port source;
    reach_system_stats_sample previous;
    int32_t has_previous;
    double elapsed_seconds;
    reach_system_stats_snapshot snapshot;
};

reach_result reach_system_stats_create(reach_system_stats_port source,
                                       reach_system_stats **out_service)
{
    if (out_service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_system_stats *service = new (std::nothrow) reach_system_stats();
    if (service == nullptr)
    {
        return REACH_ERROR;
    }

    service->source = source;
    service->elapsed_seconds = REACH_SYSTEM_STATS_INTERVAL_SECONDS;
    *out_service = service;
    return REACH_OK;
}

void reach_system_stats_destroy(reach_system_stats *service)
{
    if (service != nullptr && service->source.ops.destroy != nullptr)
    {
        service->source.ops.destroy(service->source.source);
    }
    delete service;
}

static uint64_t reach_system_stats_delta(uint64_t current, uint64_t previous)
{
    return current > previous ? current - previous : 0;
}

static float reach_system_stats_percent(uint64_t used, uint64_t total)
{
    if (total == 0)
    {
        return 0.0f;
    }
    float percent = (float)((double)used / (double)total * 100.0);
    if (percent < 0.0f)
    {
        return 0.0f;
    }
    return percent > 100.0f ? 100.0f : percent;
}

int32_t reach_system_stats_tick(reach_system_stats *service, double delta_seconds)
{
    if (service == nullptr || service->source.ops.sample == nullptr)
    {
        return 0;
    }

    service->elapsed_seconds += delta_seconds;
    if (service->elapsed_seconds < REACH_SYSTEM_STATS_INTERVAL_SECONDS)
    {
        return 0;
    }

    double interval = service->elapsed_seconds;
    service->elapsed_seconds = 0.0;

    reach_system_stats_sample sample = {};
    if (service->source.ops.sample(service->source.source, &sample) != REACH_OK)
    {
        return 0;
    }

    reach_system_stats_snapshot next = {};
    next.memory_percent =
        reach_system_stats_percent(sample.memory_used_bytes, sample.memory_total_bytes);

    if (service->has_previous)
    {
        uint64_t total_delta =
            reach_system_stats_delta(sample.cpu_total_time, service->previous.cpu_total_time);
        uint64_t idle_delta =
            reach_system_stats_delta(sample.cpu_idle_time, service->previous.cpu_idle_time);
        uint64_t busy_delta = total_delta > idle_delta ? total_delta - idle_delta : 0;
        next.cpu_percent = reach_system_stats_percent(busy_delta, total_delta);

        if (interval > 0.0)
        {
            uint64_t received = reach_system_stats_delta(sample.network_received_bytes,
                                                         service->previous.network_received_bytes);
            uint64_t sent = reach_system_stats_delta(sample.network_sent_bytes,
                                                     service->previous.network_sent_bytes);
            next.network_received_bytes_per_second = (uint64_t)((double)received / interval);
            next.network_sent_bytes_per_second = (uint64_t)((double)sent / interval);
        }
        next.valid = 1;
    }

    service->previous = sample;
    service->has_previous = 1;

    int32_t changed = next.valid != service->snapshot.valid ||
                      (int32_t)next.cpu_percent != (int32_t)service->snapshot.cpu_percent ||
                      (int32_t)next.memory_percent != (int32_t)service->snapshot.memory_percent ||
                      next.network_received_bytes_per_second !=
                          service->snapshot.network_received_bytes_per_second ||
                      next.network_sent_bytes_per_second !=
                          service->snapshot.network_sent_bytes_per_second;
    service->snapshot = next;
    return changed;
}

void reach_system_stats_snapshot_take(const reach_system_stats *service,
                                      reach_system_stats_snapshot *out_snapshot)
{
    if (out_snapshot == nullptr)
    {
        return;
    }
    *out_snapshot = service != nullptr ? service->snapshot : reach_system_stats_snapshot{};
}
