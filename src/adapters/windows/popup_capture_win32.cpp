#include "windows_adapters_internal.h"

#include <windows.h>

#include <new>

struct reach_popup_capture_adapter {
    HHOOK mouse_hook;
    reach_popup_capture_mouse_down_callback mouse_callback;
    void *mouse_callback_userdata;
};

static reach_popup_capture_adapter *g_reach_popup_capture_instance;

static LRESULT CALLBACK reach_popup_capture_mouse_hook_proc(
    int code,
    WPARAM wparam,
    LPARAM lparam
)
{
    if (code >= 0 && g_reach_popup_capture_instance != nullptr) {
        if (wparam == WM_LBUTTONDOWN ||
            wparam == WM_RBUTTONDOWN ||
            wparam == WM_MBUTTONDOWN ||
            wparam == WM_XBUTTONDOWN) {
            MSLLHOOKSTRUCT *mouse = reinterpret_cast<MSLLHOOKSTRUCT *>(lparam);
            if (mouse != nullptr &&
                g_reach_popup_capture_instance->mouse_callback != nullptr) {
                g_reach_popup_capture_instance->mouse_callback(
                    g_reach_popup_capture_instance->mouse_callback_userdata,
                    mouse->pt.x,
                    mouse->pt.y);
            }
        }
    }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

static reach_result reach_popup_capture_begin_capture(
    void *userdata,
    reach_platform_window *surface
)
{
    (void)userdata;
    void *native_handle = reach_windows_platform_window_native_handle(surface);
    if (native_handle == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }
    HWND hwnd = static_cast<HWND>(native_handle);
    SetCapture(hwnd);
    return REACH_OK;
}

static void reach_popup_capture_end_capture(
    void *userdata,
    reach_platform_window *surface
)
{
    (void)userdata;
    void *native_handle = reach_windows_platform_window_native_handle(surface);
    if (native_handle == nullptr) {
        return;
    }
    HWND hwnd = static_cast<HWND>(native_handle);
    if (GetCapture() == hwnd) {
        ReleaseCapture();
    }
}

static int32_t reach_popup_capture_is_capture_active(
    void *userdata,
    reach_platform_window *surface
)
{
    (void)userdata;
    void *native_handle = reach_windows_platform_window_native_handle(surface);
    if (native_handle == nullptr) {
        return 0;
    }
    HWND hwnd = static_cast<HWND>(native_handle);
    return GetCapture() == hwnd ? 1 : 0;
}

static reach_result reach_popup_capture_sync_mouse_hook(
    void *userdata,
    int32_t should_hook,
    reach_popup_capture_mouse_down_callback callback,
    void *callback_userdata
)
{
    reach_popup_capture_adapter *adapter =
        static_cast<reach_popup_capture_adapter *>(userdata);
    if (adapter == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    if (should_hook && adapter->mouse_hook == nullptr) {
        adapter->mouse_callback = callback;
        adapter->mouse_callback_userdata = callback_userdata;
        adapter->mouse_hook = SetWindowsHookExW(
            WH_MOUSE_LL,
            reach_popup_capture_mouse_hook_proc,
            GetModuleHandleW(nullptr),
            0);
        if (adapter->mouse_hook != nullptr) {
            g_reach_popup_capture_instance = adapter;
        } else {
            adapter->mouse_callback = nullptr;
            adapter->mouse_callback_userdata = nullptr;
            return REACH_ERROR;
        }
    } else if (!should_hook && adapter->mouse_hook != nullptr) {
        UnhookWindowsHookEx(adapter->mouse_hook);
        adapter->mouse_hook = nullptr;
        adapter->mouse_callback = nullptr;
        adapter->mouse_callback_userdata = nullptr;
        if (g_reach_popup_capture_instance == adapter) {
            g_reach_popup_capture_instance = nullptr;
        }
    }

    return REACH_OK;
}

static void reach_popup_capture_destroy(void *userdata)
{
    reach_popup_capture_adapter *adapter =
        static_cast<reach_popup_capture_adapter *>(userdata);
    if (adapter == nullptr) {
        return;
    }

    if (adapter->mouse_hook != nullptr) {
        UnhookWindowsHookEx(adapter->mouse_hook);
        adapter->mouse_hook = nullptr;
    }

    adapter->mouse_callback = nullptr;
    adapter->mouse_callback_userdata = nullptr;

    if (g_reach_popup_capture_instance == adapter) {
        g_reach_popup_capture_instance = nullptr;
    }

    delete adapter;
}

extern "C" reach_result reach_windows_create_popup_capture(
    reach_popup_capture_port *out_port
)
{
    if (out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_popup_capture_adapter *adapter = new (std::nothrow)
        reach_popup_capture_adapter();
    if (adapter == nullptr) {
        return REACH_ERROR;
    }

    adapter->mouse_hook = nullptr;
    adapter->mouse_callback = nullptr;
    adapter->mouse_callback_userdata = nullptr;

    out_port->userdata = adapter;
    out_port->begin_capture = reach_popup_capture_begin_capture;
    out_port->end_capture = reach_popup_capture_end_capture;
    out_port->is_capture_active = reach_popup_capture_is_capture_active;
    out_port->sync_mouse_hook = reach_popup_capture_sync_mouse_hook;
    out_port->destroy = reach_popup_capture_destroy;

    return REACH_OK;
}
