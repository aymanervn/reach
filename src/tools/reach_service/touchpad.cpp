#include "touchpad.h"

#include "../../adapters/windows/touchpad_reports_win32.h"
#include "touchpad_swipe.h"

#include "../../adapters/windows/window_management/reach_service_shared_state_win32.h"

#include <windows.h>
#include <wtsapi32.h>

static const wchar_t *REACH_HELPER_TOUCHPAD_WINDOW_CLASS = L"ReachHelperTouchpadWindow";
static const USHORT REACH_HELPER_HID_PAGE_DIGITIZER = 0x0D;
static const USHORT REACH_HELPER_HID_USAGE_TOUCHPAD = 0x05;

struct reach_helper_touchpad
{
    HWND window;
    HWINEVENTHOOK desktop_switch_hook;
    reach_touchpad_reports *reports;
    reach_touchpad_swipe swipe;
};

static reach_helper_touchpad g_touchpad;

static void reach_helper_touchpad_handle_frame(const reach_touchpad_frame *frame, void *user)
{
    (void)user;
    reach_ui_event_type events[8] = {};
    const size_t count = reach_touchpad_swipe_update(&g_touchpad.swipe, frame, events,
                                                     sizeof(events) / sizeof(events[0]));
    for (size_t index = 0; index < count; ++index)
    {
        (void)reach_service_shared_append_gesture((uint32_t)events[index]);
    }
}

static void reach_helper_touchpad_cancel_switch(void)
{
    /* Never leave reach.exe with a switcher only this swipe could close. */
    if (reach_touchpad_swipe_switching(&g_touchpad.swipe))
    {
        (void)reach_service_shared_append_gesture(REACH_UI_EVENT_APP_SWITCH_CANCEL);
    }
    reach_touchpad_swipe_abandon(&g_touchpad.swipe);
}

static LRESULT CALLBACK reach_helper_touchpad_window_proc(HWND hwnd, UINT message, WPARAM wparam,
                                                          LPARAM lparam)
{
    /* Locking switches to the secure desktop, where no touchpad reports arrive. */
    if (message == WM_WTSSESSION_CHANGE && wparam == WTS_SESSION_LOCK)
    {
        reach_helper_touchpad_cancel_switch();
        return 0;
    }
    if (message == WM_INPUT && g_touchpad.reports != nullptr)
    {
        /* Raw input carries no timestamp; the message time is when Windows queued it. */
        (void)reach_touchpad_reports_feed(g_touchpad.reports, reinterpret_cast<HRAWINPUT>(lparam),
                                          (double)(DWORD)GetMessageTime() / 1000.0,
                                          reach_helper_touchpad_handle_frame, nullptr);
        /* DefWindowProc releases the raw input buffer. */
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static void CALLBACK reach_helper_touchpad_desktop_switched(HWINEVENTHOOK hook, DWORD event,
                                                            HWND hwnd, LONG object, LONG child,
                                                            DWORD thread, DWORD time)
{
    (void)hook;
    (void)event;
    (void)hwnd;
    (void)object;
    (void)child;
    (void)thread;
    (void)time;
    /* UAC prompts and Ctrl+Alt+Del show the secure desktop without locking the session; no
       touchpad reports arrive there, so the fingers' lift would never be seen. */
    reach_helper_touchpad_cancel_switch();
}

void reach_helper_touchpad_start(void)
{
    if (g_touchpad.window != nullptr)
    {
        return;
    }
    g_touchpad = {};

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
    (void)WTSRegisterSessionNotification(g_touchpad.window, NOTIFY_FOR_THIS_SESSION);
    g_touchpad.desktop_switch_hook =
        SetWinEventHook(EVENT_SYSTEM_DESKTOPSWITCH, EVENT_SYSTEM_DESKTOPSWITCH, nullptr,
                        reach_helper_touchpad_desktop_switched, 0, 0, WINEVENT_OUTOFCONTEXT);
}

void reach_helper_touchpad_stop(void)
{
    reach_helper_touchpad_cancel_switch();
    if (g_touchpad.desktop_switch_hook != nullptr)
    {
        UnhookWinEvent(g_touchpad.desktop_switch_hook);
    }
    if (g_touchpad.window != nullptr)
    {
        (void)WTSUnRegisterSessionNotification(g_touchpad.window);
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
