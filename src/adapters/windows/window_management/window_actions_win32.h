#ifndef REACH_WINDOW_ACTIONS_WIN32_H
#define REACH_WINDOW_ACTIONS_WIN32_H

#include "../windows_adapters_internal.h"

#include <windows.h>

reach_result reach_window_management_activate(HWND hwnd);
reach_result reach_window_management_minimize(HWND hwnd);
reach_result reach_window_management_snap(HWND hwnd, reach_split_mode mode);
reach_result reach_window_management_close(HWND hwnd);

#endif
