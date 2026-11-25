#ifndef REACH_WINDOW_ACTIONS_H
#define REACH_WINDOW_ACTIONS_H

#include "reach/support/layout.h"
#include "reach/support/util.h"

#include <windows.h>

reach_result reach_window_management_activate(HWND hwnd);
reach_result reach_window_management_activate_covered(HWND hwnd);
reach_result reach_window_management_activate_exact(HWND hwnd);
void reach_window_management_prepare_minimize(HWND hwnd);
reach_result reach_window_management_minimize(HWND hwnd);
reach_result reach_window_management_leave_game_to_desktop(HWND game);
reach_result reach_window_management_snap(HWND hwnd, reach_split_mode mode);
reach_result reach_window_management_close(HWND hwnd);

// Window-event thread only: wakes pending actions whose window reached the state they wait for.
void reach_window_management_note_window_event(HWND hwnd);
// Names the window-event thread, which must keep dispatching while an action waits on it.
void reach_window_management_set_event_thread(DWORD thread_id);

#endif
