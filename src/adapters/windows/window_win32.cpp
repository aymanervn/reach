#include "windows_adapters_internal.h"

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#include <new>
#include <wchar.h>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#define IDI_ICON1 101
#define REACH_PLATFORM_WINDOW_MAX_PENDING_EVENTS 128

#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

struct reach_platform_window {
    HWND hwnd;
    reach_surface_role role;
    reach_platform_window_event_callback callback;
    void *callback_user;
    reach_ui_event pending_events[REACH_PLATFORM_WINDOW_MAX_PENDING_EVENTS];
    size_t pending_event_count;
    int width;
    int height;
    float corner_radius;
    int tracking_mouse_leave;
    int pointer_move_enabled;
};

static int32_t reach_platform_window_queue_event(reach_platform_window *window, const reach_ui_event *event);

static const wchar_t *reach_window_class_name()
{
    return L"ReachPlatformWindow";
}

static HFONT reach_create_windows_menu_font()
{
    NONCLIENTMETRICSW metrics = {};
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0)) {
        if (metrics.lfMenuFont.lfHeight < 0) {
            metrics.lfMenuFont.lfHeight += 2;
        } else if (metrics.lfMenuFont.lfHeight > 2) {
            metrics.lfMenuFont.lfHeight -= 2;
        }
        return CreateFontIndirectW(&metrics.lfMenuFont);
    }
    return static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}

static void reach_delete_menu_font(HFONT font)
{
    if (font != nullptr && font != GetStockObject(DEFAULT_GUI_FONT)) {
        DeleteObject(font);
    }
}

static LRESULT CALLBACK reach_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    reach_platform_window *window = reinterpret_cast<reach_platform_window *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_SETCURSOR:
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        return TRUE;
    case REACH_WM_WALLPAPER_CHANGED:
        if (window != nullptr) {
            reach_ui_event event = {};
            event.type = REACH_UI_EVENT_WALLPAPER_CHANGED;
            reach_platform_window_queue_event(window, &event);
        }
        return 0;
    case REACH_WM_CONFIG_CHANGED:
        if (window != nullptr) {
            reach_ui_event event = {};
            event.type = REACH_UI_EVENT_CONFIG_CHANGED;
            reach_platform_window_queue_event(window, &event);
        }
        return 0;
    case REACH_WM_LAUNCHER_SEARCH_READY:
        if (window != nullptr) {
            reach_ui_event event = {};
            event.type = REACH_UI_EVENT_LAUNCHER_SEARCH_READY;
            reach_platform_window_queue_event(window, &event);
        }
        return 0;
    case WM_DISPLAYCHANGE:
        if (window != nullptr) {
            reach_ui_event event = {};
            event.type = REACH_UI_EVENT_DISPLAY_CHANGED;
            reach_platform_window_queue_event(window, &event);
        }
        return 0;
    case WM_KEYDOWN:
        if (window != nullptr) {
            reach_ui_event event = {};
            event.modifiers =
                (GetKeyState(VK_CONTROL) & 0x8000) != 0
                    ? REACH_UI_EVENT_MODIFIER_CTRL
                    : 0;
            if (wparam == VK_ESCAPE) {
                event.type = REACH_UI_EVENT_ESCAPE;
            } else if (wparam == VK_BACK) {
                event.type = REACH_UI_EVENT_BACKSPACE;
            } else if (wparam == VK_DELETE) {
                event.type = REACH_UI_EVENT_DELETE;
            } else if (wparam == VK_RETURN) {
                event.type = REACH_UI_EVENT_ENTER;
            } else if (wparam == VK_UP) {
                event.type = REACH_UI_EVENT_ARROW_UP;
            } else if (wparam == VK_DOWN) {
                event.type = REACH_UI_EVENT_ARROW_DOWN;
            } else if (wparam == VK_LEFT) {
                event.type = REACH_UI_EVENT_ARROW_LEFT;
            } else if (wparam == VK_RIGHT) {
                event.type = REACH_UI_EVENT_ARROW_RIGHT;
            } else if (wparam == VK_HOME) {
                event.type = REACH_UI_EVENT_HOME;
            } else if (wparam == VK_END) {
                event.type = REACH_UI_EVENT_END;
            }
            if (event.type != REACH_UI_EVENT_NONE) {
                reach_platform_window_queue_event(window, &event);
                return 0;
            }
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    case WM_CHAR:
        if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            return 0;
        }
        if (window != nullptr && wparam >= 0x20) {
            reach_ui_event event = {};
            event.type = REACH_UI_EVENT_TEXT;
            event.text[0] = static_cast<uint16_t>(wparam);
            event.text[1] = 0;
            reach_platform_window_queue_event(window, &event);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
        if (window != nullptr) {
            SetCapture(hwnd);
            POINT point = {};
            point.x = GET_X_LPARAM(lparam);
            point.y = GET_Y_LPARAM(lparam);
            ClientToScreen(hwnd, &point);
            reach_ui_event event = {};
            event.type = REACH_UI_EVENT_POINTER_DOWN;
            event.x = point.x;
            event.y = point.y;
            reach_platform_window_queue_event(window, &event);
        }
        return 0;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        if (window != nullptr) {
            if (GetCapture() == hwnd) {
                ReleaseCapture();
            }
            POINT point = {};
            point.x = GET_X_LPARAM(lparam);
            point.y = GET_Y_LPARAM(lparam);
            ClientToScreen(hwnd, &point);
            reach_ui_event event = {};
            event.type = message == WM_RBUTTONUP
                ? REACH_UI_EVENT_POINTER_CONTEXT
                : (message == WM_MBUTTONUP ? REACH_UI_EVENT_POINTER_MIDDLE : REACH_UI_EVENT_POINTER_UP);
            event.x = point.x;
            event.y = point.y;
            reach_platform_window_queue_event(window, &event);
        }
        return 0;
        case WM_MOUSEMOVE:
            if (window != nullptr &&
                window->pointer_move_enabled) {
                if (!window->tracking_mouse_leave) {
                    TRACKMOUSEEVENT track = {};
                    track.cbSize = sizeof(track);
                    track.dwFlags = TME_LEAVE;
                    track.hwndTrack = hwnd;
                    window->tracking_mouse_leave = TrackMouseEvent(&track) ? 1 : 0;
                }
                POINT point = {};
                point.x = GET_X_LPARAM(lparam);
                point.y = GET_Y_LPARAM(lparam);
                ClientToScreen(hwnd, &point);
                reach_ui_event event = {};
                event.type = REACH_UI_EVENT_POINTER_MOVE;
                event.x = point.x;
                event.y = point.y;
                reach_platform_window_queue_event(window, &event);
            }
            return 0;
    case WM_MOUSELEAVE:
        if (window != nullptr) {
            window->tracking_mouse_leave = 0;
            reach_ui_event event = {};
            event.type = REACH_UI_EVENT_POINTER_LEAVE;
            reach_platform_window_queue_event(window, &event);
        }
        return 0;
    case WM_SIZE:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT *measure = reinterpret_cast<MEASUREITEMSTRUCT *>(lparam);
        if (measure != nullptr && measure->CtlType == ODT_MENU) {
            const wchar_t *text = reinterpret_cast<const wchar_t *>(measure->itemData);
            SIZE size = {};
            HDC dc = GetDC(hwnd);
            if (dc != nullptr && text != nullptr) {
                HFONT font = reach_create_windows_menu_font();
                HGDIOBJ old_font = SelectObject(dc, font);
                GetTextExtentPoint32W(dc, text, (int)wcslen(text), &size);
                SelectObject(dc, old_font);
                reach_delete_menu_font(font);
                ReleaseDC(hwnd, dc);
            }
            measure->itemWidth = (UINT)(size.cx + 28);
            measure->itemHeight = 30;
            return TRUE;
        }
        break;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *draw = reinterpret_cast<DRAWITEMSTRUCT *>(lparam);
        if (draw != nullptr && draw->CtlType == ODT_MENU) {
            const wchar_t *text = reinterpret_cast<const wchar_t *>(draw->itemData);
            COLORREF background = (draw->itemState & ODS_SELECTED) ? RGB(48, 45, 42) : RGB(32, 30, 28);
            HBRUSH brush = CreateSolidBrush(background);
            if (brush != nullptr) {
                FillRect(draw->hDC, &draw->rcItem, brush);
                DeleteObject(brush);
            }
            SetBkMode(draw->hDC, TRANSPARENT);
            SetTextColor(draw->hDC, RGB(218, 216, 212));
            HFONT font = reach_create_windows_menu_font();
            HGDIOBJ old_font = SelectObject(draw->hDC, font);
            RECT text_rect = draw->rcItem;
            text_rect.left += 13;
            text_rect.right -= 13;
            DrawTextW(draw->hDC, text != nullptr ? text : L"", -1, &text_rect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
            SelectObject(draw->hDC, old_font);
            reach_delete_menu_font(font);
            return TRUE;
        }
        break;
    }
    case WM_ERASEBKGND:
        return 1;
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static reach_result reach_register_platform_class()
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = reach_window_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_ICON1));
    wc.hIconSm = wc.hIcon;
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
    if (role == REACH_SURFACE_DOCK ||
        role == REACH_SURFACE_LAUNCHER ||
        role == REACH_SURFACE_TRAY_MENU ||
        role == REACH_SURFACE_SWITCHER ||
        role == REACH_SURFACE_CONTEXT_MENU ||
        role == REACH_SURFACE_QUICK_SETTINGS) {
        style |= WS_EX_NOREDIRECTIONBITMAP;
    } else {
        style |= WS_EX_LAYERED;
    }
    if (role == REACH_SURFACE_DOCK ||
        role == REACH_SURFACE_LAUNCHER ||
        role == REACH_SURFACE_TRAY_MENU ||
        role == REACH_SURFACE_SWITCHER ||
        role == REACH_SURFACE_CONTEXT_MENU ||
        role == REACH_SURFACE_QUICK_SETTINGS) {
        style |= WS_EX_TOPMOST;
    }
    if (role == REACH_SURFACE_DOCK ||
        role == REACH_SURFACE_TRAY_MENU ||
        role == REACH_SURFACE_SWITCHER ||
        role == REACH_SURFACE_CONTEXT_MENU ||
        role == REACH_SURFACE_QUICK_SETTINGS) {
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
        window->role == REACH_SURFACE_TRAY_MENU ||
        window->role == REACH_SURFACE_SWITCHER ||
        window->role == REACH_SURFACE_CONTEXT_MENU ||
        window->role == REACH_SURFACE_QUICK_SETTINGS
        ? SW_SHOWNOACTIVATE
        : SW_SHOW;
    ShowWindow(window->hwnd, show_command);
    if (window->role != REACH_SURFACE_DOCK &&
        window->role != REACH_SURFACE_TRAY_MENU &&
        window->role != REACH_SURFACE_SWITCHER &&
        window->role != REACH_SURFACE_CONTEXT_MENU &&
        window->role != REACH_SURFACE_QUICK_SETTINGS) {
        SetForegroundWindow(window->hwnd);
        SetActiveWindow(window->hwnd);
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
    window->width = width;
    window->height = height;
    return ok ? REACH_OK : REACH_ERROR;
}

static reach_result reach_platform_window_apply_rounded_corners(reach_platform_window *window, float radius)
{
    if (window == nullptr || window->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    window->corner_radius = radius;

    /*
        Composition-backed surfaces draw their own rounded shape in Direct2D.
        Native corners or Win32 regions add a second clipping path and can make
        edges look different from the dock.
    */
    if (window->role == REACH_SURFACE_DOCK ||
        window->role == REACH_SURFACE_LAUNCHER ||
        window->role == REACH_SURFACE_TRAY_MENU ||
        window->role == REACH_SURFACE_SWITCHER ||
        window->role == REACH_SURFACE_CONTEXT_MENU ||
        window->role == REACH_SURFACE_QUICK_SETTINGS) {
        return REACH_OK;
    }

    int preference = DWMWCP_ROUND;
    (void)DwmSetWindowAttribute(window->hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));

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
    if (window->role == REACH_SURFACE_DOCK ||
        window->role == REACH_SURFACE_LAUNCHER ||
        window->role == REACH_SURFACE_TRAY_MENU ||
        window->role == REACH_SURFACE_SWITCHER ||
        window->role == REACH_SURFACE_CONTEXT_MENU ||
        window->role == REACH_SURFACE_QUICK_SETTINGS) {
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

static reach_result reach_platform_window_dispatch_events(reach_platform_window *window)
{
    if (window == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    if (window->callback == nullptr || window->pending_event_count == 0) {
        return REACH_OK;
    }

    reach_ui_event events[REACH_PLATFORM_WINDOW_MAX_PENDING_EVENTS] = {};
    size_t event_count = window->pending_event_count;
    for (size_t index = 0; index < event_count; ++index) {
        events[index] = window->pending_events[index];
    }
    window->pending_event_count = 0;

    for (size_t index = 0; index < event_count; ++index) {
        window->callback(window->callback_user, &events[index]);
    }

    return REACH_OK;
}

static reach_result reach_platform_window_set_pointer_move_enabled(
    reach_platform_window *window,
    int32_t enabled
)
{
    if (window == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    window->pointer_move_enabled = enabled ? 1 : 0;

    if (!window->pointer_move_enabled) {
        window->tracking_mouse_leave = 0;
    }

    return REACH_OK;
}

void *reach_windows_platform_window_native_handle(reach_platform_window *window)
{
    return window == nullptr ? nullptr : window->hwnd;
}

static int32_t reach_platform_window_queue_event(reach_platform_window *window, const reach_ui_event *event)
{
    if (window == nullptr || event == nullptr || event->type == REACH_UI_EVENT_NONE) {
        return 0;
    }

    if (window->pending_event_count < REACH_PLATFORM_WINDOW_MAX_PENDING_EVENTS) {
        window->pending_events[window->pending_event_count++] = *event;
    } else {
        window->pending_events[REACH_PLATFORM_WINDOW_MAX_PENDING_EVENTS - 1] = *event;
    }
    return 1;
}

static reach_result reach_platform_window_raise(reach_platform_window *window)
{
    if (window == nullptr || window->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    ShowWindow(window->hwnd, SW_SHOW);

    HWND foreground = GetForegroundWindow();
    DWORD foreground_thread = foreground != nullptr ? GetWindowThreadProcessId(foreground, nullptr) : 0;
    DWORD window_thread = GetWindowThreadProcessId(window->hwnd, nullptr);
    DWORD current_thread = GetCurrentThreadId();

    bool attached_foreground = false;
    bool attached_window = false;

    if (foreground_thread != 0 && foreground_thread != current_thread) {
        attached_foreground = AttachThreadInput(current_thread, foreground_thread, TRUE) != FALSE;
    }
    if (window_thread != 0 && window_thread != current_thread && window_thread != foreground_thread) {
        attached_window = AttachThreadInput(current_thread, window_thread, TRUE) != FALSE;
    }

    SetWindowPos(window->hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    BringWindowToTop(window->hwnd);
    SetForegroundWindow(window->hwnd);
    SetActiveWindow(window->hwnd);
    SetFocus(window->hwnd);

    if (attached_window) {
        AttachThreadInput(current_thread, window_thread, FALSE);
    }
    if (attached_foreground) {
        AttachThreadInput(current_thread, foreground_thread, FALSE);
    }

    return REACH_OK;
}

static reach_result reach_platform_window_post_event(reach_platform_window *window, reach_ui_event_type type)
{
    if (window == nullptr || window->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    UINT message = 0;
    switch (type) {
    case REACH_UI_EVENT_LAUNCHER_SEARCH_READY:
        message = REACH_WM_LAUNCHER_SEARCH_READY;
        break;
    case REACH_UI_EVENT_CONFIG_CHANGED:
        message = REACH_WM_CONFIG_CHANGED;
        break;
    case REACH_UI_EVENT_WALLPAPER_CHANGED:
        message = REACH_WM_WALLPAPER_CHANGED;
        break;
    default:
        return REACH_INVALID_ARGUMENT;
    }

    return PostMessageW(window->hwnd, message, 0, 0) ? REACH_OK : REACH_ERROR;
}

static void reach_platform_window_destroy(reach_platform_window *window)
{
    if (window == nullptr) {
        return;
    }

    if (window->hwnd != nullptr) {
        DestroyWindow(window->hwnd);
        window->hwnd = nullptr;
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
    window->pointer_move_enabled = 1;
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
    out_port->ops.dispatch_events = reach_platform_window_dispatch_events;
    out_port->ops.set_pointer_move_enabled = reach_platform_window_set_pointer_move_enabled;
    out_port->ops.raise = reach_platform_window_raise;
    out_port->ops.post_event = reach_platform_window_post_event;
    out_port->ops.destroy = reach_platform_window_destroy;
    return REACH_OK;
}
