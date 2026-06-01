#include "windows_adapters_internal.h"

#include "window_management/elevation_helper_client_win32.h"

#include "reach/ports/input_source.h"

#include <windows.h>
#include <sddl.h>

#include <new>
#include <vector>
#include <wchar.h>

struct reach_input_source {
    reach_input_event_callback callback;
    void *user;
    HWND window;
    HHOOK keyboard_hook;
    ATOM window_class;
    int32_t registered_hotkey_count;
    int32_t windows_key_down;
    int32_t windows_key_forwarded;
    int32_t windows_key_chord;
    DWORD windows_key_vk;
    int32_t alt_down;
    int32_t shift_down;
    int32_t alt_tab_active;
    HANDLE helper_event_thread;
    HANDLE helper_event_stop;
    wchar_t helper_event_pipe_name[128];
    LONG external_hotkeys_enabled;
};

static const wchar_t *REACH_INPUT_WINDOW_CLASS = L"ReachInputMessageWindow";
static const UINT REACH_INPUT_WM_WINDOWS_KEY = WM_APP + 20;
static const UINT REACH_INPUT_WM_UI_EVENT = WM_APP + 21;
static reach_input_source *g_reach_keyboard_source;

struct reach_helper_hotkey_event {
    uint32_t version;
    uint32_t event_count;
    uint32_t event_types[2];
};

enum reach_input_hotkey_id {
    REACH_HOTKEY_WIN_SPACE = 3,
    REACH_HOTKEY_CTRL_SPACE = 4
};

static int32_t reach_input_helper_event_type_valid(uint32_t event_type)
{
    switch (event_type) {
    case REACH_UI_EVENT_WINDOWS_KEY:
    case REACH_UI_EVENT_ALT_TAB_BEGIN:
    case REACH_UI_EVENT_ALT_TAB_NEXT:
    case REACH_UI_EVENT_ALT_TAB_PREVIOUS:
    case REACH_UI_EVENT_ALT_TAB_COMMIT:
    case REACH_UI_EVENT_ALT_TAB_CANCEL:
        return 1;
    default:
        return 0;
    }
}

static int32_t reach_input_external_hotkeys_enabled(reach_input_source *source)
{
    if (source == nullptr) {
        return 0;
    }
    return InterlockedCompareExchange(&source->external_hotkeys_enabled, 0, 0) != 0;
}

static void reach_input_set_external_hotkeys_enabled_local(reach_input_source *source, int32_t enabled)
{
    if (source != nullptr) {
        InterlockedExchange(&source->external_hotkeys_enabled, enabled ? 1 : 0);
    }
}

static reach_result reach_input_query_token_user(HANDLE token, std::vector<BYTE> *out_user)
{
    if (token == nullptr || out_user == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD needed = 0;
    (void)GetTokenInformation(token, TokenUser, nullptr, 0, &needed);
    if (needed == 0) {
        return REACH_ERROR;
    }

    out_user->resize(needed);
    if (!GetTokenInformation(token, TokenUser, out_user->data(), needed, &needed)) {
        out_user->clear();
        return REACH_ERROR;
    }

    return REACH_OK;
}

static int32_t reach_input_same_user_client(HANDLE pipe)
{
    HANDLE process_token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token)) {
        return 0;
    }

    std::vector<BYTE> process_user;
    reach_result process_result = reach_input_query_token_user(process_token, &process_user);
    CloseHandle(process_token);
    if (process_result != REACH_OK) {
        return 0;
    }

    if (!ImpersonateNamedPipeClient(pipe)) {
        return 0;
    }

    HANDLE client_token = nullptr;
    BOOL opened_client = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &client_token);
    RevertToSelf();
    if (!opened_client) {
        return 0;
    }

    std::vector<BYTE> client_user;
    reach_result client_result = reach_input_query_token_user(client_token, &client_user);
    CloseHandle(client_token);
    if (client_result != REACH_OK) {
        return 0;
    }

    TOKEN_USER *process_token_user = reinterpret_cast<TOKEN_USER *>(process_user.data());
    TOKEN_USER *client_token_user = reinterpret_cast<TOKEN_USER *>(client_user.data());
    if (process_token_user == nullptr || client_token_user == nullptr ||
        process_token_user->User.Sid == nullptr || client_token_user->User.Sid == nullptr) {
        return 0;
    }

    return EqualSid(process_token_user->User.Sid, client_token_user->User.Sid);
}

static SECURITY_ATTRIBUTES reach_input_pipe_security(PSECURITY_DESCRIPTOR *sd)
{
    *sd = nullptr;
    SECURITY_ATTRIBUTES attributes = {};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = FALSE;

    const wchar_t *sddl = L"D:P(A;;GA;;;IU)(A;;GA;;;SY)(A;;GA;;;BA)";
    if (ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, sd, nullptr)) {
        attributes.lpSecurityDescriptor = *sd;
    }

    return attributes;
}

static HANDLE reach_input_create_event_pipe(const wchar_t *name)
{
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    SECURITY_ATTRIBUTES security = reach_input_pipe_security(&descriptor);
    HANDLE pipe = CreateNamedPipeW(
        name,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1,
        sizeof(reach_helper_hotkey_event),
        sizeof(reach_helper_hotkey_event),
        250,
        security.lpSecurityDescriptor != nullptr ? &security : nullptr);

    if (descriptor != nullptr) {
        LocalFree(descriptor);
    }

    return pipe;
}

static DWORD WINAPI reach_input_helper_event_thread(void *param)
{
    reach_input_source *source = static_cast<reach_input_source *>(param);
    if (source == nullptr) {
        return 0;
    }

    for (;;) {
        if (WaitForSingleObject(source->helper_event_stop, 0) == WAIT_OBJECT_0) {
            return 0;
        }

        HANDLE pipe = reach_input_create_event_pipe(source->helper_event_pipe_name);
        if (pipe == INVALID_HANDLE_VALUE) {
            Sleep(250);
            continue;
        }

        BOOL connected = ConnectNamedPipe(pipe, nullptr) ? TRUE : GetLastError() == ERROR_PIPE_CONNECTED;
        if (connected) {
            int32_t same_user = 0;
            int32_t same_user_checked = 0;
            for (;;) {
                reach_helper_hotkey_event event = {};
                DWORD read = 0;
                if (!ReadFile(pipe, &event, sizeof(event), &read, nullptr) ||
                    read != sizeof(event)) {
                    break;
                }
                if (!same_user_checked) {
                    same_user = reach_input_same_user_client(pipe);
                    same_user_checked = 1;
                }
                if (same_user &&
                    event.version == reach_elevation_helper_protocol_version() &&
                    event.event_count > 0 && event.event_count <= 2) {
                    for (uint32_t index = 0; index < event.event_count; ++index) {
                        if (reach_input_helper_event_type_valid(event.event_types[index])) {
                            PostMessageW(
                                source->window,
                                REACH_INPUT_WM_UI_EVENT,
                                event.event_types[index],
                                0);
                        }
                    }
                }
            }
            if (same_user_checked && same_user &&
                WaitForSingleObject(source->helper_event_stop, 0) != WAIT_OBJECT_0) {
                reach_input_set_external_hotkeys_enabled_local(source, 0);
            }
            DisconnectNamedPipe(pipe);
        }

        CloseHandle(pipe);
    }
}

static void reach_input_wake_helper_event_thread(reach_input_source *source)
{
    if (source == nullptr || source->helper_event_pipe_name[0] == 0) {
        return;
    }

    HANDLE pipe = CreateFileW(
        source->helper_event_pipe_name,
        GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);
    if (pipe != INVALID_HANDLE_VALUE) {
        reach_helper_hotkey_event event = {};
        DWORD written = 0;
        (void)WriteFile(pipe, &event, sizeof(event), &written, nullptr);
        CloseHandle(pipe);
    }
}

static void reach_input_send_windows_key_down(DWORD vk)
{
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = static_cast<WORD>(vk == VK_RWIN ? VK_RWIN : VK_LWIN);
    input.ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
    (void)SendInput(1, &input, sizeof(INPUT));
}

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
    if (message == REACH_INPUT_WM_UI_EVENT && source != nullptr && source->callback != nullptr) {
        reach_ui_event event = {};
        event.type = static_cast<reach_ui_event_type>(wparam);
        source->callback(source->user, &event);
        return 0;
    }

    if (message == WM_HOTKEY && source != nullptr && source->callback != nullptr) {
        switch ((int)wparam) {
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
        if (keyboard != nullptr && (keyboard->flags & LLKHF_INJECTED) != 0) {
            return CallNextHookEx(source->keyboard_hook, code, wparam, lparam);
        }
        if (reach_input_external_hotkeys_enabled(source) && keyboard != nullptr &&
            (keyboard->vkCode == VK_MENU || keyboard->vkCode == VK_LMENU ||
             keyboard->vkCode == VK_RMENU || keyboard->vkCode == VK_TAB ||
             keyboard->vkCode == VK_ESCAPE || keyboard->vkCode == VK_LWIN ||
             keyboard->vkCode == VK_RWIN)) {
            return CallNextHookEx(source->keyboard_hook, code, wparam, lparam);
        }
        if (keyboard != nullptr &&
            (keyboard->vkCode == VK_SHIFT || keyboard->vkCode == VK_LSHIFT || keyboard->vkCode == VK_RSHIFT)) {
            source->shift_down = (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN) ? 1 : 0;
        }
        if (keyboard != nullptr &&
            (keyboard->vkCode == VK_MENU || keyboard->vkCode == VK_LMENU || keyboard->vkCode == VK_RMENU)) {
            if (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN) {
                source->alt_down = 1;
            } else if (wparam == WM_KEYUP || wparam == WM_SYSKEYUP) {
                source->alt_down = 0;
                if (source->alt_tab_active) {
                    source->alt_tab_active = 0;
                    PostMessageW(source->window, REACH_INPUT_WM_UI_EVENT, REACH_UI_EVENT_ALT_TAB_COMMIT, 0);
                }
            }
        }
        if (keyboard != nullptr &&
            keyboard->vkCode == VK_TAB &&
            (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN) &&
            (source->alt_down || (keyboard->flags & LLKHF_ALTDOWN) != 0)) {
            if (!source->alt_tab_active) {
                source->alt_tab_active = 1;
                PostMessageW(source->window, REACH_INPUT_WM_UI_EVENT, REACH_UI_EVENT_ALT_TAB_BEGIN, 0);
            }
            PostMessageW(
                source->window,
                REACH_INPUT_WM_UI_EVENT,
                source->shift_down ? REACH_UI_EVENT_ALT_TAB_PREVIOUS : REACH_UI_EVENT_ALT_TAB_NEXT,
                0);
            return 1;
        }
        if (keyboard != nullptr &&
            keyboard->vkCode == VK_ESCAPE &&
            source->alt_tab_active &&
            (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN)) {
            source->alt_tab_active = 0;
            PostMessageW(source->window, REACH_INPUT_WM_UI_EVENT, REACH_UI_EVENT_ALT_TAB_CANCEL, 0);
            return 1;
        }
        if (keyboard != nullptr && (keyboard->vkCode == VK_LWIN || keyboard->vkCode == VK_RWIN)) {
            if (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN) {
                if (!source->windows_key_down) {
                    source->windows_key_down = 1;
                    source->windows_key_forwarded = 0;
                    source->windows_key_chord = 0;
                    source->windows_key_vk = keyboard->vkCode;
                }
                return 1;
            }
            if (wparam == WM_KEYUP || wparam == WM_SYSKEYUP) {
                int32_t forwarded = source->windows_key_forwarded;
                int32_t chord = source->windows_key_chord;
                source->windows_key_down = 0;
                source->windows_key_forwarded = 0;
                source->windows_key_chord = 0;
                source->windows_key_vk = 0;
                if (forwarded || chord) {
                    return CallNextHookEx(source->keyboard_hook, code, wparam, lparam);
                }
                PostMessageW(source->window, REACH_INPUT_WM_WINDOWS_KEY, 0, 0);
                return 1;
            }
        }
        if (keyboard != nullptr &&
            source->windows_key_down &&
            !source->windows_key_forwarded &&
            (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN)) {
            source->windows_key_chord = 1;
            source->windows_key_forwarded = 1;
            reach_input_send_windows_key_down(source->windows_key_vk);
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
    source->windows_key_forwarded = 0;
    source->windows_key_chord = 0;
    source->windows_key_vk = 0;
    source->alt_down = 0;
    source->shift_down = 0;
    source->alt_tab_active = 0;
    reach_input_set_external_hotkeys_enabled_local(source, 0);
    if (source->helper_event_pipe_name[0] == 0) {
        swprintf_s(
            source->helper_event_pipe_name,
            128,
            L"\\\\.\\pipe\\ReachInputEvents-%lu",
            static_cast<unsigned long>(GetCurrentProcessId()));
    }
    if (source->helper_event_stop == nullptr) {
        source->helper_event_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    } else {
        ResetEvent(source->helper_event_stop);
    }
    if (source->helper_event_thread == nullptr && source->helper_event_stop != nullptr) {
        source->helper_event_thread = CreateThread(
            nullptr,
            0,
            reach_input_helper_event_thread,
            source,
            0,
            nullptr);
    }
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

static reach_result reach_input_set_external_hotkey_forwarding_enabled(reach_input_source *source, int32_t enabled)
{
    if (source == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    int32_t next_enabled = enabled ? 1 : 0;
    if (reach_input_external_hotkeys_enabled(source) == next_enabled) {
        return REACH_OK;
    }

    uint32_t hotkey_mask =
        REACH_ELEVATION_HELPER_HOTKEY_ALT_TAB |
        REACH_ELEVATION_HELPER_HOTKEY_WINDOWS_KEY;
    reach_result result = reach_elevation_helper_set_hotkey_forwarding(
        next_enabled,
        hotkey_mask,
        source->helper_event_pipe_name);

    if (result == REACH_OK) {
        reach_input_set_external_hotkeys_enabled_local(source, next_enabled);
    } else if (!next_enabled) {
        reach_input_set_external_hotkeys_enabled_local(source, 0);
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
        UnregisterHotKey(source->window, REACH_HOTKEY_WIN_SPACE);
        UnregisterHotKey(source->window, REACH_HOTKEY_CTRL_SPACE);
    }
    (void)reach_input_set_external_hotkey_forwarding_enabled(source, 0);
    if (source->windows_key_down) {
        INPUT release[2] = {};
        release[0].type = INPUT_KEYBOARD;
        release[0].ki.wVk = VK_LWIN;
        release[0].ki.dwFlags = KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
        release[1].type = INPUT_KEYBOARD;
        release[1].ki.wVk = VK_RWIN;
        release[1].ki.dwFlags = KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
        (void)SendInput(2, release, sizeof(INPUT));
    }
    if (source->keyboard_hook != nullptr) {
        UnhookWindowsHookEx(source->keyboard_hook);
        source->keyboard_hook = nullptr;
    }
    if (g_reach_keyboard_source == source) {
        g_reach_keyboard_source = nullptr;
    }
    if (source->helper_event_stop != nullptr) {
        SetEvent(source->helper_event_stop);
        reach_input_wake_helper_event_thread(source);
    }
    if (source->helper_event_thread != nullptr) {
        WaitForSingleObject(source->helper_event_thread, 1000);
        CloseHandle(source->helper_event_thread);
        source->helper_event_thread = nullptr;
    }
    if (source->helper_event_stop != nullptr) {
        CloseHandle(source->helper_event_stop);
        source->helper_event_stop = nullptr;
    }
    source->registered_hotkey_count = 0;
    source->windows_key_down = 0;
    source->windows_key_forwarded = 0;
    source->windows_key_chord = 0;
    source->windows_key_vk = 0;
    source->alt_down = 0;
    source->shift_down = 0;
    source->alt_tab_active = 0;
    reach_input_set_external_hotkeys_enabled_local(source, 0);
    source->callback = nullptr;
    source->user = nullptr;
    return REACH_OK;
}

static reach_result reach_input_get_pointer_position(reach_input_source *source, reach_point_i32 *out_position)
{
    (void)source;
    if (out_position == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    POINT cursor = {};
    if (!GetCursorPos(&cursor)) {
        return REACH_ERROR;
    }

    out_position->x = cursor.x;
    out_position->y = cursor.y;
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
    out_port->ops.get_pointer_position = reach_input_get_pointer_position;
    out_port->ops.set_external_hotkey_forwarding_enabled =
        reach_input_set_external_hotkey_forwarding_enabled;
    out_port->ops.destroy = reach_input_destroy;
    return REACH_OK;
}
