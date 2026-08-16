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

static int32_t reach_clock_sample_now(reach_clock *service, reach_clock_sample *out_sample)
{
    return service != nullptr && service->source.ops.sample_local != nullptr &&
           service->source.ops.sample_local(service->source.source, out_sample) == REACH_OK;
}

static int32_t reach_clock_sample_differs(const reach_clock_snapshot *snapshot,
                                          const reach_clock_sample *sample)
{
    return !snapshot->valid || sample->minute != snapshot->minute ||
           sample->hour != snapshot->hour || sample->day != snapshot->day ||
           sample->month != snapshot->month || sample->year != snapshot->year;
}

int32_t reach_clock_minute_elapsed(reach_clock *service)
{
    reach_clock_sample sample = {};
    if (!reach_clock_sample_now(service, &sample))
    {
        return 0;
    }
    return reach_clock_sample_differs(&service->snapshot, &sample);
}

uint32_t reach_clock_next_minute_delay_ms(reach_clock *service)
{
    reach_clock_sample sample = {};
    if (!reach_clock_sample_now(service, &sample))
    {
        return REACH_CLOCK_WAIT_FOREVER;
    }

    int32_t into_minute = sample.second * 1000 + sample.millisecond;
    if (into_minute < 0 || into_minute >= 60000)
    {
        return 1;
    }
    return (uint32_t)(60000 - into_minute);
}

int32_t reach_clock_tick(reach_clock *service)
{
    reach_clock_sample sample = {};
    if (!reach_clock_sample_now(service, &sample))
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

    int32_t changed = reach_clock_sample_differs(&service->snapshot, &sample);
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
