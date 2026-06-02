#ifndef REACH_WINDOW_FILTER_WIN32_H
#define REACH_WINDOW_FILTER_WIN32_H

#include <windows.h>
#include <stdint.h>

int32_t reach_window_is_desktop_surface(HWND hwnd);
int32_t reach_window_is_app_candidate(HWND hwnd);
int32_t reach_window_is_app(HWND hwnd);
int32_t reach_window_is_displayed_app(HWND hwnd);

#endif
