#include "touchpad_swipe.h"

#include "reach/support/util.h"

#include <math.h>

static const uint32_t REACH_TOUCHPAD_SWIPE_FINGERS = 3;

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
        swipe->step_anchor_x = frame->centroid_x;
        swipe->direction = dx >= 0.0f ? 1.0f : -1.0f;
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
    /* Travel is measured along the starting direction, so a left swipe mirrors a right one. */
    const float step = REACH_TOUCHPAD_SWIPE_STEP_FRACTION * frame->range_x * swipe->direction;
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

    const uint32_t contacts = frame->contact_count;
    switch (swipe->phase)
    {
    case REACH_TOUCHPAD_SWIPE_IDLE:
        if (contacts == REACH_TOUCHPAD_SWIPE_FINGERS)
        {
            swipe->origin_x = frame->centroid_x;
            swipe->origin_y = frame->centroid_y;
            swipe->phase = REACH_TOUCHPAD_SWIPE_TRACKING;
        }
        break;
    case REACH_TOUCHPAD_SWIPE_TRACKING:
        if (contacts < REACH_TOUCHPAD_SWIPE_FINGERS)
        {
            swipe->phase = REACH_TOUCHPAD_SWIPE_IDLE;
        }
        else if (contacts > REACH_TOUCHPAD_SWIPE_FINGERS)
        {
            swipe->phase = REACH_TOUCHPAD_SWIPE_SETTLED;
        }
        else
        {
            reach_touchpad_swipe_track(swipe, frame, out_events, capacity, &count);
        }
        break;
    case REACH_TOUCHPAD_SWIPE_SWITCHING:
        if (contacts < REACH_TOUCHPAD_SWIPE_FINGERS)
        {
            reach_touchpad_swipe_emit(REACH_UI_EVENT_APP_SWITCH_COMMIT, out_events, capacity,
                                      &count);
            swipe->phase = contacts == 0 ? REACH_TOUCHPAD_SWIPE_IDLE : REACH_TOUCHPAD_SWIPE_SETTLED;
        }
        else if (contacts > REACH_TOUCHPAD_SWIPE_FINGERS)
        {
            reach_touchpad_swipe_emit(REACH_UI_EVENT_APP_SWITCH_CANCEL, out_events, capacity,
                                      &count);
            swipe->phase = REACH_TOUCHPAD_SWIPE_SETTLED;
        }
        else
        {
            reach_touchpad_swipe_step(swipe, frame, out_events, capacity, &count);
        }
        break;
    case REACH_TOUCHPAD_SWIPE_SETTLED:
        if (contacts == 0)
        {
            swipe->phase = REACH_TOUCHPAD_SWIPE_IDLE;
        }
        break;
    }
    return count;
}

void reach_touchpad_swipe_abandon(reach_touchpad_swipe *swipe)
{
    if (swipe != nullptr)
    {
        swipe->phase = REACH_TOUCHPAD_SWIPE_SETTLED;
    }
}
