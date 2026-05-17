#include "reach/wm.h"

#include <windows.h>
#include <shobjidl.h>

#include <new>

struct reach_wm {
    HWINEVENTHOOK create_hook;
    HWINEVENTHOOK destroy_hook;
    HWINEVENTHOOK foreground_hook;
    HWND foreground;
};

static void CALLBACK reach_wm_event_proc(
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
    (void)window;
    (void)object_id;
    (void)child_id;
    (void)event_thread;
    (void)event_time;
}

static reach_result reach_work_area_for_window(HWND window, RECT *out_rect)
{
    if (out_rect == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        return REACH_ERROR;
    }

    *out_rect = info.rcWork;
    return REACH_OK;
}

static RECT reach_split_rect(RECT work, reach_split_mode mode)
{
    RECT result = work;
    const LONG width = work.right - work.left;
    const LONG height = work.bottom - work.top;

    switch (mode) {
    case REACH_SPLIT_LEFT:
        result.right = work.left + width / 2;
        break;
    case REACH_SPLIT_RIGHT:
        result.left = work.left + width / 2;
        break;
    case REACH_SPLIT_TOP:
        result.bottom = work.top + height / 2;
        break;
    case REACH_SPLIT_BOTTOM:
        result.top = work.top + height / 2;
        break;
    case REACH_SPLIT_FULL:
    default:
        break;
    }

    return result;
}

reach_result reach_wm_create(reach_wm **out_wm)
{
    if (out_wm == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_wm *wm = new (std::nothrow) reach_wm();
    if (wm == nullptr) {
        *out_wm = nullptr;
        return REACH_ERROR;
    }

    wm->foreground = GetForegroundWindow();
    *out_wm = wm;
    return REACH_OK;
}

void reach_wm_destroy(reach_wm *wm)
{
    if (wm != nullptr) {
        (void)reach_wm_uninstall_hooks(wm);
    }
    delete wm;
}

reach_result reach_wm_install_hooks(reach_wm *wm)
{
    if (wm == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    (void)reach_wm_uninstall_hooks(wm);

    wm->create_hook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_CREATE, nullptr, reach_wm_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    wm->destroy_hook = SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, nullptr, reach_wm_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    wm->foreground_hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, reach_wm_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    return (wm->create_hook != nullptr && wm->destroy_hook != nullptr && wm->foreground_hook != nullptr) ? REACH_OK : REACH_ERROR;
}

reach_result reach_wm_uninstall_hooks(reach_wm *wm)
{
    if (wm == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    if (wm->create_hook != nullptr) {
        UnhookWinEvent(wm->create_hook);
        wm->create_hook = nullptr;
    }
    if (wm->destroy_hook != nullptr) {
        UnhookWinEvent(wm->destroy_hook);
        wm->destroy_hook = nullptr;
    }
    if (wm->foreground_hook != nullptr) {
        UnhookWinEvent(wm->foreground_hook);
        wm->foreground_hook = nullptr;
    }

    return REACH_OK;
}

reach_result reach_wm_snap_window(reach_wm *wm, reach_window_id window, reach_split_mode mode)
{
    (void)wm;
    HWND hwnd = reinterpret_cast<HWND>(window);
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return REACH_INVALID_ARGUMENT;
    }

    RECT work = {};
    reach_result result = reach_work_area_for_window(hwnd, &work);
    if (result != REACH_OK) {
        return result;
    }

    RECT target = reach_split_rect(work, mode);
    if (IsZoomed(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }

    BOOL ok = SetWindowPos(hwnd, nullptr, target.left, target.top, target.right - target.left, target.bottom - target.top, SWP_NOZORDER | SWP_NOACTIVATE);
    return ok ? REACH_OK : REACH_ERROR;
}

reach_result reach_wm_update_z_order(reach_wm *wm)
{
    if (wm == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    wm->foreground = GetForegroundWindow();
    return REACH_OK;
}

reach_window_id reach_wm_foreground_window(const reach_wm *wm)
{
    HWND foreground = wm != nullptr ? wm->foreground : GetForegroundWindow();
    return reinterpret_cast<reach_window_id>(foreground);
}
