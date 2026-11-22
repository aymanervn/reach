#ifndef REACH_TOOLS_REACH_SERVICE_TOUCHPAD_H
#define REACH_TOOLS_REACH_SERVICE_TOUCHPAD_H

#include <stdint.h>

/* Reads the precision touchpad in the elevated helper, which keeps receiving every contact while
   an elevated window has focus, and appends recognized swipes to the shared hotkey queue.
   Start and stop on the keyboard-hook thread: its message loop delivers the raw input. */
void reach_helper_touchpad_start(int32_t (*alt_tab_active)(void));
void reach_helper_touchpad_stop(void);

#endif
