#ifndef REACH_ADAPTERS_WINDOWS_TOUCHPAD_SWIPE_H
#define REACH_ADAPTERS_WINDOWS_TOUCHPAD_SWIPE_H

#include "touchpad_frame.h"

#include "reach/core/ui_events.h"

#include <stddef.h>
#include <stdint.h>

/* Recognizes three-finger swipes from touchpad frames. No OS calls: one update per frame.
   Horizontal swipes drive the switcher (begin, step per distance, commit when a finger lifts).
   Left and right mirror each other: travel in the starting direction moves to older apps,
   travel back walks toward the current one.
   Vertical swipes fire SWIPE_UP or SWIPE_DOWN once. A fourth contact abandons the gesture. */

static const float REACH_TOUCHPAD_SWIPE_LOCK_FRACTION = 0.08f;
static const float REACH_TOUCHPAD_SWIPE_STEP_FRACTION = 0.25f;

typedef enum reach_touchpad_swipe_phase
{
    REACH_TOUCHPAD_SWIPE_IDLE = 0,
    REACH_TOUCHPAD_SWIPE_TRACKING = 1,
    REACH_TOUCHPAD_SWIPE_SWITCHING = 2,
    /* Fired or abandoned: nothing more until every finger lifts. */
    REACH_TOUCHPAD_SWIPE_SETTLED = 3
} reach_touchpad_swipe_phase;

/* Zero-initialized is idle. */
typedef struct reach_touchpad_swipe
{
    reach_touchpad_swipe_phase phase;
    float origin_x;
    float origin_y;
    float step_anchor_x;
    /* +1 when the switch started rightward, -1 leftward. */
    float direction;
} reach_touchpad_swipe;

/* Returns the number of events written to out_events, in the order they must be posted. */
size_t reach_touchpad_swipe_update(reach_touchpad_swipe *swipe, const reach_touchpad_frame *frame,
                                   reach_ui_event_type *out_events, size_t capacity);

/* Drops the current gesture without events; the swipe stays quiet until every finger lifts. */
void reach_touchpad_swipe_abandon(reach_touchpad_swipe *swipe);

#endif
