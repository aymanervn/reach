#include "windows_adapters_internal.h"

#include <windows.h>

#include <new>

struct reach_foreground_watcher
{
    HWINEVENTHOOK hook;
    reach_foreground_watcher_callback callback;
    void *user;
    HWND foreground;
};

static reach_foreground_watcher *g_foreground_watcher = nullptr;

static void CALLBACK reach_foreground_watcher_proc(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
                                                   LONG object_id, LONG child_id, DWORD thread,
                                                   DWORD time)
{
    (void)hook;
    (void)child_id;
    (void)thread;
    (void)time;

    reach_foreground_watcher *watcher = g_foreground_watcher;
    if (watcher == nullptr || event != EVENT_SYSTEM_FOREGROUND || object_id != OBJID_WINDOW ||
        hwnd == nullptr)
    {
        return;
    }

    watcher->foreground = hwnd;
    if (watcher->callback != nullptr)
    {
        watcher->callback(watcher->user);
    }
}

static reach_result reach_foreground_watcher_start(reach_foreground_watcher *watcher,
                                                   reach_foreground_watcher_callback callback,
                                                   void *user)
{
    if (watcher == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    watcher->callback = callback;
    watcher->user = user;
    watcher->foreground = GetForegroundWindow();

    if (watcher->hook == nullptr)
    {
        watcher->hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
                                        reach_foreground_watcher_proc, 0, 0,
                                        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    }

    return watcher->hook != nullptr ? REACH_OK : REACH_ERROR;
}

static reach_result reach_foreground_watcher_stop(reach_foreground_watcher *watcher)
{
    if (watcher == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (watcher->hook != nullptr)
    {
        UnhookWinEvent(watcher->hook);
        watcher->hook = nullptr;
    }
    watcher->callback = nullptr;
    watcher->user = nullptr;
    return REACH_OK;
}

static reach_window_id reach_foreground_watcher_foreground(const reach_foreground_watcher *watcher)
{
    return watcher == nullptr ? 0 : reinterpret_cast<reach_window_id>(watcher->foreground);
}

static void reach_foreground_watcher_destroy(reach_foreground_watcher *watcher)
{
    if (watcher == nullptr)
    {
        return;
    }

    (void)reach_foreground_watcher_stop(watcher);
    if (g_foreground_watcher == watcher)
    {
        g_foreground_watcher = nullptr;
    }
    delete watcher;
}

reach_result reach_windows_create_foreground_watcher(reach_foreground_watcher_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_foreground_watcher *watcher = new (std::nothrow) reach_foreground_watcher();
    if (watcher == nullptr)
    {
        return REACH_ERROR;
    }

    watcher->hook = nullptr;
    watcher->callback = nullptr;
    watcher->user = nullptr;
    watcher->foreground = nullptr;
    g_foreground_watcher = watcher;

    *out_port = {};
    out_port->watcher = watcher;
    out_port->ops.start = reach_foreground_watcher_start;
    out_port->ops.stop = reach_foreground_watcher_stop;
    out_port->ops.foreground = reach_foreground_watcher_foreground;
    out_port->ops.destroy = reach_foreground_watcher_destroy;
    return REACH_OK;
}
