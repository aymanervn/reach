#include "reach/features/common/marquee.h"

#include "reach/support/animation.h"

static const float REACH_MARQUEE_SPEED_PIXELS_PER_SECOND = 26.0f;
static const double REACH_MARQUEE_HOLD_SECONDS = 1.6;
static const float REACH_MARQUEE_MIN_OVERFLOW = 1.0f;

void reach_marquee_reset(reach_marquee_state *state)
{
    if (state != nullptr)
    {
        state->elapsed_seconds = 0.0;
    }
}

static float reach_marquee_overflow(const reach_marquee_request *request)
{
    if (request == nullptr)
    {
        return 0.0f;
    }
    float overflow = request->content_width - request->viewport_width;
    return overflow > REACH_MARQUEE_MIN_OVERFLOW ? overflow : 0.0f;
}

int32_t reach_marquee_scrolls(const reach_marquee_request *request)
{
    return reach_marquee_overflow(request) > 0.0f;
}

float reach_marquee_advance(reach_marquee_state *state, const reach_marquee_request *request)
{
    if (state == nullptr || request == nullptr)
    {
        return 0.0f;
    }

    float overflow = reach_marquee_overflow(request);
    if (overflow <= 0.0f)
    {
        reach_marquee_reset(state);
        return 0.0f;
    }

    double travel_seconds = (double)overflow / (double)REACH_MARQUEE_SPEED_PIXELS_PER_SECOND;
    double cycle_seconds = (REACH_MARQUEE_HOLD_SECONDS + travel_seconds) * 2.0;

    state->elapsed_seconds += request->delta_seconds > 0.0 ? request->delta_seconds : 0.0;
    while (state->elapsed_seconds >= cycle_seconds)
    {
        state->elapsed_seconds -= cycle_seconds;
    }

    double phase = state->elapsed_seconds;
    if (phase < REACH_MARQUEE_HOLD_SECONDS)
    {
        return 0.0f;
    }
    phase -= REACH_MARQUEE_HOLD_SECONDS;
    if (phase < travel_seconds)
    {
        return -overflow *
               reach_easing_apply((float)(phase / travel_seconds), REACH_EASING_EASE_IN_OUT);
    }
    phase -= travel_seconds;
    if (phase < REACH_MARQUEE_HOLD_SECONDS)
    {
        return -overflow;
    }
    phase -= REACH_MARQUEE_HOLD_SECONDS;
    return -overflow *
           (1.0f - reach_easing_apply((float)(phase / travel_seconds), REACH_EASING_EASE_IN_OUT));
}
