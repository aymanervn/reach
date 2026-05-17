#include "reach/platform/windows_adapters.h"

#include "reach/ports/input_source.h"

#include <windows.h>

#include <new>

struct reach_input_source {
    reach_input_event_callback callback;
    void *user;
    HHOOK keyboard_hook;
};

static reach_input_source *g_keyboard_source;

static LRESULT CALLBACK reach_keyboard_proc(int code, WPARAM wparam, LPARAM lparam)
{
    if (code == HC_ACTION && g_keyboard_source != nullptr && g_keyboard_source->callback != nullptr) {
        const KBDLLHOOKSTRUCT *keyboard = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lparam);
        if (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN) {
            reach_ui_event event = {};
            if (keyboard->vkCode == VK_LWIN || keyboard->vkCode == VK_RWIN) {
                event.type = REACH_UI_EVENT_WINDOWS_KEY;
                g_keyboard_source->callback(g_keyboard_source->user, &event);
            } else if (keyboard->vkCode == VK_ESCAPE) {
                event.type = REACH_UI_EVENT_ESCAPE;
                g_keyboard_source->callback(g_keyboard_source->user, &event);
            } else if (keyboard->vkCode == VK_BACK) {
                event.type = REACH_UI_EVENT_BACKSPACE;
                g_keyboard_source->callback(g_keyboard_source->user, &event);
            }
        }
    }

    return CallNextHookEx(nullptr, code, wparam, lparam);
}

static reach_result reach_input_start(reach_input_source *source, reach_input_event_callback callback, void *user)
{
    REACH_ASSERT(source != nullptr);
    REACH_ASSERT(callback != nullptr);
    if (source == nullptr || callback == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    source->callback = callback;
    source->user = user;
    g_keyboard_source = source;
    source->keyboard_hook = SetWindowsHookExW(WH_KEYBOARD_LL, reach_keyboard_proc, GetModuleHandleW(nullptr), 0);
    return source->keyboard_hook != nullptr ? REACH_OK : REACH_ERROR;
}

static reach_result reach_input_stop(reach_input_source *source)
{
    REACH_ASSERT(source != nullptr);
    if (source == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    if (source->keyboard_hook != nullptr) {
        UnhookWindowsHookEx(source->keyboard_hook);
        source->keyboard_hook = nullptr;
    }
    if (g_keyboard_source == source) {
        g_keyboard_source = nullptr;
    }
    source->callback = nullptr;
    source->user = nullptr;
    return REACH_OK;
}

static void reach_input_destroy(reach_input_source *source)
{
    delete source;
}

reach_result reach_windows_create_input_source(reach_input_source_port *out_port)
{
    REACH_ASSERT(out_port != nullptr);
    if (out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_input_source *source = new (std::nothrow) reach_input_source();
    if (source == nullptr) {
        return REACH_ERROR;
    }

    out_port->source = source;
    out_port->ops.start = reach_input_start;
    out_port->ops.stop = reach_input_stop;
    out_port->ops.destroy = reach_input_destroy;
    return REACH_OK;
}
