#include "windows_adapters_internal.h"

#include <windows.h>

#include <new>

struct reach_dock_reveal_edge
{
    HWND hwnd;
    reach_dock_reveal_edge_callback callback;
    void *callback_user;
    int visible;
    int bounds_valid;
    int tracking_mouse_leave;
    reach_rect_f32 bounds;
};

static const wchar_t *reach_dock_reveal_edge_class_name()
{
    return L"ReachDockRevealEdgeWindow";
}

static void reach_dock_reveal_edge_notify(reach_dock_reveal_edge *edge)
{
    if (edge != nullptr && edge->callback != nullptr)
    {
        edge->callback(edge->callback_user);
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
            reach_dock_reveal_edge_notify(edge);
        }
        return 0;
    case WM_MOUSELEAVE:
        if (edge != nullptr)
        {
            edge->tracking_mouse_leave = 0;
        }
        reach_dock_reveal_edge_notify(edge);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        reach_dock_reveal_edge_notify(edge);
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

    BOOL ok = SetWindowPos(edge->hwnd, HWND_TOPMOST, (int)bounds.x, (int)bounds.y, width, height,
                           SWP_NOACTIVATE | SWP_NOOWNERZORDER);

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
    SetWindowPos(edge->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
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

    /*
        Alpha 1 keeps the edge effectively invisible while preserving normal
        hit testing. Do not use WS_EX_TRANSPARENT; this window must receive
        mouse input.
    */
    SetLayeredWindowAttributes(edge->hwnd, 0, 1, LWA_ALPHA);

    out_port->edge = edge;
    out_port->ops.set_bounds = reach_dock_reveal_edge_set_bounds;
    out_port->ops.show = reach_dock_reveal_edge_show;
    out_port->ops.hide = reach_dock_reveal_edge_hide;
    out_port->ops.set_callback = reach_dock_reveal_edge_set_callback;
    out_port->ops.destroy = reach_dock_reveal_edge_destroy;
    return REACH_OK;
}
