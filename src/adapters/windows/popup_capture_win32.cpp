#include "windows_adapters_internal.h"

#include <windows.h>
#include <windowsx.h>

#include <new>

#define REACH_POPUP_CAPTURE_WM_MOUSE_DOWN (WM_APP + 91)

struct reach_popup_capture_adapter
{
    HHOOK mouse_hook;
    HWND message_window;
    reach_popup_capture_mouse_down_callback mouse_callback;
    void *mouse_callback_userdata;
};

static reach_popup_capture_adapter *g_reach_popup_capture_instance;

static LRESULT CALLBACK reach_popup_capture_message_window_proc(HWND hwnd, UINT message,
                                                                WPARAM wparam, LPARAM lparam)
{
    if (message == REACH_POPUP_CAPTURE_WM_MOUSE_DOWN)
    {
        reach_popup_capture_adapter *adapter =
            reinterpret_cast<reach_popup_capture_adapter *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (adapter != nullptr && adapter->mouse_callback != nullptr)
        {
            int32_t x = static_cast<int32_t>(GET_X_LPARAM(lparam));
            int32_t y = static_cast<int32_t>(GET_Y_LPARAM(lparam));
            adapter->mouse_callback(adapter->mouse_callback_userdata, x, y);
        }
        return 0;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static LRESULT CALLBACK reach_popup_capture_mouse_hook_proc(int code, WPARAM wparam, LPARAM lparam)
{
    if (code >= 0 && g_reach_popup_capture_instance != nullptr)
    {
        if (wparam == WM_LBUTTONDOWN || wparam == WM_RBUTTONDOWN || wparam == WM_MBUTTONDOWN ||
            wparam == WM_XBUTTONDOWN)
        {
            MSLLHOOKSTRUCT *mouse = reinterpret_cast<MSLLHOOKSTRUCT *>(lparam);
            if (mouse != nullptr && g_reach_popup_capture_instance->message_window != nullptr)
            {
                PostMessageW(g_reach_popup_capture_instance->message_window,
                             REACH_POPUP_CAPTURE_WM_MOUSE_DOWN, 0,
                             MAKELPARAM(mouse->pt.x, mouse->pt.y));
            }
        }
    }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

static HWND reach_popup_capture_create_message_window(reach_popup_capture_adapter *adapter)
{
    if (adapter == nullptr)
    {
        return nullptr;
    }

    HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = reach_popup_capture_message_window_proc;
    wc.hInstance = instance;
    wc.lpszClassName = L"ReachPopupCaptureMessageWindow";

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                instance, nullptr);

    if (hwnd != nullptr)
    {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(adapter));
    }

    return hwnd;
}

static reach_result
reach_popup_capture_sync_mouse_hook(void *userdata, int32_t should_hook,
                                    reach_popup_capture_mouse_down_callback callback,
                                    void *callback_userdata)
{
    reach_popup_capture_adapter *adapter = static_cast<reach_popup_capture_adapter *>(userdata);
    if (adapter == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (should_hook)
    {
        adapter->mouse_callback = callback;
        adapter->mouse_callback_userdata = callback_userdata;
        if (adapter->message_window == nullptr)
        {
            adapter->message_window = reach_popup_capture_create_message_window(adapter);
            if (adapter->message_window == nullptr)
            {
                adapter->mouse_callback = nullptr;
                adapter->mouse_callback_userdata = nullptr;
                return REACH_ERROR;
            }
        }
        /* Re-assert: the OS silently removes a low-level hook whose thread
           stalls past the hook timeout — without this, an open popup would
           quietly stop closing on outside clicks. Re-installing is cheap. */
        if (adapter->mouse_hook != nullptr)
        {
            UnhookWindowsHookEx(adapter->mouse_hook);
            adapter->mouse_hook = nullptr;
        }
        adapter->mouse_hook = SetWindowsHookExW(WH_MOUSE_LL, reach_popup_capture_mouse_hook_proc,
                                                GetModuleHandleW(nullptr), 0);
        if (adapter->mouse_hook != nullptr)
        {
            g_reach_popup_capture_instance = adapter;
        }
        else
        {
            adapter->mouse_callback = nullptr;
            adapter->mouse_callback_userdata = nullptr;
            return REACH_ERROR;
        }
    }
    else if (!should_hook && adapter->mouse_hook != nullptr)
    {
        UnhookWindowsHookEx(adapter->mouse_hook);
        adapter->mouse_hook = nullptr;
        adapter->mouse_callback = nullptr;
        adapter->mouse_callback_userdata = nullptr;
        if (g_reach_popup_capture_instance == adapter)
        {
            g_reach_popup_capture_instance = nullptr;
        }
    }

    return REACH_OK;
}

static void reach_popup_capture_destroy(void *userdata)
{
    reach_popup_capture_adapter *adapter = static_cast<reach_popup_capture_adapter *>(userdata);
    if (adapter == nullptr)
    {
        return;
    }

    if (adapter->mouse_hook != nullptr)
    {
        UnhookWindowsHookEx(adapter->mouse_hook);
        adapter->mouse_hook = nullptr;
    }

    adapter->mouse_callback = nullptr;
    adapter->mouse_callback_userdata = nullptr;

    if (g_reach_popup_capture_instance == adapter)
    {
        g_reach_popup_capture_instance = nullptr;
    }
    if (adapter->message_window != nullptr)
    {
        DestroyWindow(adapter->message_window);
        adapter->message_window = nullptr;
    }
    delete adapter;
}

extern "C" reach_result reach_windows_create_popup_capture(reach_popup_capture_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_popup_capture_adapter *adapter = new (std::nothrow) reach_popup_capture_adapter();
    if (adapter == nullptr)
    {
        return REACH_ERROR;
    }

    adapter->mouse_hook = nullptr;
    adapter->mouse_callback = nullptr;
    adapter->mouse_callback_userdata = nullptr;
    adapter->message_window = nullptr;

    out_port->userdata = adapter;
    out_port->sync_mouse_hook = reach_popup_capture_sync_mouse_hook;
    out_port->destroy = reach_popup_capture_destroy;

    return REACH_OK;
}
