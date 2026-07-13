#include "windows_adapters_internal.h"

#include <windows.h>

#include <new>

#define REACH_DOCK_REVEAL_EDGE_MAX_PENDING_EVENTS 8

struct reach_dock_reveal_edge
{
    HWND hwnd;
    reach_dock_reveal_edge_callback callback;
    void *callback_user;
    int visible;
    int bounds_valid;
    int tracking_mouse_leave;
    reach_rect_f32 bounds;
    reach_dock_reveal_edge_event pending_events[REACH_DOCK_REVEAL_EDGE_MAX_PENDING_EVENTS];
    size_t pending_event_count;
};

static const wchar_t *reach_dock_reveal_edge_class_name()
{
    return L"ReachDockRevealEdgeWindow";
}

static void reach_dock_reveal_edge_queue_event(reach_dock_reveal_edge *edge,
                                               reach_dock_reveal_edge_event event)
{
    if (edge == nullptr)
    {
        return;
    }
    if (edge->pending_event_count > 0 &&
        edge->pending_events[edge->pending_event_count - 1] == event)
    {
        return;
    }
    if (edge->pending_event_count < REACH_DOCK_REVEAL_EDGE_MAX_PENDING_EVENTS)
    {
        edge->pending_events[edge->pending_event_count++] = event;
    }
    else
    {
        edge->pending_events[REACH_DOCK_REVEAL_EDGE_MAX_PENDING_EVENTS - 1] = event;
    }
}

static LRESULT CALLBACK reach_dock_reveal_edge_proc(HWND hwnd, UINT message, WPARAM wparam,
                                                    LPARAM lparam)
{
    reach_dock_reveal_edge *edge =
        reinterpret_cast<reach_dock_reveal_edge *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

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
        if (edge != nullptr && !edge->tracking_mouse_leave)
        {
            TRACKMOUSEEVENT track = {};
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = hwnd;
            edge->tracking_mouse_leave = TrackMouseEvent(&track) ? 1 : 0;
            reach_dock_reveal_edge_queue_event(edge, REACH_DOCK_REVEAL_EDGE_ENTER);
        }
        return 0;
    case WM_MOUSELEAVE:
        if (edge != nullptr)
        {
            edge->tracking_mouse_leave = 0;
        }
        reach_dock_reveal_edge_queue_event(edge, REACH_DOCK_REVEAL_EDGE_LEAVE);
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

static reach_result reach_dock_reveal_edge_register_class()
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = reach_dock_reveal_edge_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = reach_dock_reveal_edge_class_name();

    ATOM atom = RegisterClassExW(&wc);
    if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return REACH_ERROR;
    }

    return REACH_OK;
}

static reach_result reach_dock_reveal_edge_set_bounds(reach_dock_reveal_edge *edge,
                                                      reach_rect_f32 bounds)
{
    if (edge == nullptr || edge->hwnd == nullptr)
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

    BOOL ok = SetWindowPos(edge->hwnd, nullptr, (int)bounds.x, (int)bounds.y, width, height,
                           SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);

    if (!ok)
    {
        return REACH_ERROR;
    }

    edge->bounds = bounds;
    edge->bounds_valid = 1;
    return REACH_OK;
}

static reach_result reach_dock_reveal_edge_show(reach_dock_reveal_edge *edge)
{
    if (edge == nullptr || edge->hwnd == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    ShowWindow(edge->hwnd, SW_SHOWNOACTIVATE);
    edge->visible = 1;
    return REACH_OK;
}

static reach_result reach_dock_reveal_edge_hide(reach_dock_reveal_edge *edge)
{
    if (edge == nullptr || edge->hwnd == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    ShowWindow(edge->hwnd, SW_HIDE);
    edge->visible = 0;
    edge->tracking_mouse_leave = 0;
    return REACH_OK;
}

static reach_result reach_dock_reveal_edge_place_behind(reach_dock_reveal_edge *edge,
                                                        reach_window_id window)
{
    if (edge == nullptr || edge->hwnd == nullptr || window == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    HWND target = reinterpret_cast<HWND>(window);
    BOOL ok = SetWindowPos(edge->hwnd, target, 0, 0, 0, 0,
                           SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    return ok ? REACH_OK : REACH_ERROR;
}

static reach_result reach_dock_reveal_edge_set_callback(reach_dock_reveal_edge *edge,
                                                        reach_dock_reveal_edge_callback callback,
                                                        void *user)
{
    if (edge == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    edge->callback = callback;
    edge->callback_user = user;
    return REACH_OK;
}

static int32_t reach_dock_reveal_edge_has_pending_events(const reach_dock_reveal_edge *edge)
{
    return edge != nullptr && edge->pending_event_count > 0;
}

static reach_result reach_dock_reveal_edge_dispatch_events(reach_dock_reveal_edge *edge)
{
    if (edge == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (edge->callback == nullptr || edge->pending_event_count == 0)
    {
        return REACH_OK;
    }

    reach_dock_reveal_edge_event events[REACH_DOCK_REVEAL_EDGE_MAX_PENDING_EVENTS] = {};
    size_t event_count = edge->pending_event_count;
    for (size_t index = 0; index < event_count; ++index)
    {
        events[index] = edge->pending_events[index];
    }
    edge->pending_event_count = 0;
    for (size_t index = 0; index < event_count; ++index)
    {
        edge->callback(edge->callback_user, events[index]);
    }
    return REACH_OK;
}

static void reach_dock_reveal_edge_destroy(reach_dock_reveal_edge *edge)
{
    if (edge == nullptr)
    {
        return;
    }

    if (edge->hwnd != nullptr)
    {
        DestroyWindow(edge->hwnd);
        edge->hwnd = nullptr;
    }

    delete edge;
}

reach_result reach_windows_create_dock_reveal_edge(reach_dock_reveal_edge_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};

    reach_result result = reach_dock_reveal_edge_register_class();
    if (result != REACH_OK)
    {
        return result;
    }

    reach_dock_reveal_edge *edge = new (std::nothrow) reach_dock_reveal_edge();
    if (edge == nullptr)
    {
        return REACH_ERROR;
    }

    edge->hwnd =
        CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED,
                        reach_dock_reveal_edge_class_name(), L"ReachDockRevealEdge", WS_POPUP, 0, 0,
                        1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), edge);

    if (edge->hwnd == nullptr)
    {
        delete edge;
        return REACH_ERROR;
    }

    SetLayeredWindowAttributes(edge->hwnd, 0, 1, LWA_ALPHA);

    out_port->edge = edge;
    out_port->ops.set_bounds = reach_dock_reveal_edge_set_bounds;
    out_port->ops.show = reach_dock_reveal_edge_show;
    out_port->ops.hide = reach_dock_reveal_edge_hide;
    out_port->ops.place_behind = reach_dock_reveal_edge_place_behind;
    out_port->ops.set_callback = reach_dock_reveal_edge_set_callback;
    out_port->ops.has_pending_events = reach_dock_reveal_edge_has_pending_events;
    out_port->ops.dispatch_events = reach_dock_reveal_edge_dispatch_events;
    out_port->ops.destroy = reach_dock_reveal_edge_destroy;
    return REACH_OK;
}
