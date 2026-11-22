#ifndef REACH_ADAPTERS_WINDOWS_TOUCHPAD_FRAME_H
#define REACH_ADAPTERS_WINDOWS_TOUCHPAD_FRAME_H

#include <stdint.h>

/* One touchpad frame: touching contacts and their centroid, in the device's logical units. */
typedef struct reach_touchpad_frame
{
    uint32_t contact_count;
    float centroid_x;
    float centroid_y;
    float range_x;
    float range_y;
    /* When the frame arrived, in seconds on any monotonic clock. */
    double time_seconds;
} reach_touchpad_frame;

#endif
