#include "reach/core/blink.h"

#include <math.h>

void reach_blink_stop(reach_blink *blink)
{
    if (blink != 0)
    {
        blink->started_seconds = 0.0;
        blink->deadline_seconds = 0.0;
        blink->visible = 0;
    }
}

void reach_blink_reset_at(reach_blink *blink, double visible_seconds, double hidden_seconds,
                          double now_seconds)
{
    if (blink == 0)
        return;
    if (!isfinite(now_seconds) || now_seconds < 0.0 || !isfinite(visible_seconds) ||
        !isfinite(hidden_seconds) || visible_seconds <= 0.0 || hidden_seconds <= 0.0 ||
        !isfinite(now_seconds + visible_seconds + hidden_seconds) ||
        now_seconds + visible_seconds <= now_seconds ||
        now_seconds + visible_seconds + hidden_seconds <= now_seconds + visible_seconds)
    {
        reach_blink_stop(blink);
        return;
    }
    blink->started_seconds = now_seconds;
    blink->deadline_seconds = now_seconds + visible_seconds;
    blink->visible_seconds = visible_seconds;
    blink->hidden_seconds = hidden_seconds;
    blink->visible = 1;
}

int32_t reach_blink_update_at(reach_blink *blink, double now_seconds)
{
    if (blink == 0 || blink->deadline_seconds <= 0.0 || !isfinite(now_seconds) ||
        now_seconds < blink->deadline_seconds)
        return 0;
    double period = blink->visible_seconds + blink->hidden_seconds;
    double cycle =
        blink->started_seconds + floor((now_seconds - blink->started_seconds) / period) * period;
    int32_t visible = now_seconds < cycle + blink->visible_seconds;
    double deadline = cycle + (visible ? blink->visible_seconds : period);
    if (deadline <= now_seconds)
    {
        visible = 1;
        deadline += blink->visible_seconds;
    }
    int32_t changed = blink->visible != visible;
    blink->visible = visible;
    blink->deadline_seconds = deadline;
    return changed;
}
