#ifndef REACH_ADAPTERS_WINDOWS_TOUCHPAD_REPORTS_WIN32_H
#define REACH_ADAPTERS_WINDOWS_TOUCHPAD_REPORTS_WIN32_H

#include "touchpad_frame.h"

#include <stddef.h>
#include <stdint.h>

#include <windows.h>

/* Precision touchpads deliver raw HID reports. A report carries a few finger slots, and in hybrid
   mode one touch frame spans several reports: only the first carries the frame's contact count.
   This assembles reports into frames. Centroid and ranges are in the device's logical units. */

typedef struct reach_touchpad_reports reach_touchpad_reports;

reach_touchpad_reports *reach_touchpad_reports_create(void);

void reach_touchpad_reports_destroy(reach_touchpad_reports *reports);

typedef void (*reach_touchpad_frame_callback)(const reach_touchpad_frame *frame, void *user);

/* Calls callback once per frame the input completes, in order, stamping each with time_seconds.
   Returns the number of frames delivered. */
size_t reach_touchpad_reports_feed(reach_touchpad_reports *reports, HRAWINPUT input,
                                   double time_seconds, reach_touchpad_frame_callback callback,
                                   void *user);

#endif
