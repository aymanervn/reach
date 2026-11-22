#include "touchpad_swipe.h"

#include "reach/support/util.h"

#include <math.h>

static const uint32_t REACH_TOUCHPAD_SWIPE_FINGERS = 3;
/* Speed smoothing time constant, and the longest frame gap still treated as continuous motion. */
static const double REACH_TOUCHPAD_SWIPE_SPEED_SMOOTHING_SECONDS = 0.05;
static const double REACH_TOUCHPAD_SWIPE_MAX_FRAME_GAP_SECONDS = 0.25;

float reach_touchpad_swipe_step_fraction(const reach_touchpad_swipe *swipe)
{
    const float span = REACH_TOUCHPAD_SWIPE_FAST_SPEED - REACH_TOUCHPAD_SWIPE_SLOW_SPEED;
    float t = (swipe->speed - REACH_TOUCHPAD_SWIPE_SLOW_SPEED) / span;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return REACH_TOUCHPAD_SWIPE_SLOW_STEP_FRACTION +
           (REACH_TOUCHPAD_SWIPE_FAST_STEP_FRACTION - REACH_TOUCHPAD_SWIPE_SLOW_STEP_FRACTION) * t;
}

static void reach_touchpad_swipe_track_speed(reach_touchpad_swipe *swipe,
                                             const reach_touchpad_frame *frame)
{
    const double dt = frame->time_seconds - swipe->last_time_seconds;
    if (dt > 0.0 && dt <= REACH_TOUCHPAD_SWIPE_MAX_FRAME_GAP_SECONDS)
    {
        const float instant = fabsf(frame->centroid_x - swipe->last_x) / frame->range_x / (float)dt;
        const float alpha = (float)(dt / (dt + REACH_TOUCHPAD_SWIPE_SPEED_SMOOTHING_SECONDS));
        swipe->speed += alpha * (instant - swipe->speed);
    }
    swipe->last_x = frame->centroid_x;
    swipe->last_time_seconds = frame->time_seconds;
}

static void reach_touchpad_swipe_emit(reach_ui_event_type type, reach_ui_event_type *out_events,
                                      size_t capacity, size_t *count)
{
    if (*count < capacity)
    {
        out_events[(*count)++] = type;
    }
}

static void reach_touchpad_swipe_track(reach_touchpad_swipe *swipe,
                                       const reach_touchpad_frame *frame,
                                       reach_ui_event_type *out_events, size_t capacity,
                                       size_t *count)
{
    const float dx = (frame->centroid_x - swipe->origin_x) / frame->range_x;
    const float dy = (frame->centroid_y - swipe->origin_y) / frame->range_y;
    if (fabsf(dx) < REACH_TOUCHPAD_SWIPE_LOCK_FRACTION &&
        fabsf(dy) < REACH_TOUCHPAD_SWIPE_LOCK_FRACTION)
    {
        return;
    }

    if (fabsf(dx) >= fabsf(dy))
    {
        reach_touchpad_swipe_emit(REACH_UI_EVENT_APP_SWITCH_BEGIN, out_events, capacity, count);
        if (dx < 0.0f)
        {
            /* BEGIN selects the previous app; two steps back wrap to the far end, like
               Alt+Shift+Tab, so a left swipe moves the highlight left from there. */
            reach_touchpad_swipe_emit(REACH_UI_EVENT_APP_SWITCH_PREVIOUS, out_events, capacity,
                                      count);
            reach_touchpad_swipe_emit(REACH_UI_EVENT_APP_SWITCH_PREVIOUS, out_events, capacity,
                                      count);
        }
        swipe->step_anchor_x = frame->centroid_x;
        swipe->phase = REACH_TOUCHPAD_SWIPE_SWITCHING;
        return;
    }

    /* Touchpad y grows downward. */
    reach_touchpad_swipe_emit(dy < 0.0f ? REACH_UI_EVENT_SWIPE_UP : REACH_UI_EVENT_SWIPE_DOWN,
                              out_events, capacity, count);
    swipe->phase = REACH_TOUCHPAD_SWIPE_SETTLED;
}

static void reach_touchpad_swipe_step(reach_touchpad_swipe *swipe,
                                      const reach_touchpad_frame *frame,
                                      reach_ui_event_type *out_events, size_t capacity,
                                      size_t *count)
{
    /* Rightward travel moves the highlight right (older apps), leftward moves it left. */
    const float step = reach_touchpad_swipe_step_fraction(swipe) * frame->range_x;
    while ((frame->centroid_x - swipe->step_anchor_x) / step >= 1.0f && *count < capacity)
    {
        reach_touchpad_swipe_emit(REACH_UI_EVENT_APP_SWITCH_NEXT, out_events, capacity, count);
        swipe->step_anchor_x += step;
    }
    while ((swipe->step_anchor_x - frame->centroid_x) / step >= 1.0f && *count < capacity)
    {
        reach_touchpad_swipe_emit(REACH_UI_EVENT_APP_SWITCH_PREVIOUS, out_events, capacity, count);
        swipe->step_anchor_x -= step;
    }
}

size_t reach_touchpad_swipe_update(reach_touchpad_swipe *swipe, const reach_touchpad_frame *frame,
                                   reach_ui_event_type *out_events, size_t capacity)
{
    size_t count = 0;
    if (swipe == nullptr || frame == nullptr || out_events == nullptr)
    {
        return 0;
    }
    REACH_ASSERT(frame->range_x > 0.0f && frame->range_y > 0.0f);
    if (frame->range_x <= 0.0f || frame->range_y <= 0.0f)
    {
        return 0;
    }

    /* Only confident contacts start or steer a gesture; any extra contact, palm included,
       abandons it. */
    const uint32_t contacts = frame->contact_count;
    const uint32_t touching = frame->contact_count + frame->uncertain_count;
    switch (swipe->phase)
    {
    case REACH_TOUCHPAD_SWIPE_IDLE:
        if (contacts == REACH_TOUCHPAD_SWIPE_FINGERS && touching == REACH_TOUCHPAD_SWIPE_FINGERS)
        {
            swipe->origin_x = frame->centroid_x;
            swipe->origin_y = frame->centroid_y;
            swipe->speed = 0.0f;
            swipe->last_x = frame->centroid_x;
            swipe->last_time_seconds = frame->time_seconds;
            swipe->phase = REACH_TOUCHPAD_SWIPE_TRACKING;
        }
        break;
    case REACH_TOUCHPAD_SWIPE_TRACKING:
        if (touching > REACH_TOUCHPAD_SWIPE_FINGERS)
        {
            swipe->phase = REACH_TOUCHPAD_SWIPE_SETTLED;
        }
        else if (contacts < REACH_TOUCHPAD_SWIPE_FINGERS)
        {
            swipe->phase = REACH_TOUCHPAD_SWIPE_IDLE;
        }
        else
        {
            reach_touchpad_swipe_track_speed(swipe, frame);
            reach_touchpad_swipe_track(swipe, frame, out_events, capacity, &count);
        }
        break;
    case REACH_TOUCHPAD_SWIPE_SWITCHING:
        if (touching > REACH_TOUCHPAD_SWIPE_FINGERS)
        {
            reach_touchpad_swipe_emit(REACH_UI_EVENT_APP_SWITCH_CANCEL, out_events, capacity,
                                      &count);
            swipe->phase = REACH_TOUCHPAD_SWIPE_SETTLED;
        }
        else if (contacts < REACH_TOUCHPAD_SWIPE_FINGERS)
        {
            reach_touchpad_swipe_emit(REACH_UI_EVENT_APP_SWITCH_COMMIT, out_events, capacity,
                                      &count);
            swipe->phase = touching == 0 ? REACH_TOUCHPAD_SWIPE_IDLE : REACH_TOUCHPAD_SWIPE_SETTLED;
        }
        else
        {
            reach_touchpad_swipe_track_speed(swipe, frame);
            reach_touchpad_swipe_step(swipe, frame, out_events, capacity, &count);
        }
        break;
    case REACH_TOUCHPAD_SWIPE_SETTLED:
        if (touching == 0)
        {
            swipe->phase = REACH_TOUCHPAD_SWIPE_IDLE;
        }
        break;
    }
    return count;
}

int32_t reach_touchpad_swipe_switching(const reach_touchpad_swipe *swipe)
{
    return swipe != nullptr && swipe->phase == REACH_TOUCHPAD_SWIPE_SWITCHING;
}

void reach_touchpad_swipe_abandon(reach_touchpad_swipe *swipe)
{
    if (swipe != nullptr)
    {
        swipe->phase = REACH_TOUCHPAD_SWIPE_SETTLED;
    }
}
