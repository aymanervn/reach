#ifndef REACH_WINDOW_FOCUS_WIN32_H
#define REACH_WINDOW_FOCUS_WIN32_H

#include <windows.h>

#include <stdint.h>

enum reach_window_focus_flags
{
    REACH_WINDOW_FOCUS_RAISE = 1u,
    REACH_WINDOW_FOCUS_KEYBOARD = 2u,
};

// The thread whose input queue owns hwnd, for AttachThreadInput; 0 when unknown.
DWORD reach_window_input_thread(HWND hwnd);

// Calling thread: any. Hands the foreground to target past the foreground lock. RAISE also brings
// it to the top of the z-order; KEYBOARD gives it keyboard focus unless the app already placed
// focus inside it. Returns 1 when target or one of its owned windows is foreground afterwards.
int32_t reach_window_take_foreground(HWND target, uint32_t flags);

#endif
