#include "reach/platform/windows_adapters.h"

#include "reach/ports/input_source.h"

#include <windows.h>

#include <new>

struct reach_input_source {
    reach_input_event_callback callback;
    void *user;
    HWND window;
    HHOOK keyboard_hook;
    ATOM window_class;
    int32_t registered_hotkey_count;
    int32_t windows_key_down;
};

static const wchar_t *REACH_INPUT_WINDOW_CLASS = L"ReachInputMessageWindow";
static const UINT REACH_INPUT_WM_WINDOWS_KEY = WM_APP + 20;
static reach_input_source *g_reach_keyboard_source;

enum reach_input_hotkey_id {
    REACH_HOTKEY_LEFT_WINDOWS = 1,
    REACH_HOTKEY_RIGHT_WINDOWS = 2,
    REACH_HOTKEY_WIN_SPACE = 3,
    REACH_HOTKEY_CTRL_SPACE = 4
};

static LRESULT CALLBACK reach_input_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_NCCREATE) {
        CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return TRUE;
    }

    reach_input_source *source = reinterpret_cast<reach_input_source *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == REACH_INPUT_WM_WINDOWS_KEY && source != nullptr && source->callback != nullptr) {
        reach_ui_event event = {};
        event.type = REACH_UI_EVENT_WINDOWS_KEY;
        source->callback(source->user, &event);
        return 0;
    }

    if (message == WM_HOTKEY && source != nullptr && source->callback != nullptr) {
        switch ((int)wparam) {
        case REACH_HOTKEY_LEFT_WINDOWS:
        case REACH_HOTKEY_RIGHT_WINDOWS:
        case REACH_HOTKEY_WIN_SPACE:
        case REACH_HOTKEY_CTRL_SPACE: {
            reach_ui_event event = {};
            event.type = REACH_UI_EVENT_WINDOWS_KEY;
            source->callback(source->user, &event);
            return 0;
        }
        default:
            break;
        }
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static LRESULT CALLBACK reach_input_keyboard_proc(int code, WPARAM wparam, LPARAM lparam)
{
    reach_input_source *source = g_reach_keyboard_source;
    if (code == HC_ACTION && source != nullptr && source->window != nullptr) {
        const KBDLLHOOKSTRUCT *keyboard = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lparam);
        if (keyboard != nullptr && (keyboard->vkCode == VK_LWIN || keyboard->vkCode == VK_RWIN)) {
            if (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN) {
                if (!source->windows_key_down) {
                    source->windows_key_down = 1;
                    PostMessageW(source->window, REACH_INPUT_WM_WINDOWS_KEY, 0, 0);
                }
                return 1;
            }
            if (wparam == WM_KEYUP || wparam == WM_SYSKEYUP) {
                source->windows_key_down = 0;
                return 1;
            }
        }
    }

    return CallNextHookEx(source != nullptr ? source->keyboard_hook : nullptr, code, wparam, lparam);
}

static reach_result reach_input_create_window(reach_input_source *source)
{
    REACH_ASSERT(source != nullptr);
    if (source == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = reach_input_window_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = REACH_INPUT_WINDOW_CLASS;
    source->window_class = RegisterClassExW(&wc);
    if (source->window_class == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return REACH_ERROR;
    }

    source->window = CreateWindowExW(
        0,
        REACH_INPUT_WINDOW_CLASS,
        L"",
        0,
        0,
        0,
        0,
        0,
        HWND_MESSAGE,
        nullptr,
        GetModuleHandleW(nullptr),
        source);
    return source->window != nullptr ? REACH_OK : REACH_ERROR;
}

static int32_t reach_input_register_hotkey(reach_input_source *source, int id, UINT modifiers, UINT key)
{
    REACH_ASSERT(source != nullptr);
    if (source == nullptr || source->window == nullptr) {
        return 0;
    }

    if (RegisterHotKey(source->window, id, modifiers | MOD_NOREPEAT, key)) {
        ++source->registered_hotkey_count;
        return 1;
    }
    return 0;
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
    if (source->window == nullptr && reach_input_create_window(source) != REACH_OK) {
        return REACH_ERROR;
    }

    source->registered_hotkey_count = 0;
    source->windows_key_down = 0;
    if (source->keyboard_hook == nullptr) {
        g_reach_keyboard_source = source;
        source->keyboard_hook = SetWindowsHookExW(
            WH_KEYBOARD_LL,
            reach_input_keyboard_proc,
            GetModuleHandleW(nullptr),
            0);
    }
    if (source->keyboard_hook == nullptr) {
        (void)reach_input_register_hotkey(source, REACH_HOTKEY_WIN_SPACE, MOD_WIN, VK_SPACE);
    }
    if (source->keyboard_hook == nullptr && source->registered_hotkey_count == 0) {
        (void)reach_input_register_hotkey(source, REACH_HOTKEY_CTRL_SPACE, MOD_CONTROL, VK_SPACE);
    }
    return REACH_OK;
}

static reach_result reach_input_stop(reach_input_source *source)
{
    REACH_ASSERT(source != nullptr);
    if (source == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    if (source->window != nullptr) {
        UnregisterHotKey(source->window, REACH_HOTKEY_LEFT_WINDOWS);
        UnregisterHotKey(source->window, REACH_HOTKEY_RIGHT_WINDOWS);
        UnregisterHotKey(source->window, REACH_HOTKEY_WIN_SPACE);
        UnregisterHotKey(source->window, REACH_HOTKEY_CTRL_SPACE);
    }
    if (source->keyboard_hook != nullptr) {
        UnhookWindowsHookEx(source->keyboard_hook);
        source->keyboard_hook = nullptr;
    }
    if (g_reach_keyboard_source == source) {
        g_reach_keyboard_source = nullptr;
    }
    source->registered_hotkey_count = 0;
    source->windows_key_down = 0;
    source->callback = nullptr;
    source->user = nullptr;
    return REACH_OK;
}

static void reach_input_destroy(reach_input_source *source)
{
    if (source != nullptr) {
        (void)reach_input_stop(source);
        if (source->window != nullptr) {
            DestroyWindow(source->window);
            source->window = nullptr;
        }
    }
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
