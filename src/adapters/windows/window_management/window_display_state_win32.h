#ifndef REACH_WINDOW_DISPLAY_STATE_WIN32_H
#define REACH_WINDOW_DISPLAY_STATE_WIN32_H

#include <windows.h>
#include <stdint.h>

int32_t reach_window_is_on_primary_monitor(HWND hwnd);
int32_t reach_window_is_fullscreen_on_primary(HWND hwnd);
int32_t reach_window_is_exclusive_fullscreen(HWND hwnd);
int32_t reach_window_any_visible_maximized_on_primary(void);
int32_t reach_window_any_visible_fullscreen_on_primary(void);
int32_t reach_window_any_visible_exclusive_fullscreen_on_primary(void);

#endif
