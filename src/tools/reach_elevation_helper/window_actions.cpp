#include "window_actions.h"

struct reach_window_action_state
{
    int32_t is_window;
    int32_t visible;
    int32_t iconic;
    int32_t zoomed;
    UINT show_cmd;
    RECT rect;
    POINT min_position;
};

static HWND reach_window_management_activation_target(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return nullptr;
    }

    HWND target = hwnd;
    HWND root_owner = GetAncestor(hwnd, GA_ROOTOWNER);
    if (root_owner != nullptr && IsWindow(root_owner))
    {
        target = root_owner;
    }

    HWND popup = GetLastActivePopup(target);
    if (popup != nullptr && IsWindow(popup) && IsWindowVisible(popup))
    {
        target = popup;
    }

    return target;
}

static reach_window_action_state reach_window_management_capture_state(HWND hwnd)
{
    reach_window_action_state state = {};
    state.is_window = hwnd != nullptr && IsWindow(hwnd) ? 1 : 0;
    if (!state.is_window)
    {
        return state;
    }

    state.visible = IsWindowVisible(hwnd) ? 1 : 0;
    state.iconic = IsIconic(hwnd) ? 1 : 0;
    state.zoomed = IsZoomed(hwnd) ? 1 : 0;
    (void)GetWindowRect(hwnd, &state.rect);

    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    if (GetWindowPlacement(hwnd, &placement))
    {
        state.show_cmd = placement.showCmd;
        state.min_position = placement.ptMinPosition;
    }

    return state;
}

static int32_t reach_window_management_rect_equalish(RECT a, RECT b)
{
    const LONG tolerance = 2;
    auto abs_long = [](LONG value) -> LONG
    { return value < 0 ? -value : value; };
    return abs_long(a.left - b.left) <= tolerance && abs_long(a.top - b.top) <= tolerance &&
           abs_long(a.right - b.right) <= tolerance && abs_long(a.bottom - b.bottom) <= tolerance;
}

static int32_t reach_window_management_wait_for(HWND hwnd,
                                                int32_t (*predicate)(HWND, const RECT *),
                                                const RECT *expected, DWORD timeout_ms)
{
    DWORD start = GetTickCount();
    for (;;)
    {
        if (predicate(hwnd, expected))
        {
            return 1;
        }
        if (GetTickCount() - start >= timeout_ms)
        {
            return 0;
        }
        Sleep(25);
    }
}

static int32_t reach_window_management_minimized(HWND hwnd, const RECT *expected)
{
    (void)expected;
    if (hwnd == nullptr || !IsWindow(hwnd) || !IsIconic(hwnd))
    {
        return 0;
    }

    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    return GetWindowPlacement(hwnd, &placement) && placement.showCmd == SW_SHOWMINIMIZED;
}

static int32_t reach_window_management_restored(HWND hwnd, const RECT *expected)
{
    (void)expected;
    if (hwnd == nullptr || !IsWindow(hwnd) || IsIconic(hwnd))
    {
        return 0;
    }

    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    if (!GetWindowPlacement(hwnd, &placement))
    {
        return 0;
    }

    return placement.showCmd == SW_SHOWNORMAL || placement.showCmd == SW_SHOWMAXIMIZED ||
           placement.showCmd == SW_SHOW || placement.showCmd == SW_RESTORE;
}

static int32_t reach_window_management_rect_matches(HWND hwnd, const RECT *expected)
{
    if (hwnd == nullptr || expected == nullptr || !IsWindow(hwnd) || IsIconic(hwnd))
    {
        return 0;
    }

    RECT actual = {};
    return GetWindowRect(hwnd, &actual) && reach_window_management_rect_equalish(actual, *expected);
}

static int32_t reach_window_management_closed(HWND hwnd, const RECT *expected)
{
    (void)expected;
    return hwnd == nullptr || !IsWindow(hwnd);
}

static void reach_window_management_log_failure(const char *action, HWND hwnd,
                                                const reach_window_action_state *before,
                                                const reach_window_action_state *after)
{
    (void)action;
    (void)hwnd;
    (void)before;
    (void)after;
}

static int32_t reach_window_management_foreground_matches(HWND target)
{
    HWND foreground = GetForegroundWindow();
    return target != nullptr && foreground != nullptr &&
           (foreground == target || GetAncestor(foreground, GA_ROOT) == target ||
            GetAncestor(foreground, GA_ROOTOWNER) == target);
}

static int32_t reach_window_management_focus_target(HWND target)
{
    if (target == nullptr || !IsWindow(target))
    {
        return 0;
    }

    DWORD target_thread = GetWindowThreadProcessId(target, nullptr);
    DWORD foreground_thread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    DWORD current_thread = GetCurrentThreadId();

    BOOL attached_target = FALSE;
    BOOL attached_foreground = FALSE;
    if (target_thread != 0 && target_thread != current_thread)
    {
        attached_target = AttachThreadInput(current_thread, target_thread, TRUE);
    }
    if (foreground_thread != 0 && foreground_thread != current_thread &&
        foreground_thread != target_thread)
    {
        attached_foreground = AttachThreadInput(current_thread, foreground_thread, TRUE);
    }

    (void)SetActiveWindow(target);
    (void)SetFocus(target);
    BOOL foreground_ok = SetForegroundWindow(target);
    BringWindowToTop(target);

    if (attached_foreground)
    {
        (void)AttachThreadInput(current_thread, foreground_thread, FALSE);
    }
    if (attached_target)
    {
        (void)AttachThreadInput(current_thread, target_thread, FALSE);
    }

    return foreground_ok || reach_window_management_foreground_matches(target);
}

reach_result reach_window_management_activate(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (IsIconic(hwnd))
    {
        reach_window_action_state before = reach_window_management_capture_state(hwnd);
        WINDOWPLACEMENT placement = {};
        placement.length = sizeof(placement);
        const bool restore_to_maximized = GetWindowPlacement(hwnd, &placement) &&
                                          (placement.showCmd == SW_SHOWMAXIMIZED ||
                                           (placement.flags & WPF_RESTORETOMAXIMIZED) != 0);

        ShowWindow(hwnd, restore_to_maximized ? SW_SHOWMAXIMIZED : SW_RESTORE);
        if (!reach_window_management_wait_for(hwnd, reach_window_management_restored, nullptr, 750))
        {
            reach_window_action_state after = reach_window_management_capture_state(hwnd);
            reach_window_management_log_failure("activate.restore", hwnd, &before, &after);
            return REACH_ERROR;
        }
    }
    else if (!IsWindowVisible(hwnd))
    {
        ShowWindow(hwnd, SW_SHOW);
    }

    HWND target = reach_window_management_activation_target(hwnd);
    if (target == nullptr)
    {
        return REACH_ERROR;
    }

    SetWindowPos(target, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetWindowPos(target, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    BringWindowToTop(target);

    if (reach_window_management_focus_target(target))
    {
        return REACH_OK;
    }

    reach_window_action_state state = reach_window_management_capture_state(target);
    reach_window_management_log_failure("activate.foreground", target, &state, &state);
    return REACH_ERROR;
}

reach_result reach_window_management_minimize(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return REACH_INVALID_ARGUMENT;
    }

    HWND target = hwnd;

    reach_window_action_state before = reach_window_management_capture_state(target);
    (void)ShowWindowAsync(target, SW_MINIMIZE);
    (void)PostMessageW(target, WM_SYSCOMMAND, SC_MINIMIZE, 0);

    if (reach_window_management_wait_for(target, reach_window_management_minimized, nullptr, 750))
    {
        return REACH_OK;
    }

    reach_window_action_state after = reach_window_management_capture_state(target);
    reach_window_management_log_failure("minimize", target, &before, &after);
    return REACH_ERROR;
}

reach_result reach_window_management_snap(HWND hwnd, reach_split_mode mode)
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return REACH_INVALID_ARGUMENT;
    }

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info))
    {
        return REACH_ERROR;
    }

    reach_rect_i32 work_area = {info.rcWork.left, info.rcWork.top, info.rcWork.right,
                                info.rcWork.bottom};
    reach_rect_i32 target = {};
    reach_result result = reach_layout_compute_split(work_area, mode, &target);
    if (result != REACH_OK)
    {
        return result;
    }

    if (IsZoomed(hwnd))
    {
        ShowWindow(hwnd, SW_RESTORE);
        if (!reach_window_management_wait_for(hwnd, reach_window_management_restored, nullptr, 750))
        {
            reach_window_action_state after = reach_window_management_capture_state(hwnd);
            reach_window_management_log_failure("snap.restore", hwnd, &after, &after);
            return REACH_ERROR;
        }
    }

    reach_window_action_state before = reach_window_management_capture_state(hwnd);
    RECT expected = {target.left, target.top, target.right, target.bottom};
    BOOL ok = SetWindowPos(hwnd, nullptr, target.left, target.top, target.right - target.left,
                           target.bottom - target.top, SWP_NOZORDER | SWP_NOACTIVATE);
    if (ok && reach_window_management_wait_for(hwnd, reach_window_management_rect_matches,
                                               &expected, 750))
    {
        return REACH_OK;
    }

    reach_window_action_state after = reach_window_management_capture_state(hwnd);
    reach_window_management_log_failure("snap", hwnd, &before, &after);
    return REACH_ERROR;
}

reach_result reach_window_management_close(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_window_action_state before = reach_window_management_capture_state(hwnd);
    if (!PostMessageW(hwnd, WM_CLOSE, 0, 0))
    {
        reach_window_action_state after = reach_window_management_capture_state(hwnd);
        reach_window_management_log_failure("close.post", hwnd, &before, &after);
        return REACH_ERROR;
    }

    if (reach_window_management_wait_for(hwnd, reach_window_management_closed, nullptr, 750))
    {
        return REACH_OK;
    }

    reach_window_action_state after = reach_window_management_capture_state(hwnd);
    reach_window_management_log_failure("close", hwnd, &before, &after);
    return REACH_ERROR;
}
