#include "reach/core/frame_clock.h"

#include <math.h>

int32_t reach_frame_rate_resolve(int32_t adaptive, int32_t refresh_rate_hz,
                                 int32_t battery_saver_on)
{
    return adaptive && !battery_saver_on && refresh_rate_hz > 0 ? refresh_rate_hz : 60;
}

uint32_t reach_frame_clock_wait_ms(reach_frame_clock *clock, double now_seconds, int32_t rate_hz,
                                   int32_t active)
{
    if (!active)
    {
        *clock = (reach_frame_clock){0};
        return UINT32_MAX;
    }
    rate_hz = rate_hz > 0 ? rate_hz : 60;
    if (!clock->active)
    {
        clock->active = 1;
        clock->rate_hz = rate_hz;
        clock->next_frame_seconds = now_seconds;
    }
    else if (clock->rate_hz != rate_hz)
    {
        clock->rate_hz = rate_hz;
        clock->next_frame_seconds = now_seconds + 1.0 / rate_hz;
    }
    double remaining_ms = (clock->next_frame_seconds - now_seconds) * 1000.0;
    return remaining_ms > 0.0 ? (uint32_t)ceil(remaining_ms) : 0;
}

void reach_frame_clock_presented(reach_frame_clock *clock, double now_seconds)
{
    if (!clock->active || clock->rate_hz <= 0)
    {
        return;
    }
    double period = 1.0 / clock->rate_hz;
    if (now_seconds >= clock->next_frame_seconds)
    {
        double elapsed_periods = floor((now_seconds - clock->next_frame_seconds) / period);
        clock->next_frame_seconds += (elapsed_periods + 1.0) * period;
    }
}
