#include "windows_adapters_internal.h"

#include <windows.h>

#include <new>

#define REACH_SCREEN_HOTSPOT_MAX_PENDING_EVENTS 8

struct reach_screen_hotspot
{
    HWND hwnd;
    reach_screen_hotspot_callback callback;
    void *callback_user;
    int visible;
    int bounds_valid;
    int tracking_mouse_leave;
    reach_rect_f32 bounds;
    reach_screen_hotspot_event pending_events[REACH_SCREEN_HOTSPOT_MAX_PENDING_EVENTS];
    size_t pending_event_count;
};

static const wchar_t *reach_screen_hotspot_class_name()
{
    return L"ReachScreenHotspotWindow";
}

static void reach_screen_hotspot_queue_event(reach_screen_hotspot *hotspot,
                                               reach_screen_hotspot_event event)
{
    if (hotspot == nullptr)
    {
        return;
    }
    if (hotspot->pending_event_count > 0 &&
        hotspot->pending_events[hotspot->pending_event_count - 1] == event)
    {
        return;
    }
    if (hotspot->pending_event_count < REACH_SCREEN_HOTSPOT_MAX_PENDING_EVENTS)
    {
        hotspot->pending_events[hotspot->pending_event_count++] = event;
    }
    else
    {
        hotspot->pending_events[REACH_SCREEN_HOTSPOT_MAX_PENDING_EVENTS - 1] = event;
    }
}

static LRESULT CALLBACK reach_screen_hotspot_proc(HWND hwnd, UINT message, WPARAM wparam,
                                                    LPARAM lparam)
{
    reach_screen_hotspot *hotspot =
        reinterpret_cast<reach_screen_hotspot *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_NCCREATE:
    {
        CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return TRUE;
    }
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_NCHITTEST:
        return HTCLIENT;
    case WM_MOUSEMOVE:
        if (hotspot != nullptr && !hotspot->tracking_mouse_leave)
        {
            TRACKMOUSEEVENT track = {};
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = hwnd;
            hotspot->tracking_mouse_leave = TrackMouseEvent(&track) ? 1 : 0;
            reach_screen_hotspot_queue_event(hotspot, REACH_SCREEN_HOTSPOT_ENTER);
        }
        return 0;
    case WM_MOUSELEAVE:
        if (hotspot != nullptr)
        {
            hotspot->tracking_mouse_leave = 0;
        }
        reach_screen_hotspot_queue_event(hotspot, REACH_SCREEN_HOTSPOT_LEAVE);
        return 0;
    case WM_SETCURSOR:
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        return TRUE;
    case WM_ERASEBKGND:
        return 1;
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

static reach_result reach_screen_hotspot_register_class()
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = reach_screen_hotspot_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = reach_screen_hotspot_class_name();

    ATOM atom = RegisterClassExW(&wc);
    if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return REACH_ERROR;
    }

    return REACH_OK;
}

static reach_result reach_screen_hotspot_set_bounds(reach_screen_hotspot *hotspot,
                                                      reach_rect_f32 bounds)
{
    if (hotspot == nullptr || hotspot->hwnd == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    int width = (int)bounds.width;
    int height = (int)bounds.height;
    if (width < 1)
    {
        width = 1;
    }
    if (height < 1)
    {
        height = 1;
    }

    BOOL ok = SetWindowPos(hotspot->hwnd, nullptr, (int)bounds.x, (int)bounds.y, width, height,
                           SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);

    if (!ok)
    {
        return REACH_ERROR;
    }

    hotspot->bounds = bounds;
    hotspot->bounds_valid = 1;
    return REACH_OK;
}

static reach_result reach_screen_hotspot_show(reach_screen_hotspot *hotspot)
{
    if (hotspot == nullptr || hotspot->hwnd == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    ShowWindow(hotspot->hwnd, SW_SHOWNOACTIVATE);
    hotspot->visible = 1;
    return REACH_OK;
}

static reach_result reach_screen_hotspot_hide(reach_screen_hotspot *hotspot)
{
    if (hotspot == nullptr || hotspot->hwnd == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    ShowWindow(hotspot->hwnd, SW_HIDE);
    hotspot->visible = 0;
    hotspot->tracking_mouse_leave = 0;
    return REACH_OK;
}

static reach_result reach_screen_hotspot_place_behind(reach_screen_hotspot *hotspot,
                                                        reach_window_id window)
{
    if (hotspot == nullptr || hotspot->hwnd == nullptr || window == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    HWND target = reinterpret_cast<HWND>(window);
    BOOL ok = SetWindowPos(hotspot->hwnd, target, 0, 0, 0, 0,
                           SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    return ok ? REACH_OK : REACH_ERROR;
}

static reach_result reach_screen_hotspot_set_callback(reach_screen_hotspot *hotspot,
                                                        reach_screen_hotspot_callback callback,
                                                        void *user)
{
    if (hotspot == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    hotspot->callback = callback;
    hotspot->callback_user = user;
    return REACH_OK;
}

static int32_t reach_screen_hotspot_has_pending_events(const reach_screen_hotspot *hotspot)
{
    return hotspot != nullptr && hotspot->pending_event_count > 0;
}

static reach_result reach_screen_hotspot_dispatch_events(reach_screen_hotspot *hotspot)
{
    if (hotspot == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (hotspot->callback == nullptr || hotspot->pending_event_count == 0)
    {
        return REACH_OK;
    }

    reach_screen_hotspot_event events[REACH_SCREEN_HOTSPOT_MAX_PENDING_EVENTS] = {};
    size_t event_count = hotspot->pending_event_count;
    for (size_t index = 0; index < event_count; ++index)
    {
        events[index] = hotspot->pending_events[index];
    }
    hotspot->pending_event_count = 0;
    for (size_t index = 0; index < event_count; ++index)
    {
        hotspot->callback(hotspot->callback_user, events[index]);
    }
    return REACH_OK;
}

static void reach_screen_hotspot_destroy(reach_screen_hotspot *hotspot)
{
    if (hotspot == nullptr)
    {
        return;
    }

    if (hotspot->hwnd != nullptr)
    {
        DestroyWindow(hotspot->hwnd);
        hotspot->hwnd = nullptr;
    }

    delete hotspot;
}

reach_result reach_windows_create_screen_hotspot(reach_screen_hotspot_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};

    reach_result result = reach_screen_hotspot_register_class();
    if (result != REACH_OK)
    {
        return result;
    }

    reach_screen_hotspot *hotspot = new (std::nothrow) reach_screen_hotspot();
    if (hotspot == nullptr)
    {
        return REACH_ERROR;
    }

    hotspot->hwnd =
        CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED,
                        reach_screen_hotspot_class_name(), L"ReachScreenHotspot", WS_POPUP, 0, 0,
                        1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), hotspot);

    if (hotspot->hwnd == nullptr)
    {
        delete hotspot;
        return REACH_ERROR;
    }

    SetLayeredWindowAttributes(hotspot->hwnd, 0, 1, LWA_ALPHA);

    out_port->hotspot = hotspot;
    out_port->ops.set_bounds = reach_screen_hotspot_set_bounds;
    out_port->ops.show = reach_screen_hotspot_show;
    out_port->ops.hide = reach_screen_hotspot_hide;
    out_port->ops.place_behind = reach_screen_hotspot_place_behind;
    out_port->ops.set_callback = reach_screen_hotspot_set_callback;
    out_port->ops.has_pending_events = reach_screen_hotspot_has_pending_events;
    out_port->ops.dispatch_events = reach_screen_hotspot_dispatch_events;
    out_port->ops.destroy = reach_screen_hotspot_destroy;
    return REACH_OK;
}
