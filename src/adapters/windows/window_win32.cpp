#include "reach/platform/windows_adapters.h"

#include <windows.h>

#include <new>

struct reach_platform_window {
    HWND hwnd;
    reach_surface_role role;
};

static const wchar_t *reach_window_class_name()
{
    return L"ReachPlatformWindow";
}

static LRESULT CALLBACK reach_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

static reach_result reach_register_platform_class()
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = reach_window_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = reach_window_class_name();

    ATOM atom = RegisterClassExW(&wc);
    if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return REACH_ERROR;
    }

    return REACH_OK;
}

static DWORD reach_window_ex_style(reach_surface_role role)
{
    DWORD style = WS_EX_TOOLWINDOW | WS_EX_LAYERED;
    if (role == REACH_SURFACE_DOCK || role == REACH_SURFACE_LAUNCHER) {
        style |= WS_EX_TOPMOST;
    }
    if (role == REACH_SURFACE_DOCK) {
        style |= WS_EX_NOACTIVATE;
    }
    return style;
}

static reach_result reach_platform_window_show(reach_platform_window *window)
{
    if (window == nullptr || window->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    ShowWindow(window->hwnd, SW_SHOWNOACTIVATE);
    return REACH_OK;
}

static reach_result reach_platform_window_hide(reach_platform_window *window)
{
    if (window == nullptr || window->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    ShowWindow(window->hwnd, SW_HIDE);
    return REACH_OK;
}

static reach_result reach_platform_window_set_bounds(reach_platform_window *window, reach_rect_f32 bounds)
{
    if (window == nullptr || window->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    BOOL ok = SetWindowPos(
        window->hwnd,
        HWND_TOPMOST,
        (int)bounds.x,
        (int)bounds.y,
        (int)bounds.width,
        (int)bounds.height,
        SWP_NOACTIVATE);
    return ok ? REACH_OK : REACH_ERROR;
}

static reach_result reach_platform_window_set_opacity(reach_platform_window *window, float opacity)
{
    if (window == nullptr || window->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    if (opacity < 0.0f) {
        opacity = 0.0f;
    }
    if (opacity > 1.0f) {
        opacity = 1.0f;
    }

    BYTE alpha = (BYTE)(opacity * 255.0f);
    return SetLayeredWindowAttributes(window->hwnd, 0, alpha, LWA_ALPHA) ? REACH_OK : REACH_ERROR;
}

static void *reach_platform_window_native_handle(reach_platform_window *window)
{
    return window == nullptr ? nullptr : window->hwnd;
}

static void reach_platform_window_destroy(reach_platform_window *window)
{
    if (window == nullptr) {
        return;
    }

    if (window->hwnd != nullptr) {
        DestroyWindow(window->hwnd);
    }
    delete window;
}

reach_result reach_windows_create_platform_window(reach_surface_role role, reach_platform_window_port *out_port)
{
    if (out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_result result = reach_register_platform_class();
    if (result != REACH_OK) {
        return result;
    }

    reach_platform_window *window = new (std::nothrow) reach_platform_window();
    if (window == nullptr) {
        return REACH_ERROR;
    }
    window->role = role;

    window->hwnd = CreateWindowExW(
        reach_window_ex_style(role),
        reach_window_class_name(),
        L"Reach",
        WS_POPUP,
        0,
        0,
        1,
        1,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    if (window->hwnd == nullptr) {
        delete window;
        return REACH_ERROR;
    }

    out_port->window = window;
    out_port->role = role;
    out_port->ops.show = reach_platform_window_show;
    out_port->ops.hide = reach_platform_window_hide;
    out_port->ops.set_bounds = reach_platform_window_set_bounds;
    out_port->ops.set_opacity = reach_platform_window_set_opacity;
    out_port->ops.native_handle = reach_platform_window_native_handle;
    out_port->ops.destroy = reach_platform_window_destroy;
    return REACH_OK;
}
