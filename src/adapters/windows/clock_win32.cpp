#include "windows_adapters_internal.h"

#include <windows.h>

struct reach_clock_source
{
    int32_t placeholder;
};

static reach_clock_source reach_clock_instance = {};

static reach_result reach_clock_sample_local(reach_clock_source *source,
                                             reach_clock_sample *out_sample)
{
    (void)source;
    if (out_sample == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    SYSTEMTIME local = {};
    GetLocalTime(&local);

    *out_sample = {};
    out_sample->year = local.wYear;
    out_sample->month = local.wMonth;
    out_sample->day = local.wDay;
    out_sample->weekday = local.wDayOfWeek;
    out_sample->hour = local.wHour;
    out_sample->minute = local.wMinute;
    out_sample->second = local.wSecond;
    out_sample->millisecond = local.wMilliseconds;
    return REACH_OK;
}

static void reach_clock_destroy(reach_clock_source *source)
{
    (void)source;
}

reach_result reach_windows_create_clock(reach_clock_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    out_port->source = &reach_clock_instance;
    out_port->ops.sample_local = reach_clock_sample_local;
    out_port->ops.destroy = reach_clock_destroy;
    return REACH_OK;
}
