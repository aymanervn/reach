#ifndef REACH_SERVICES_MONITOR_TOPOLOGY_H
#define REACH_SERVICES_MONITOR_TOPOLOGY_H

#include <chrono>
#include <stdint.h>
#include <vector>

#include "reach/ports/monitor.h"

typedef enum reach_monitor_topology_result
{
    REACH_MONITOR_TOPOLOGY_FAILED = 0,
    REACH_MONITOR_TOPOLOGY_UNCHANGED = 1,
    REACH_MONITOR_TOPOLOGY_CHANGED = 2
} reach_monitor_topology_result;

typedef struct reach_monitor_topology
{
    std::chrono::steady_clock::time_point confirmation_due;
    int32_t hint_active;
    int32_t confirmation_scheduled;
    int32_t confirmation_attempted;
} reach_monitor_topology;

static inline void reach_monitor_topology_cancel(reach_monitor_topology *topology)
{
    topology->hint_active = 0;
    topology->confirmation_scheduled = 0;
    topology->confirmation_attempted = 0;
}

static inline void reach_monitor_topology_request(reach_monitor_topology *topology)
{
    topology->hint_active = 1;
    topology->confirmation_scheduled = 0;
    topology->confirmation_attempted = 0;
}

static inline void reach_monitor_topology_confirm_later(reach_monitor_topology *topology)
{
    topology->confirmation_due =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(150);
    topology->confirmation_scheduled = 1;
    topology->confirmation_attempted = 1;
}

static inline int32_t reach_monitor_topology_confirmation_due(
    const reach_monitor_topology *topology)
{
    return topology->confirmation_scheduled &&
           std::chrono::steady_clock::now() >= topology->confirmation_due;
}

static inline uint32_t reach_monitor_topology_confirmation_wait_ms(
    const reach_monitor_topology *topology)
{
    auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                         topology->confirmation_due - std::chrono::steady_clock::now())
                         .count();
    return remaining > 0 ? (uint32_t)remaining : 1u;
}

static inline int32_t reach_monitor_topology_rect_equal(reach_rect_i32 left,
                                                        reach_rect_i32 right)
{
    return left.left == right.left && left.top == right.top &&
           left.right == right.right && left.bottom == right.bottom;
}

static inline int32_t reach_monitor_topology_info_equal(const reach_monitor_info &left,
                                                        const reach_monitor_info &right)
{
    return left.id == right.id && left.primary == right.primary &&
           left.dpi_x == right.dpi_x && left.dpi_y == right.dpi_y &&
           left.refresh_rate_hz == right.refresh_rate_hz &&
           reach_monitor_topology_rect_equal(left.bounds, right.bounds) &&
           reach_monitor_topology_rect_equal(left.work_area, right.work_area);
}

static inline reach_monitor_topology_result reach_monitor_topology_refresh(
    reach_monitor_port *port)
{
    if (port == nullptr || port->list == nullptr || port->ops.refresh == nullptr ||
        port->ops.count == nullptr || port->ops.get == nullptr)
    {
        return REACH_MONITOR_TOPOLOGY_FAILED;
    }

    std::vector<reach_monitor_info> previous;
    size_t previous_count = port->ops.count(port->list);
    previous.reserve(previous_count);
    for (size_t index = 0; index < previous_count; ++index)
    {
        const reach_monitor_info *monitor = port->ops.get(port->list, index);
        if (monitor == nullptr)
        {
            return REACH_MONITOR_TOPOLOGY_FAILED;
        }
        previous.push_back(*monitor);
    }

    if (port->ops.refresh(port->list) != REACH_OK)
    {
        return REACH_MONITOR_TOPOLOGY_FAILED;
    }

    size_t current_count = port->ops.count(port->list);
    if (current_count == 0)
    {
        return REACH_MONITOR_TOPOLOGY_FAILED;
    }
    if (previous_count != current_count)
    {
        return REACH_MONITOR_TOPOLOGY_CHANGED;
    }

    std::vector<int32_t> matched(current_count, 0);
    for (const reach_monitor_info &old_monitor : previous)
    {
        int32_t found = 0;
        for (size_t index = 0; index < current_count; ++index)
        {
            const reach_monitor_info *current = port->ops.get(port->list, index);
            if (current == nullptr)
            {
                return REACH_MONITOR_TOPOLOGY_FAILED;
            }
            if (!matched[index] && reach_monitor_topology_info_equal(old_monitor, *current))
            {
                matched[index] = 1;
                found = 1;
                break;
            }
        }
        if (!found)
        {
            return REACH_MONITOR_TOPOLOGY_CHANGED;
        }
    }
    return REACH_MONITOR_TOPOLOGY_UNCHANGED;
}

#endif
