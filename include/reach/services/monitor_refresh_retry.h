#ifndef REACH_SERVICES_MONITOR_REFRESH_RETRY_H
#define REACH_SERVICES_MONITOR_REFRESH_RETRY_H

#include <chrono>
#include <stdint.h>

typedef struct reach_monitor_refresh_retry
{
    std::chrono::steady_clock::time_point next_attempt;
    uint32_t failures;
    int32_t scheduled;
} reach_monitor_refresh_retry;

static inline void reach_monitor_refresh_retry_reset(reach_monitor_refresh_retry *retry)
{
    retry->failures = 0;
    retry->scheduled = 0;
}

static inline int32_t reach_monitor_refresh_retry_defer(reach_monitor_refresh_retry *retry)
{
    ++retry->failures;
    if (retry->failures > 2)
    {
        retry->scheduled = 0;
        return 0;
    }

    retry->next_attempt = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(150 * retry->failures);
    retry->scheduled = 1;
    return 1;
}

static inline int32_t reach_monitor_refresh_retry_due(const reach_monitor_refresh_retry *retry)
{
    return retry->scheduled && std::chrono::steady_clock::now() >= retry->next_attempt;
}

static inline uint32_t reach_monitor_refresh_retry_wait_ms(
    const reach_monitor_refresh_retry *retry)
{
    auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                         retry->next_attempt - std::chrono::steady_clock::now())
                         .count();
    return remaining > 0 ? (uint32_t)remaining : 1u;
}

#endif
