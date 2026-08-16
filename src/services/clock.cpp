#include "reach/services/clock.h"

#include <new>

struct reach_clock
{
    reach_clock_port source;
    reach_clock_snapshot snapshot;
};

reach_result reach_clock_create(reach_clock_port source, reach_clock **out_service)
{
    if (out_service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_clock *service = new (std::nothrow) reach_clock();
    if (service == nullptr)
    {
        return REACH_ERROR;
    }

    service->source = source;
    *out_service = service;
    return REACH_OK;
}

void reach_clock_destroy(reach_clock *service)
{
    if (service != nullptr && service->source.ops.destroy != nullptr)
    {
        service->source.ops.destroy(service->source.source);
    }
    delete service;
}

int32_t reach_clock_tick(reach_clock *service)
{
    if (service == nullptr || service->source.ops.sample_local == nullptr)
    {
        return 0;
    }

    reach_clock_sample sample = {};
    if (service->source.ops.sample_local(service->source.source, &sample) != REACH_OK)
    {
        return 0;
    }

    reach_clock_snapshot next = {};
    next.year = sample.year;
    next.month = sample.month;
    next.day = sample.day;
    next.weekday = sample.weekday;
    next.hour = sample.hour;
    next.minute = sample.minute;
    next.valid = 1;

    int32_t changed = !service->snapshot.valid || next.minute != service->snapshot.minute ||
                      next.hour != service->snapshot.hour || next.day != service->snapshot.day ||
                      next.month != service->snapshot.month || next.year != service->snapshot.year;
    service->snapshot = next;
    return changed;
}

void reach_clock_snapshot_take(const reach_clock *service, reach_clock_snapshot *out_snapshot)
{
    if (out_snapshot == nullptr)
    {
        return;
    }
    *out_snapshot = service != nullptr ? service->snapshot : reach_clock_snapshot{};
}
