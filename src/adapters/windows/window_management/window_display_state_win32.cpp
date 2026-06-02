#include "window_display_state_win32.h"

#include "window_filter_win32.h"

#include <dwmapi.h>

static BOOL CALLBACK reach_find_primary_monitor_proc(HMONITOR monitor, HDC dc, LPRECT rect,
                                                     LPARAM param)
{
    (void)dc;
    (void)rect;
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info) && (info.dwFlags & MONITORINFOF_PRIMARY) != 0)
    {
        HMONITOR *out_monitor = reinterpret_cast<HMONITOR *>(param);
        if (out_monitor != nullptr)
        {
            *out_monitor = monitor;
        }
        return FALSE;
    }
    return TRUE;
}

static HMONITOR reach_primary_monitor(void)
{
    HMONITOR primary = nullptr;
    EnumDisplayMonitors(nullptr, nullptr, reach_find_primary_monitor_proc,
                        reinterpret_cast<LPARAM>(&primary));
    if (primary == nullptr)
    {
        POINT point = {0, 0};
        primary = MonitorFromPoint(point, MONITOR_DEFAULTTOPRIMARY);
    }
    return primary;
}

int32_t reach_window_is_on_primary_monitor(HWND hwnd)
{
    HMONITOR primary = reach_primary_monitor();
    if (primary == nullptr)
    {
        return 0;
    }
    HMONITOR window_monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    return window_monitor == primary;
}

int32_t reach_window_is_fullscreen_on_primary(HWND hwnd)
{
    if (!reach_window_is_displayed_app(hwnd))
    {
        return 0;
    }

    RECT win_rect = {};
    if (!GetWindowRect(hwnd, &win_rect))
    {
        return 0;
    }

    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (mon == nullptr)
    {
        return 0;
    }

    HMONITOR primary = reach_primary_monitor();
    if (primary == nullptr || mon != primary)
    {
        return 0;
    }

    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(mon, &mi))
    {
        return 0;
    }

    return win_rect.left <= mi.rcWork.left && win_rect.top <= mi.rcWork.top &&
           win_rect.right >= mi.rcWork.right && win_rect.bottom >= mi.rcWork.bottom;
}

int32_t reach_window_any_visible_fullscreen_on_primary(void)
{
    for (HWND hwnd = GetTopWindow(nullptr); hwnd != nullptr; hwnd = GetWindow(hwnd, GW_HWNDNEXT))
    {
        if (reach_window_is_desktop_surface(hwnd))
        {
            continue;
        }
        if (reach_window_is_fullscreen_on_primary(hwnd))
        {
            return 1;
        }
    }
    return 0;
}

int32_t reach_window_is_exclusive_fullscreen(HWND hwnd)
{
    if (!reach_window_is_displayed_app(hwnd))
    {
        return 0;
    }

    BOOL excluded = FALSE;
    if (SUCCEEDED(
            DwmGetWindowAttribute(hwnd, DWMWA_EXCLUDED_FROM_PEEK, &excluded, sizeof(excluded))))
    {
        if (excluded)
        {
            return 1;
        }
    }
    return 0;
}

int32_t reach_window_any_visible_exclusive_fullscreen_on_primary(void)
{
    for (HWND hwnd = GetTopWindow(nullptr); hwnd != nullptr; hwnd = GetWindow(hwnd, GW_HWNDNEXT))
    {
        if (reach_window_is_desktop_surface(hwnd))
        {
            continue;
        }
        if (reach_window_is_exclusive_fullscreen(hwnd) && reach_window_is_on_primary_monitor(hwnd))
        {
            return 1;
        }
    }
    return 0;
}

int32_t reach_window_any_visible_maximized_on_primary(void)
{
    for (HWND hwnd = GetTopWindow(nullptr); hwnd != nullptr; hwnd = GetWindow(hwnd, GW_HWNDNEXT))
    {
        if (reach_window_is_desktop_surface(hwnd))
        {
            continue;
        }
        if (!reach_window_is_displayed_app(hwnd) || !IsZoomed(hwnd))
        {
            continue;
        }
        if (reach_window_is_on_primary_monitor(hwnd))
        {
            return 1;
        }
    }
    return 0;
}
