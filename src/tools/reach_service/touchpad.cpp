#include "touchpad.h"

#include "../../adapters/windows/touchpad_reports_win32.h"
#include "../../adapters/windows/touchpad_swipe.h"
#include "../../adapters/windows/window_management/reach_service_shared_state_win32.h"

#include <windows.h>

static const wchar_t *REACH_HELPER_TOUCHPAD_WINDOW_CLASS = L"ReachHelperTouchpadWindow";
static const USHORT REACH_HELPER_HID_PAGE_DIGITIZER = 0x0D;
static const USHORT REACH_HELPER_HID_USAGE_TOUCHPAD = 0x05;

struct reach_helper_touchpad
{
    HWND window;
    reach_touchpad_reports *reports;
    reach_touchpad_swipe swipe;
    int32_t (*alt_tab_active)(void);
};

static reach_helper_touchpad g_touchpad;

static void reach_helper_touchpad_handle_frame(const reach_touchpad_frame *frame)
{
    reach_ui_event_type events[8] = {};
    const size_t count = reach_touchpad_swipe_update(&g_touchpad.swipe, frame, events,
                                                     sizeof(events) / sizeof(events[0]));
    for (size_t index = 0; index < count; ++index)
    {
        /* The keyboard and the touchpad share one switcher; a keyboard switch in progress wins. */
        if (events[index] == REACH_UI_EVENT_APP_SWITCH_BEGIN &&
            g_touchpad.alt_tab_active != nullptr && g_touchpad.alt_tab_active())
        {
            reach_touchpad_swipe_abandon(&g_touchpad.swipe);
            return;
        }
        (void)reach_service_shared_append_gesture((uint32_t)events[index]);
    }
}

static LRESULT CALLBACK reach_helper_touchpad_window_proc(HWND hwnd, UINT message, WPARAM wparam,
                                                          LPARAM lparam)
{
    if (message == WM_INPUT && g_touchpad.reports != nullptr)
    {
        reach_touchpad_frame frame = {};
        if (reach_touchpad_reports_feed(g_touchpad.reports, reinterpret_cast<HRAWINPUT>(lparam),
                                        &frame))
        {
            /* Raw input carries no timestamp; the message time is when Windows queued it. */
            frame.time_seconds = (double)(DWORD)GetMessageTime() / 1000.0;
            reach_helper_touchpad_handle_frame(&frame);
        }
        /* DefWindowProc releases the raw input buffer. */
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

void reach_helper_touchpad_start(int32_t (*alt_tab_active)(void))
{
    if (g_touchpad.window != nullptr)
    {
        return;
    }
    g_touchpad = {};
    g_touchpad.alt_tab_active = alt_tab_active;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = reach_helper_touchpad_window_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = REACH_HELPER_TOUCHPAD_WINDOW_CLASS;
    if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return;
    }
    g_touchpad.window = CreateWindowExW(0, REACH_HELPER_TOUCHPAD_WINDOW_CLASS, L"", 0, 0, 0, 0, 0,
                                        HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    if (g_touchpad.window == nullptr)
    {
        return;
    }
    g_touchpad.reports = reach_touchpad_reports_create();
    if (g_touchpad.reports == nullptr)
    {
        reach_helper_touchpad_stop();
        return;
    }

    RAWINPUTDEVICE device = {};
    device.usUsagePage = REACH_HELPER_HID_PAGE_DIGITIZER;
    device.usUsage = REACH_HELPER_HID_USAGE_TOUCHPAD;
    device.dwFlags = RIDEV_INPUTSINK;
    device.hwndTarget = g_touchpad.window;
    /* Machines without a precision touchpad simply never send reports. */
    (void)RegisterRawInputDevices(&device, 1, sizeof(device));
}

void reach_helper_touchpad_stop(void)
{
    if (g_touchpad.window != nullptr)
    {
        RAWINPUTDEVICE device = {};
        device.usUsagePage = REACH_HELPER_HID_PAGE_DIGITIZER;
        device.usUsage = REACH_HELPER_HID_USAGE_TOUCHPAD;
        device.dwFlags = RIDEV_REMOVE;
        (void)RegisterRawInputDevices(&device, 1, sizeof(device));
        DestroyWindow(g_touchpad.window);
    }
    reach_touchpad_reports_destroy(g_touchpad.reports);
    g_touchpad = {};
}
