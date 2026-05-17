#include "reach/platform/windows_adapters.h"

#include <windows.h>
#include <shlwapi.h>

#include <new>
#include <vector>

struct reach_window_manager {
    HWINEVENTHOOK create_hook;
    HWINEVENTHOOK destroy_hook;
    HWINEVENTHOOK foreground_hook;
    HWND foreground;
    std::vector<HWND> maximized_windows;
    std::vector<reach_window_snapshot> windows;
    std::vector<HWND> managed_minimized;
};

static reach_window_manager *g_window_manager;

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

    if (g_window_manager != nullptr && window != nullptr && IsWindow(window)) {
        g_window_manager->foreground = GetForegroundWindow();
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
    return manager->create_hook != nullptr && manager->destroy_hook != nullptr && manager->foreground_hook != nullptr ? REACH_OK : REACH_ERROR;
}

static reach_result reach_window_manager_stop(reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    if (manager == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    if (manager->create_hook != nullptr) {
        UnhookWinEvent(manager->create_hook);
        manager->create_hook = nullptr;
    }
    if (manager->destroy_hook != nullptr) {
        UnhookWinEvent(manager->destroy_hook);
        manager->destroy_hook = nullptr;
    }
    if (manager->foreground_hook != nullptr) {
        UnhookWinEvent(manager->foreground_hook);
        manager->foreground_hook = nullptr;
    }
    if (g_window_manager == manager) {
        g_window_manager = nullptr;
    }
    return REACH_OK;
}

static BOOL CALLBACK reach_window_manager_enum_proc(HWND hwnd, LPARAM param)
{
    reach_window_manager *manager = reinterpret_cast<reach_window_manager *>(param);
    if (manager == nullptr || hwnd == nullptr || !IsWindowVisible(hwnd) || !IsZoomed(hwnd)) {
        return TRUE;
    }
    if (GetWindow(hwnd, GW_OWNER) != nullptr) {
        return TRUE;
    }

    LONG_PTR ex_style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((ex_style & WS_EX_TOOLWINDOW) != 0) {
        return TRUE;
    }

    RECT rect = {};
    if (!GetWindowRect(hwnd, &rect) || rect.right <= rect.left || rect.bottom <= rect.top) {
        return TRUE;
    }

    manager->maximized_windows.push_back(hwnd);
    return TRUE;
}

static int32_t reach_window_manager_is_reach_window(HWND hwnd)
{
    wchar_t class_name[64] = {};
    GetClassNameW(hwnd, class_name, 64);
    return lstrcmpiW(class_name, L"ReachPlatformWindow") == 0 || lstrcmpiW(class_name, L"ReachInputMessageWindow") == 0;
}

static int32_t reach_window_manager_contains(const std::vector<HWND> &windows, HWND hwnd)
{
    for (HWND existing : windows) {
        if (existing == hwnd) {
            return 1;
        }
    }
    return 0;
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

static int32_t reach_window_manager_is_app_window(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd) || reach_window_manager_is_reach_window(hwnd)) {
        return 0;
    }
    int32_t managed_minimized = g_window_manager != nullptr && reach_window_manager_contains(g_window_manager->managed_minimized, hwnd);
    if (!IsWindowVisible(hwnd) && !IsIconic(hwnd) && !managed_minimized) {
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

static BOOL CALLBACK reach_window_manager_enum_windows_proc(HWND hwnd, LPARAM param)
{
    reach_window_manager *manager = reinterpret_cast<reach_window_manager *>(param);
    if (manager == nullptr || !reach_window_manager_is_app_window(hwnd)) {
        return TRUE;
    }

    reach_window_snapshot snapshot = {};
    snapshot.id = reinterpret_cast<uintptr_t>(hwnd);
    snapshot.visible = IsWindowVisible(hwnd) ? 1 : 0;
    snapshot.maximized = IsZoomed(hwnd) ? 1 : 0;
    snapshot.minimized = IsIconic(hwnd) || reach_window_manager_contains(manager->managed_minimized, hwnd) ? 1 : 0;
    RECT rect = {};
    if (GetWindowRect(hwnd, &rect)) {
        snapshot.bounds = { rect.left, rect.top, rect.right, rect.bottom };
    }
    wchar_t title[260] = {};
    GetWindowTextW(hwnd, title, 260);
    (void)reach_copy_utf16(snapshot.title, 260, reinterpret_cast<const uint16_t *>(title));
    if (!reach_window_manager_query_process_path(hwnd, snapshot.path, 260)) {
        return TRUE;
    }

    if (snapshot.minimized) {
        ShowWindow(hwnd, SW_HIDE);
        snapshot.visible = 0;
        if (!reach_window_manager_contains(manager->managed_minimized, hwnd)) {
            manager->managed_minimized.push_back(hwnd);
        }
    }

    manager->windows.push_back(snapshot);
    return TRUE;
}

static void reach_window_manager_refresh_maximized(reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    if (manager == nullptr) {
        return;
    }

    manager->maximized_windows.clear();
    EnumWindows(reach_window_manager_enum_proc, reinterpret_cast<LPARAM>(manager));
}

static void reach_window_manager_refresh_windows(reach_window_manager *manager)
{
    if (manager == nullptr) {
        return;
    }
    manager->windows.clear();
    for (size_t index = 0; index < manager->managed_minimized.size();) {
        if (!IsWindow(manager->managed_minimized[index])) {
            manager->managed_minimized.erase(manager->managed_minimized.begin() + index);
        } else {
            ++index;
        }
    }
    EnumWindows(reach_window_manager_enum_windows_proc, reinterpret_cast<LPARAM>(manager));
    for (HWND hwnd : manager->managed_minimized) {
        if (!IsWindow(hwnd)) {
            continue;
        }
        int32_t already_present = 0;
        for (const reach_window_snapshot &snapshot : manager->windows) {
            if (snapshot.id == reinterpret_cast<uintptr_t>(hwnd)) {
                already_present = 1;
                break;
            }
        }
        if (!already_present) {
            reach_window_manager_enum_windows_proc(hwnd, reinterpret_cast<LPARAM>(manager));
        }
    }
}

static reach_result reach_window_manager_refresh(reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    if (manager == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    manager->foreground = GetForegroundWindow();
    reach_window_manager_refresh_maximized(manager);
    reach_window_manager_refresh_windows(manager);
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
    return manager->foreground != nullptr && IsZoomed(manager->foreground);
}

static int32_t reach_window_manager_any_window_is_maximized(const reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    return manager != nullptr && !manager->maximized_windows.empty();
}

static size_t reach_window_manager_maximized_window_count(const reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    return manager == nullptr ? 0 : manager->maximized_windows.size();
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
    ShowWindow(hwnd, SW_RESTORE);
    for (size_t index = 0; index < manager->managed_minimized.size();) {
        if (manager->managed_minimized[index] == hwnd || !IsWindow(manager->managed_minimized[index])) {
            manager->managed_minimized.erase(manager->managed_minimized.begin() + index);
        } else {
            ++index;
        }
    }
    SetForegroundWindow(hwnd);
    manager->foreground = hwnd;
    return REACH_OK;
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
    reach_window_manager_refresh_maximized(manager);
    reach_window_manager_refresh_windows(manager);
    out_port->manager = manager;
    out_port->ops.start = reach_window_manager_start;
    out_port->ops.stop = reach_window_manager_stop;
    out_port->ops.refresh = reach_window_manager_refresh;
    out_port->ops.snap = reach_window_manager_snap;
    out_port->ops.foreground = reach_window_manager_foreground;
    out_port->ops.foreground_is_maximized = reach_window_manager_foreground_is_maximized;
    out_port->ops.any_window_is_maximized = reach_window_manager_any_window_is_maximized;
    out_port->ops.maximized_window_count = reach_window_manager_maximized_window_count;
    out_port->ops.window_count = reach_window_manager_window_count;
    out_port->ops.window_at = reach_window_manager_window_at;
    out_port->ops.activate = reach_window_manager_activate;
    out_port->ops.close = reach_window_manager_close;
    out_port->ops.destroy = reach_window_manager_destroy;
    return REACH_OK;
}
