#include "reach/platform/windows_adapters.h"

#include <windows.h>
#include <dwmapi.h>
#include <shlwapi.h>

#include <new>
#include <vector>

struct reach_window_manager {
    HWINEVENTHOOK create_hook;
    HWINEVENTHOOK destroy_hook;
    HWINEVENTHOOK foreground_hook;
    HWINEVENTHOOK show_hook;
    HWINEVENTHOOK hide_hook;
    HWINEVENTHOOK minimize_start_hook;
    HWINEVENTHOOK minimize_end_hook;
    HWINEVENTHOOK location_hook;
    HWND foreground;
    std::vector<reach_window_snapshot> windows;
    std::vector<HWND> window_order;
    std::vector<reach_window_snapshot> pending_windows;
    std::vector<HWND> hidden_minimized_windows;
    int32_t dirty;
};

static reach_window_manager *g_window_manager;

static int32_t reach_window_manager_contains_hwnd(const std::vector<HWND> &windows, HWND hwnd)
{
    for (HWND existing : windows) {
        if (existing == hwnd) {
            return 1;
        }
    }
    return 0;
}

static void reach_window_manager_remove_hwnd(std::vector<HWND> &windows, HWND hwnd)
{
    for (size_t index = 0; index < windows.size();) {
        if (windows[index] == hwnd) {
            windows.erase(windows.begin() + index);
        } else {
            ++index;
        }
    }
}

static void reach_window_manager_track_hidden_minimized(reach_window_manager *manager, HWND hwnd)
{
    if (manager == nullptr || hwnd == nullptr || !IsWindow(hwnd)) {
        return;
    }
    if (!reach_window_manager_contains_hwnd(manager->hidden_minimized_windows, hwnd)) {
        manager->hidden_minimized_windows.push_back(hwnd);
    }
}

static void reach_window_manager_set_reach_minimize_position(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return;
    }

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        return;
    }

    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    if (!GetWindowPlacement(hwnd, &placement)) {
        return;
    }

    placement.ptMinPosition.x = info.rcWork.left + (info.rcWork.right - info.rcWork.left) / 2;
    placement.ptMinPosition.y = info.rcWork.bottom - 24;
    placement.flags |= WPF_SETMINPOSITION;
    (void)SetWindowPlacement(hwnd, &placement);
}

static void CALLBACK reach_window_manager_event_proc(
    HWINEVENTHOOK hook,
    DWORD event,
    HWND window,
    LONG object_id,
    LONG child_id,
    DWORD event_thread,
    DWORD event_time)
{
    (void)hook;
    (void)event;
    (void)object_id;
    (void)child_id;
    (void)event_thread;
    (void)event_time;

    if (event != EVENT_SYSTEM_FOREGROUND && object_id != OBJID_WINDOW) {
        return;
    }
    if (event != EVENT_SYSTEM_FOREGROUND && event != EVENT_OBJECT_DESTROY) {
        if (window == nullptr || !IsWindow(window) || GetAncestor(window, GA_ROOT) != window) {
            return;
        }
    }
    if (event == EVENT_SYSTEM_MINIMIZESTART) {
        reach_window_manager_set_reach_minimize_position(window);
        if (g_window_manager != nullptr) {
            reach_window_manager_track_hidden_minimized(g_window_manager, window);
        }
        ShowWindowAsync(window, SW_HIDE);
    } else if (event == EVENT_SYSTEM_MINIMIZEEND || event == EVENT_OBJECT_SHOW || event == EVENT_OBJECT_DESTROY) {
        if (g_window_manager != nullptr) {
            reach_window_manager_remove_hwnd(g_window_manager->hidden_minimized_windows, window);
        }
    }

    if (g_window_manager != nullptr) {
        g_window_manager->foreground = GetForegroundWindow();
        g_window_manager->dirty = 1;
    }
}

static void reach_window_manager_unhook(HWINEVENTHOOK *hook)
{
    if (hook != nullptr && *hook != nullptr) {
        UnhookWinEvent(*hook);
        *hook = nullptr;
    }
}

static reach_result reach_window_manager_stop(reach_window_manager *manager);

static reach_result reach_window_manager_start(reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    if (manager == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    (void)reach_window_manager_stop(manager);
    g_window_manager = manager;
    manager->create_hook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_CREATE, nullptr, reach_window_manager_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    manager->destroy_hook = SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, nullptr, reach_window_manager_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    manager->foreground_hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, reach_window_manager_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    manager->show_hook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, nullptr, reach_window_manager_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    manager->hide_hook = SetWinEventHook(EVENT_OBJECT_HIDE, EVENT_OBJECT_HIDE, nullptr, reach_window_manager_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    manager->minimize_start_hook = SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZESTART, nullptr, reach_window_manager_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    manager->minimize_end_hook = SetWinEventHook(EVENT_SYSTEM_MINIMIZEEND, EVENT_SYSTEM_MINIMIZEEND, nullptr, reach_window_manager_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    manager->location_hook = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr, reach_window_manager_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    return manager->create_hook != nullptr &&
        manager->destroy_hook != nullptr &&
        manager->foreground_hook != nullptr &&
        manager->show_hook != nullptr &&
        manager->hide_hook != nullptr &&
        manager->minimize_start_hook != nullptr &&
        manager->minimize_end_hook != nullptr &&
        manager->location_hook != nullptr
        ? REACH_OK
        : REACH_ERROR;
}

static reach_result reach_window_manager_stop(reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    if (manager == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_window_manager_unhook(&manager->create_hook);
    reach_window_manager_unhook(&manager->destroy_hook);
    reach_window_manager_unhook(&manager->foreground_hook);
    reach_window_manager_unhook(&manager->show_hook);
    reach_window_manager_unhook(&manager->hide_hook);
    reach_window_manager_unhook(&manager->minimize_start_hook);
    reach_window_manager_unhook(&manager->minimize_end_hook);
    reach_window_manager_unhook(&manager->location_hook);
    if (g_window_manager == manager) {
        g_window_manager = nullptr;
    }
    return REACH_OK;
}

static int32_t reach_window_manager_is_reach_window(HWND hwnd)
{
    wchar_t class_name[64] = {};
    GetClassNameW(hwnd, class_name, 64);
    return lstrcmpiW(class_name, L"ReachPlatformWindow") == 0 ||
        lstrcmpiW(class_name, L"ReachInputMessageWindow") == 0 ||
        lstrcmpiW(class_name, L"ReachWallpaperWindow") == 0;
}

static int32_t reach_window_manager_is_desktop_surface_window(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return 0;
    }
    if (hwnd == GetShellWindow()) {
        return 1;
    }

    wchar_t class_name[64] = {};
    GetClassNameW(hwnd, class_name, 64);
    return lstrcmpiW(class_name, L"Progman") == 0 ||
        lstrcmpiW(class_name, L"WorkerW") == 0 ||
        lstrcmpiW(class_name, L"ReachWallpaperWindow") == 0;
}

static int32_t reach_window_manager_contains(const std::vector<HWND> &windows, HWND hwnd)
{
    return reach_window_manager_contains_hwnd(windows, hwnd);
}

static int32_t reach_window_manager_query_process_path(HWND hwnd, uint16_t *out_path, size_t out_path_count)
{
    if (out_path == nullptr || out_path_count == 0) {
        return 0;
    }
    out_path[0] = 0;
    DWORD process_id = 0;
    GetWindowThreadProcessId(hwnd, &process_id);
    if (process_id == 0 || process_id == GetCurrentProcessId()) {
        return 0;
    }
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr) {
        return 0;
    }
    wchar_t path[260] = {};
    DWORD path_count = 260;
    BOOL ok = QueryFullProcessImageNameW(process, 0, path, &path_count);
    CloseHandle(process);
    if (!ok || path[0] == 0) {
        return 0;
    }
    return reach_copy_utf16(out_path, out_path_count, reinterpret_cast<const uint16_t *>(path)) == REACH_OK;
}

static int32_t reach_window_manager_is_app_candidate(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd) || reach_window_manager_is_reach_window(hwnd)) {
        return 0;
    }
    if (GetWindow(hwnd, GW_OWNER) != nullptr) {
        return 0;
    }
    LONG_PTR ex_style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((ex_style & WS_EX_TOOLWINDOW) != 0) {
        return 0;
    }
    return 1;
}

static int32_t reach_window_manager_is_app_window(HWND hwnd)
{
    if (!reach_window_manager_is_app_candidate(hwnd)) {
        return 0;
    }
    if (!IsWindowVisible(hwnd) && !IsIconic(hwnd)) {
        return 0;
    }
    return 1;
}

static int32_t reach_window_manager_is_cloaked(HWND hwnd)
{
    DWORD cloaked = 0;
    HRESULT hr = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    return SUCCEEDED(hr) && cloaked != 0;
}

static int32_t reach_window_manager_is_displayed_app_window(HWND hwnd)
{
    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    if (GetWindowPlacement(hwnd, &placement) && placement.showCmd == SW_SHOWMINIMIZED) {
        return 0;
    }
    return reach_window_manager_is_app_window(hwnd) &&
        IsWindowVisible(hwnd) &&
        !IsIconic(hwnd) &&
        !reach_window_manager_is_cloaked(hwnd);
}

static BOOL CALLBACK reach_find_primary_monitor_proc(HMONITOR monitor, HDC dc, LPRECT rect, LPARAM param)
{
    (void)dc;
    (void)rect;
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info) && (info.dwFlags & MONITORINFOF_PRIMARY) != 0) {
        HMONITOR *out_monitor = reinterpret_cast<HMONITOR *>(param);
        if (out_monitor != nullptr) {
            *out_monitor = monitor;
        }
        return FALSE;
    }
    return TRUE;
}

static HMONITOR reach_window_manager_primary_monitor(void)
{
    HMONITOR primary = nullptr;
    EnumDisplayMonitors(nullptr, nullptr, reach_find_primary_monitor_proc, reinterpret_cast<LPARAM>(&primary));
    if (primary == nullptr) {
        POINT point = { 0, 0 };
        primary = MonitorFromPoint(point, MONITOR_DEFAULTTOPRIMARY);
    }
    return primary;
}

static int32_t reach_window_manager_is_on_primary_monitor(HWND hwnd)
{
    HMONITOR primary = reach_window_manager_primary_monitor();
    if (primary == nullptr) {
        return 0;
    }
    HMONITOR window_monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    return window_monitor == primary;
}

static int32_t reach_window_manager_any_visible_maximized_on_primary(void)
{
    for (HWND hwnd = GetTopWindow(nullptr); hwnd != nullptr; hwnd = GetWindow(hwnd, GW_HWNDNEXT)) {
        if (reach_window_manager_is_desktop_surface_window(hwnd)) {
            return 0;
        }
        if (!reach_window_manager_is_displayed_app_window(hwnd) || !IsZoomed(hwnd)) {
            continue;
        }
        if (reach_window_manager_is_on_primary_monitor(hwnd)) {
            return 1;
        }
    }
    return 0;
}

static int32_t reach_window_manager_build_snapshot(HWND hwnd, int32_t force_hidden_minimized, reach_window_snapshot *out_snapshot)
{
    if (out_snapshot == nullptr || hwnd == nullptr || !IsWindow(hwnd)) {
        return 0;
    }

    reach_window_snapshot snapshot = {};
    snapshot.id = reinterpret_cast<uintptr_t>(hwnd);
    snapshot.visible = IsWindowVisible(hwnd) ? 1 : 0;
    snapshot.maximized = IsZoomed(hwnd) ? 1 : 0;
    snapshot.minimized = force_hidden_minimized ? 1 : (IsIconic(hwnd) ? 1 : 0);
    RECT rect = {};
    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    /* Use normal placement for minimized windows to avoid icon-position jumps. */
    if (snapshot.minimized && GetWindowPlacement(hwnd, &placement)) {
        rect = placement.rcNormalPosition;
    } else {
        (void)GetWindowRect(hwnd, &rect);
    }
    snapshot.bounds = { rect.left, rect.top, rect.right, rect.bottom };
    wchar_t title[260] = {};
    GetWindowTextW(hwnd, title, 260);
    (void)reach_copy_utf16(snapshot.title, 260, reinterpret_cast<const uint16_t *>(title));
    if (!reach_window_manager_query_process_path(hwnd, snapshot.path, 260)) {
        return 0;
    }

    *out_snapshot = snapshot;
    return 1;
}

static BOOL CALLBACK reach_window_manager_enum_windows_proc(HWND hwnd, LPARAM param)
{
    reach_window_manager *manager = reinterpret_cast<reach_window_manager *>(param);
    if (manager == nullptr || !reach_window_manager_is_app_window(hwnd)) {
        return TRUE;
    }

    reach_window_snapshot snapshot = {};
    if (reach_window_manager_build_snapshot(hwnd, 0, &snapshot)) {
        if (snapshot.minimized) {
            reach_window_manager_track_hidden_minimized(manager, hwnd);
            ShowWindowAsync(hwnd, SW_HIDE);
        }
        manager->pending_windows.push_back(snapshot);
    }
    return TRUE;
}

static const reach_window_snapshot *reach_window_manager_find_pending(
    const std::vector<reach_window_snapshot> &windows,
    HWND hwnd)
{
    uintptr_t id = reinterpret_cast<uintptr_t>(hwnd);
    for (const reach_window_snapshot &snapshot : windows) {
        if (snapshot.id == id) {
            return &snapshot;
        }
    }
    return nullptr;
}

static void reach_window_manager_refresh_windows(reach_window_manager *manager)
{
    if (manager == nullptr) {
        return;
    }
    manager->pending_windows.clear();
    EnumWindows(reach_window_manager_enum_windows_proc, reinterpret_cast<LPARAM>(manager));

    for (size_t index = 0; index < manager->hidden_minimized_windows.size();) {
        HWND hwnd = manager->hidden_minimized_windows[index];
        if (hwnd == nullptr || !IsWindow(hwnd) || !reach_window_manager_is_app_candidate(hwnd)) {
            manager->hidden_minimized_windows.erase(manager->hidden_minimized_windows.begin() + index);
            continue;
        }
        if (!reach_window_manager_find_pending(manager->pending_windows, hwnd)) {
            reach_window_snapshot snapshot = {};
            if (reach_window_manager_build_snapshot(hwnd, 1, &snapshot)) {
                manager->pending_windows.push_back(snapshot);
            }
        }
        ++index;
    }

    for (size_t index = 0; index < manager->window_order.size();) {
        if (reach_window_manager_find_pending(manager->pending_windows, manager->window_order[index]) == nullptr) {
            manager->window_order.erase(manager->window_order.begin() + index);
        } else {
            ++index;
        }
    }

    for (const reach_window_snapshot &snapshot : manager->pending_windows) {
        HWND hwnd = reinterpret_cast<HWND>(snapshot.id);
        if (!reach_window_manager_contains(manager->window_order, hwnd)) {
            manager->window_order.push_back(hwnd);
        }
    }

    manager->windows.clear();
    for (HWND hwnd : manager->window_order) {
        const reach_window_snapshot *snapshot = reach_window_manager_find_pending(manager->pending_windows, hwnd);
        if (snapshot != nullptr) {
            manager->windows.push_back(*snapshot);
        }
    }
    manager->pending_windows.clear();
}

static reach_result reach_window_manager_refresh(reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    if (manager == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    manager->foreground = GetForegroundWindow();
    reach_window_manager_refresh_windows(manager);
    manager->dirty = 0;
    return REACH_OK;
}

static reach_result reach_window_manager_snap(reach_window_manager *manager, uintptr_t window_id, reach_split_mode mode)
{
    REACH_ASSERT(manager != nullptr);
    REACH_ASSERT(window_id != 0);
    if (manager == nullptr || window_id == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    HWND hwnd = reinterpret_cast<HWND>(window_id);
    if (!IsWindow(hwnd)) {
        return REACH_INVALID_ARGUMENT;
    }

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        return REACH_ERROR;
    }

    reach_rect_i32 work_area = { info.rcWork.left, info.rcWork.top, info.rcWork.right, info.rcWork.bottom };
    reach_rect_i32 target = {};
    reach_result result = reach_layout_compute_split(work_area, mode, &target);
    if (result != REACH_OK) {
        return result;
    }

    if (IsZoomed(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }

    BOOL ok = SetWindowPos(hwnd, nullptr, target.left, target.top, target.right - target.left, target.bottom - target.top, SWP_NOZORDER | SWP_NOACTIVATE);
    manager->foreground = GetForegroundWindow();
    return ok ? REACH_OK : REACH_ERROR;
}

static uintptr_t reach_window_manager_foreground(const reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    return reinterpret_cast<uintptr_t>(manager->foreground);
}

static int32_t reach_window_manager_foreground_is_maximized(const reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    return manager != nullptr &&
        reach_window_manager_is_app_window(manager->foreground) &&
        IsZoomed(manager->foreground);
}

static int32_t reach_window_manager_is_window_fullscreen_on_primary(HWND hwnd)
{
    if (!reach_window_manager_is_displayed_app_window(hwnd)) {
        return 0;
    }

    RECT win_rect = {};
    if (!GetWindowRect(hwnd, &win_rect)) {
        return 0;
    }

    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (mon == nullptr) {
        return 0;
    }

    HMONITOR primary = reach_window_manager_primary_monitor();
    if (primary == nullptr || mon != primary) {
        return 0;
    }

    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(mon, &mi)) {
        return 0;
    }

    return (win_rect.left <= mi.rcWork.left &&
            win_rect.top <= mi.rcWork.top &&
            win_rect.right >= mi.rcWork.right &&
            win_rect.bottom >= mi.rcWork.bottom);
}

static int32_t reach_window_manager_any_visible_fullscreen_on_primary(void)
{
    for (HWND hwnd = GetTopWindow(nullptr); hwnd != nullptr; hwnd = GetWindow(hwnd, GW_HWNDNEXT)) {
        if (reach_window_manager_is_desktop_surface_window(hwnd)) {
            return 0;
        }
        if (reach_window_manager_is_window_fullscreen_on_primary(hwnd)) {
            return 1;
        }
    }
    return 0;
}

static int32_t reach_window_manager_is_window_exclusive_fullscreen(HWND hwnd)
{
    if (!reach_window_manager_is_displayed_app_window(hwnd)) {
        return 0;
    }

    BOOL excluded = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXCLUDED_FROM_PEEK, &excluded, sizeof(excluded)))) {
        if (excluded) {
            return 1;
        }
    }
    return 0;
}

static int32_t reach_window_manager_any_visible_exclusive_fullscreen_on_primary(void)
{
    for (HWND hwnd = GetTopWindow(nullptr); hwnd != nullptr; hwnd = GetWindow(hwnd, GW_HWNDNEXT)) {
        if (reach_window_manager_is_desktop_surface_window(hwnd)) {
            return 0;
        }
        if (reach_window_manager_is_window_exclusive_fullscreen(hwnd) &&
            reach_window_manager_is_on_primary_monitor(hwnd)) {
            return 1;
        }
    }
    return 0;
}

static int32_t reach_window_manager_foreground_is_fullscreen(const reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    if (manager == nullptr) {
        return 0;
    }
    return reach_window_manager_is_window_fullscreen_on_primary(manager->foreground);
}

static int32_t reach_window_manager_foreground_is_exclusive_fullscreen(const reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    if (manager == nullptr) {
        return 0;
    }
    return reach_window_manager_is_window_exclusive_fullscreen(manager->foreground);
}

static int32_t reach_window_manager_dock_should_auto_hide(const reach_window_manager *manager)
{
    if (manager == nullptr) {
        return 0;
    }
    if (reach_window_manager_any_visible_exclusive_fullscreen_on_primary()) {
        return 1;
    }
    if (reach_window_manager_any_visible_fullscreen_on_primary()) {
        return 1;
    }
    return reach_window_manager_any_visible_maximized_on_primary();
}

static int32_t reach_window_manager_needs_refresh(const reach_window_manager *manager)
{
    return manager != nullptr && manager->dirty;
}

static size_t reach_window_manager_window_count(const reach_window_manager *manager)
{
    return manager == nullptr ? 0 : manager->windows.size();
}

static reach_result reach_window_manager_window_at(const reach_window_manager *manager, size_t index, reach_window_snapshot *out_window)
{
    if (manager == nullptr || out_window == nullptr || index >= manager->windows.size()) {
        return REACH_INVALID_ARGUMENT;
    }
    *out_window = manager->windows[index];
    return REACH_OK;
}

static reach_result reach_window_manager_activate(reach_window_manager *manager, uintptr_t window_id)
{
    if (manager == nullptr || window_id == 0) {
        return REACH_INVALID_ARGUMENT;
    }
    HWND hwnd = reinterpret_cast<HWND>(window_id);
    if (!IsWindow(hwnd)) {
        return REACH_INVALID_ARGUMENT;
    }
    int32_t hidden_minimized = reach_window_manager_contains_hwnd(manager->hidden_minimized_windows, hwnd);
    if (hidden_minimized) {
        reach_window_manager_remove_hwnd(manager->hidden_minimized_windows, hwnd);
        ShowWindowAsync(hwnd, SW_SHOW);
        ShowWindowAsync(hwnd, SW_RESTORE);
    } else if (IsIconic(hwnd)) {
        WINDOWPLACEMENT placement = {};
        placement.length = sizeof(placement);
        int32_t was_maximized = GetWindowPlacement(hwnd, &placement) &&
            (placement.showCmd == SW_SHOWMAXIMIZED ||
             (placement.flags & WPF_RESTORETOMAXIMIZED) != 0);
        ShowWindowAsync(hwnd, was_maximized ? SW_SHOWMAXIMIZED : SW_RESTORE);
    } else if (!IsWindowVisible(hwnd)) {
        ShowWindowAsync(hwnd, SW_SHOW);
    }
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetForegroundWindow(hwnd);
    if (GetForegroundWindow() != hwnd) {
        HWND foreground = GetForegroundWindow();
        DWORD foreground_thread = foreground != nullptr ? GetWindowThreadProcessId(foreground, nullptr) : 0;
        DWORD target_thread = GetWindowThreadProcessId(hwnd, nullptr);
        DWORD current_thread = GetCurrentThreadId();
        BOOL attached_foreground = foreground_thread != 0 && foreground_thread != current_thread
            ? AttachThreadInput(current_thread, foreground_thread, TRUE)
            : FALSE;
        BOOL attached_target = target_thread != 0 && target_thread != current_thread
            ? AttachThreadInput(current_thread, target_thread, TRUE)
            : FALSE;
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetForegroundWindow(hwnd);
        if (attached_target) {
            AttachThreadInput(current_thread, target_thread, FALSE);
        }
        if (attached_foreground) {
            AttachThreadInput(current_thread, foreground_thread, FALSE);
        }
    }
    manager->foreground = hwnd;
    (void)reach_window_manager_refresh(manager);
    return REACH_OK;
}

static void reach_window_manager_preserve_restore_placement(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd) || IsIconic(hwnd)) {
        return;
    }

    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    if (!GetWindowPlacement(hwnd, &placement)) {
        return;
    }

    if (!IsZoomed(hwnd)) {
        RECT rect = {};
        if (GetWindowRect(hwnd, &rect)) {
            placement.rcNormalPosition = rect;
        }
        placement.showCmd = SW_SHOWNORMAL;
    } else {
        placement.showCmd = SW_SHOWMAXIMIZED;
    }
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) {
        placement.ptMinPosition.x = info.rcWork.left + (info.rcWork.right - info.rcWork.left) / 2;
        placement.ptMinPosition.y = info.rcWork.bottom - 24;
        placement.flags |= WPF_SETMINPOSITION;
    }
    (void)SetWindowPlacement(hwnd, &placement);
}

static reach_result reach_window_manager_minimize(reach_window_manager *manager, uintptr_t window_id)
{
    if (manager == nullptr || window_id == 0) {
        return REACH_INVALID_ARGUMENT;
    }
    HWND hwnd = reinterpret_cast<HWND>(window_id);
    if (!IsWindow(hwnd)) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_window_manager_preserve_restore_placement(hwnd);
    ShowWindowAsync(hwnd, SW_MINIMIZE);
    reach_window_manager_track_hidden_minimized(manager, hwnd);
    ShowWindowAsync(hwnd, SW_HIDE);
    manager->foreground = GetForegroundWindow();
    return reach_window_manager_refresh(manager);
}

static reach_result reach_window_manager_close(reach_window_manager *manager, uintptr_t window_id)
{
    if (manager == nullptr || window_id == 0) {
        return REACH_INVALID_ARGUMENT;
    }
    HWND hwnd = reinterpret_cast<HWND>(window_id);
    if (!IsWindow(hwnd)) {
        return REACH_INVALID_ARGUMENT;
    }
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
    return REACH_OK;
}

static void reach_window_manager_destroy(reach_window_manager *manager)
{
    if (manager != nullptr) {
        (void)reach_window_manager_stop(manager);
    }
    delete manager;
}

reach_result reach_windows_create_window_manager(reach_window_manager_port *out_port)
{
    REACH_ASSERT(out_port != nullptr);
    if (out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_window_manager *manager = new (std::nothrow) reach_window_manager();
    if (manager == nullptr) {
        return REACH_ERROR;
    }

    manager->foreground = GetForegroundWindow();
    reach_window_manager_refresh_windows(manager);
    out_port->manager = manager;
    out_port->ops.start = reach_window_manager_start;
    out_port->ops.stop = reach_window_manager_stop;
    out_port->ops.refresh = reach_window_manager_refresh;
    out_port->ops.snap = reach_window_manager_snap;
    out_port->ops.foreground = reach_window_manager_foreground;
    out_port->ops.foreground_is_maximized = reach_window_manager_foreground_is_maximized;
    out_port->ops.foreground_is_fullscreen = reach_window_manager_foreground_is_fullscreen;
    out_port->ops.foreground_is_exclusive_fullscreen = reach_window_manager_foreground_is_exclusive_fullscreen;
    out_port->ops.dock_should_auto_hide = reach_window_manager_dock_should_auto_hide;
    out_port->ops.needs_refresh = reach_window_manager_needs_refresh;
    out_port->ops.window_count = reach_window_manager_window_count;
    out_port->ops.window_at = reach_window_manager_window_at;
    out_port->ops.activate = reach_window_manager_activate;
    out_port->ops.minimize = reach_window_manager_minimize;
    out_port->ops.close = reach_window_manager_close;
    out_port->ops.destroy = reach_window_manager_destroy;
    return REACH_OK;
}
