#include "reach/platform/windows_adapters.h"
#include "reach/platform/windows_messages.h"

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#include <new>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

struct reach_platform_window {
    HWND hwnd;
    reach_surface_role role;
    reach_platform_window_event_callback callback;
    void *callback_user;
    int width;
    int height;
    float corner_radius;
};

static const wchar_t *reach_window_class_name()
{
    return L"ReachPlatformWindow";
}

static LRESULT CALLBACK reach_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    reach_platform_window *window = reinterpret_cast<reach_platform_window *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case REACH_WM_WALLPAPER_CHANGED:
        if (window != nullptr && window->callback != nullptr) {
            reach_ui_event event = {};
            event.type = REACH_UI_EVENT_WALLPAPER_CHANGED;
            window->callback(window->callback_user, &event);
        }
        return 0;
    case WM_KEYDOWN:
        if (window != nullptr && window->callback != nullptr) {
            reach_ui_event event = {};
            if (wparam == VK_ESCAPE) {
                event.type = REACH_UI_EVENT_ESCAPE;
            } else if (wparam == VK_BACK) {
                event.type = REACH_UI_EVENT_BACKSPACE;
            }
            if (event.type != REACH_UI_EVENT_NONE) {
                window->callback(window->callback_user, &event);
                return 0;
            }
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    case WM_CHAR:
        if (window != nullptr && window->callback != nullptr && wparam >= 0x20) {
            reach_ui_event event = {};
            event.type = REACH_UI_EVENT_TEXT;
            event.text[0] = static_cast<uint16_t>(wparam);
            event.text[1] = 0;
            window->callback(window->callback_user, &event);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
        if (window != nullptr && window->callback != nullptr) {
            POINT point = {};
            point.x = GET_X_LPARAM(lparam);
            point.y = GET_Y_LPARAM(lparam);
            ClientToScreen(hwnd, &point);
            reach_ui_event event = {};
            event.type = message == WM_RBUTTONUP ? REACH_UI_EVENT_POINTER_CONTEXT : REACH_UI_EVENT_POINTER_UP;
            event.x = point.x;
            event.y = point.y;
            window->callback(window->callback_user, &event);
        }
        return 0;
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
    DWORD style = WS_EX_TOOLWINDOW;
    if (role == REACH_SURFACE_DOCK) {
        style |= WS_EX_NOREDIRECTIONBITMAP;
    } else {
        style |= WS_EX_LAYERED;
    }
    if (role == REACH_SURFACE_DOCK || role == REACH_SURFACE_LAUNCHER || role == REACH_SURFACE_TRAY_MENU) {
        style |= WS_EX_TOPMOST;
    }
    if (role == REACH_SURFACE_DOCK || role == REACH_SURFACE_TRAY_MENU) {
        style |= WS_EX_NOACTIVATE;
    }
    return style;
}

static reach_result reach_platform_window_show(reach_platform_window *window)
{
    if (window == nullptr || window->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    int show_command = window->role == REACH_SURFACE_DOCK ||
        window->role == REACH_SURFACE_TRAY_MENU
        ? SW_SHOWNOACTIVATE
        : SW_SHOW;
    ShowWindow(window->hwnd, show_command);
    if (window->role != REACH_SURFACE_DOCK &&
        window->role != REACH_SURFACE_TRAY_MENU) {
        SetForegroundWindow(window->hwnd);
        SetFocus(window->hwnd);
    }
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

    int width = (int)bounds.width;
    int height = (int)bounds.height;

    BOOL ok = SetWindowPos(
        window->hwnd,
        HWND_TOPMOST,
        (int)bounds.x,
        (int)bounds.y,
        width,
        height,
        SWP_NOACTIVATE);
    if (ok && window->role == REACH_SURFACE_TRAY_MENU && (window->width != width || window->height != height)) {
        int radius = window->corner_radius > 0.0f ? (int)(window->corner_radius * 2.0f) : (height > 0 ? (int)((float)height * 0.42f) : 24);
        if (radius < 18) {
            radius = 18;
        }
        HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, radius, radius);
        if (region != nullptr) {
            SetWindowRgn(window->hwnd, region, TRUE);
        }
        window->width = width;
        window->height = height;
    }
    return ok ? REACH_OK : REACH_ERROR;
}

static reach_result reach_platform_window_apply_rounded_corners(reach_platform_window *window, float radius)
{
    if (window == nullptr || window->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    window->corner_radius = radius;

    /*
        The dock uses DirectComposition + premultiplied alpha.
        Do not apply DWM/native rounded corners here, or it creates a second
        rounded shape around the D2D-rendered dock body.
    */
    if (window->role == REACH_SURFACE_DOCK) {
        return REACH_OK;
    }

    if (window->width <= 0 || window->height <= 0) {
        return REACH_OK;
    }

    int diameter = radius > 0.0f ? (int)(radius * 2.0f) : 18;
    if (diameter < 18) {
        diameter = 18;
    }

    HRGN region = CreateRoundRectRgn(
        0,
        0,
        window->width + 1,
        window->height + 1,
        diameter,
        diameter
    );

    if (region == nullptr) {
        return REACH_ERROR;
    }

    return SetWindowRgn(window->hwnd, region, TRUE)
        ? REACH_OK
        : REACH_ERROR;
}
static reach_result reach_platform_window_set_opacity(reach_platform_window *window, float opacity)
{
    if (window == nullptr || window->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }
    if (window->role == REACH_SURFACE_DOCK) {
        return REACH_OK;
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

static reach_result reach_platform_window_set_blur_enabled(reach_platform_window *window, int32_t enabled)
{
    if (window == nullptr || window->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    DWM_BLURBEHIND blur = {};
    blur.dwFlags = DWM_BB_ENABLE;
    blur.fEnable = enabled ? TRUE : FALSE;
    HRESULT hr = DwmEnableBlurBehindWindow(window->hwnd, &blur);
    return SUCCEEDED(hr) || hr == DWM_E_COMPOSITIONDISABLED ? REACH_OK : REACH_ERROR;
}

static reach_result reach_platform_window_set_event_callback(
    reach_platform_window *window,
    reach_platform_window_event_callback callback,
    void *user)
{
    if (window == nullptr || window->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    window->callback = callback;
    window->callback_user = user;
    return REACH_OK;
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
    SetWindowLongPtrW(window->hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));

    out_port->window = window;
    out_port->role = role;
    out_port->ops.show = reach_platform_window_show;
    out_port->ops.hide = reach_platform_window_hide;
    out_port->ops.set_bounds = reach_platform_window_set_bounds;
    out_port->ops.set_opacity = reach_platform_window_set_opacity;
    out_port->ops.set_blur_enabled = reach_platform_window_set_blur_enabled;
    out_port->ops.apply_rounded_corners = reach_platform_window_apply_rounded_corners;
    out_port->ops.set_event_callback = reach_platform_window_set_event_callback;
    out_port->ops.native_handle = reach_platform_window_native_handle;
    out_port->ops.destroy = reach_platform_window_destroy;
    return REACH_OK;
}
