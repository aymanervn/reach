#ifndef REACH_TOOLS_REACH_SERVICE_TOUCHPAD_SWIPE_H
#define REACH_TOOLS_REACH_SERVICE_TOUCHPAD_SWIPE_H

#include "../../adapters/windows/touchpad_frame.h"

#include "reach/core/ui_events.h"

#include <stddef.h>
#include <stdint.h>

/* Recognizes three-finger swipes from touchpad frames. No OS calls: one update per frame.
   Horizontal swipes drive the switcher (begin, step per distance, commit when a finger lifts).
   The step shrinks as the fingers move faster, so a quick swipe reaches further while a slow
   drag (or slow drift back before lifting) stays precise.
   The highlight follows the fingers: a right swipe starts on the previous app, a left swipe on
   the app used longest ago, and every step moves the selection the way the fingers move.
   Vertical swipes fire SWIPE_UP or SWIPE_DOWN once. A fourth contact abandons the gesture. */

static const float REACH_TOUCHPAD_SWIPE_LOCK_FRACTION = 0.08f;
/* Step per app, as a fraction of the touchpad width, for slow and fast horizontal travel. */
static const float REACH_TOUCHPAD_SWIPE_SLOW_STEP_FRACTION = 0.20f;
static const float REACH_TOUCHPAD_SWIPE_FAST_STEP_FRACTION = 0.10f;
/* Horizontal speed, in touchpad widths per second, at or below which the slow step applies, and
   at or above which the fast step applies. Between them the step is interpolated. */
static const float REACH_TOUCHPAD_SWIPE_SLOW_SPEED = 0.15f;
static const float REACH_TOUCHPAD_SWIPE_FAST_SPEED = 0.6f;

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
    /* Smoothed horizontal speed in touchpad widths per second. */
    float speed;
    float last_x;
    double last_time_seconds;
} reach_touchpad_swipe;

/* The current step as a fraction of the touchpad width, from the smoothed speed. */
float reach_touchpad_swipe_step_fraction(const reach_touchpad_swipe *swipe);

/* Returns the number of events written to out_events, in the order they must be posted. */
size_t reach_touchpad_swipe_update(reach_touchpad_swipe *swipe, const reach_touchpad_frame *frame,
                                   reach_ui_event_type *out_events, size_t capacity);

/* True while a horizontal swipe has the switcher open. */
int32_t reach_touchpad_swipe_switching(const reach_touchpad_swipe *swipe);

/* Drops the current gesture without events; the swipe stays quiet until every finger lifts. */
void reach_touchpad_swipe_abandon(reach_touchpad_swipe *swipe);

#endif
