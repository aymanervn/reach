#ifndef REACH_ADAPTERS_WINDOWS_TOUCHPAD_REPORTS_WIN32_H
#define REACH_ADAPTERS_WINDOWS_TOUCHPAD_REPORTS_WIN32_H

#include <stdint.h>

#include <windows.h>

/* Precision touchpads deliver raw HID reports. A report carries a few finger slots, and in hybrid
   mode one touch frame spans several reports: only the first carries the frame's contact count.
   This assembles reports into frames. Centroid and ranges are in the device's logical units. */

typedef struct reach_touchpad_frame
{
    uint32_t contact_count;
    float centroid_x;
    float centroid_y;
    float range_x;
    float range_y;
} reach_touchpad_frame;

typedef struct reach_touchpad_reports reach_touchpad_reports;

reach_touchpad_reports *reach_touchpad_reports_create(void);

void reach_touchpad_reports_destroy(reach_touchpad_reports *reports);

/* Returns 1 when out_frame holds a completed frame. */
int32_t reach_touchpad_reports_feed(reach_touchpad_reports *reports, HRAWINPUT input,
                                    reach_touchpad_frame *out_frame);

#endif
