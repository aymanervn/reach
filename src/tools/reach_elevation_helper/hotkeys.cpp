#include "hotkeys.h"

#include "../../adapters/windows/window_management/elevation_helper_shared_state_win32.h"

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

struct reach_helper_hotkey_state
{
    HHOOK hook;
    HANDLE thread;
    DWORD thread_id;

    int32_t alt_down;
    int32_t shift_down;
    int32_t alt_tab_active;

    int32_t left_win_down;
    int32_t right_win_down;
    int32_t windows_key_used_as_chord;

    uint64_t captured_win_chord_keys;
};

struct reach_helper_key_event
{
    uint32_t key;
    DWORD vk;
    int32_t down;
    int32_t up;
};

enum reach_helper_key_decision
{
    REACH_HELPER_KEY_CONTINUE = 0,
    REACH_HELPER_KEY_PASS = 1,
    REACH_HELPER_KEY_CONSUME = 2,
};

static reach_helper_hotkey_state g_hotkeys;
static reach_helper_hotkey_callbacks g_callbacks;

static const uint32_t REACH_HELPER_OWNED_WIN_CHORD_KEYS[] = {
    REACH_ELEVATION_HELPER_HOTKEY_D,
    REACH_ELEVATION_HELPER_HOTKEY_T,
};

void reach_helper_hotkeys_configure(const reach_helper_hotkey_callbacks *callbacks)
{
    if (callbacks != nullptr)
    {
        g_callbacks = *callbacks;
    }
    else
    {
        g_callbacks = {};
    }
}

static LRESULT reach_helper_pass_key_event(int code, WPARAM wparam, LPARAM lparam)
{
    return CallNextHookEx(g_hotkeys.hook, code, wparam, lparam);
}

static int32_t reach_helper_is_key_down_message(WPARAM message)
{
    return message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
}

static int32_t reach_helper_is_key_up_message(WPARAM message)
{
    return message == WM_KEYUP || message == WM_SYSKEYUP;
}

static int32_t reach_helper_virtual_key_down(int virtual_key)
{
    return (GetAsyncKeyState(virtual_key) & 0x8000) != 0;
}

static int32_t reach_helper_physical_alt_down(void)
{
    return reach_helper_virtual_key_down(VK_MENU) || reach_helper_virtual_key_down(VK_LMENU) ||
           reach_helper_virtual_key_down(VK_RMENU);
}

static uint32_t reach_helper_hotkey_key_from_vk(DWORD vk)
{
    switch (vk)
    {
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
        return REACH_ELEVATION_HELPER_HOTKEY_ALT;

    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
        return REACH_ELEVATION_HELPER_HOTKEY_SHIFT;

    case VK_TAB:
        return REACH_ELEVATION_HELPER_HOTKEY_TAB;

    case VK_ESCAPE:
        return REACH_ELEVATION_HELPER_HOTKEY_ESCAPE;

    case VK_LWIN:
        return REACH_ELEVATION_HELPER_HOTKEY_LEFT_WIN;

    case VK_RWIN:
        return REACH_ELEVATION_HELPER_HOTKEY_RIGHT_WIN;

    case 'D':
        return REACH_ELEVATION_HELPER_HOTKEY_D;

    case 'T':
        return REACH_ELEVATION_HELPER_HOTKEY_T;

    default:
        return 0;
    }
}

static int32_t reach_helper_hotkey_is_windows_key(uint32_t key)
{
    return key == REACH_ELEVATION_HELPER_HOTKEY_LEFT_WIN ||
           key == REACH_ELEVATION_HELPER_HOTKEY_RIGHT_WIN;
}

static int32_t reach_helper_hotkey_is_modifier(uint32_t key)
{
    return key == REACH_ELEVATION_HELPER_HOTKEY_ALT || key == REACH_ELEVATION_HELPER_HOTKEY_SHIFT ||
           key == REACH_ELEVATION_HELPER_HOTKEY_LEFT_WIN ||
           key == REACH_ELEVATION_HELPER_HOTKEY_RIGHT_WIN;
}

static int32_t reach_helper_windows_key_down(void)
{
    return g_hotkeys.left_win_down || g_hotkeys.right_win_down;
}

static void reach_helper_update_modifier_state(uint32_t key, int32_t pressed)
{
    switch (key)
    {
    case REACH_ELEVATION_HELPER_HOTKEY_ALT:
        g_hotkeys.alt_down = pressed;
        break;

    case REACH_ELEVATION_HELPER_HOTKEY_SHIFT:
        g_hotkeys.shift_down = pressed;
        break;

    case REACH_ELEVATION_HELPER_HOTKEY_LEFT_WIN:
        g_hotkeys.left_win_down = pressed;
        break;

    case REACH_ELEVATION_HELPER_HOTKEY_RIGHT_WIN:
        g_hotkeys.right_win_down = pressed;
        break;

    default:
        break;
    }
}

static void reach_helper_reconcile_modifier_state(void)
{
    g_hotkeys.alt_down = reach_helper_physical_alt_down();
    g_hotkeys.shift_down = reach_helper_virtual_key_down(VK_SHIFT) ||
                           reach_helper_virtual_key_down(VK_LSHIFT) ||
                           reach_helper_virtual_key_down(VK_RSHIFT);
    g_hotkeys.left_win_down = reach_helper_virtual_key_down(VK_LWIN);
    g_hotkeys.right_win_down = reach_helper_virtual_key_down(VK_RWIN);

    if (!reach_helper_windows_key_down())
    {
        g_hotkeys.windows_key_used_as_chord = 0;
    }
}

static uint32_t reach_helper_hotkey_modifiers(void)
{
    uint32_t modifiers = 0;

    if (g_hotkeys.alt_down)
    {
        modifiers |= REACH_ELEVATION_HELPER_MODIFIER_ALT;
    }
    if (g_hotkeys.shift_down)
    {
        modifiers |= REACH_ELEVATION_HELPER_MODIFIER_SHIFT;
    }
    if (g_hotkeys.left_win_down)
    {
        modifiers |= REACH_ELEVATION_HELPER_MODIFIER_LEFT_WIN;
    }
    if (g_hotkeys.right_win_down)
    {
        modifiers |= REACH_ELEVATION_HELPER_MODIFIER_RIGHT_WIN;
    }

    return modifiers;
}

static void reach_helper_append_hotkey(uint32_t key, uint32_t action, uint32_t modifiers)
{
    (void)reach_elevation_helper_shared_append_hotkey(key, action, modifiers);
}

static uint64_t reach_helper_hotkey_bit(uint32_t key)
{
    return key < 64 ? (1ull << key) : 0;
}

static int32_t reach_helper_is_owned_win_chord_key(uint32_t key)
{
    for (size_t index = 0; index < sizeof(REACH_HELPER_OWNED_WIN_CHORD_KEYS) /
                                       sizeof(REACH_HELPER_OWNED_WIN_CHORD_KEYS[0]);
         ++index)
    {
        if (REACH_HELPER_OWNED_WIN_CHORD_KEYS[index] == key)
        {
            return 1;
        }
    }

    return 0;
}

static int32_t reach_helper_captured_win_chord_key_down(uint32_t key)
{
    uint64_t bit = reach_helper_hotkey_bit(key);
    return bit != 0 && (g_hotkeys.captured_win_chord_keys & bit) != 0;
}

static void reach_helper_set_captured_win_chord_key(uint32_t key)
{
    uint64_t bit = reach_helper_hotkey_bit(key);
    if (bit != 0)
    {
        g_hotkeys.captured_win_chord_keys |= bit;
    }
}

static void reach_helper_clear_captured_win_chord_key(uint32_t key)
{
    uint64_t bit = reach_helper_hotkey_bit(key);
    if (bit != 0)
    {
        g_hotkeys.captured_win_chord_keys &= ~bit;
    }
}

void reach_helper_clear_hotkey_state(void)
{
    g_hotkeys.alt_down = 0;
    g_hotkeys.shift_down = 0;
    g_hotkeys.alt_tab_active = 0;

    g_hotkeys.left_win_down = 0;
    g_hotkeys.right_win_down = 0;
    g_hotkeys.windows_key_used_as_chord = 0;

    g_hotkeys.captured_win_chord_keys = 0;
}

static int32_t reach_helper_game_mode_active(void)
{
    return g_callbacks.game_mode_active != nullptr && g_callbacks.game_mode_active();
}

static reach_helper_key_decision
reach_helper_handle_game_mode_key(const reach_helper_key_event *event)
{
    if (!reach_helper_game_mode_active())
    {
        return REACH_HELPER_KEY_CONTINUE;
    }

    if (event->key == REACH_ELEVATION_HELPER_HOTKEY_TAB && event->down &&
        reach_helper_physical_alt_down())
    {
        HWND game = GetForegroundWindow();
        reach_helper_clear_hotkey_state();

        if (g_callbacks.minimize_game != nullptr && g_callbacks.minimize_game(game))
        {
            return REACH_HELPER_KEY_CONSUME;
        }
    }

    return REACH_HELPER_KEY_PASS;
}

static reach_helper_key_decision
reach_helper_handle_alt_tab_key(const reach_helper_key_event *event)
{
    if (event->key == REACH_ELEVATION_HELPER_HOTKEY_TAB && event->down && g_hotkeys.alt_down)
    {
        g_hotkeys.alt_tab_active = 1;
        reach_helper_append_hotkey(event->key, REACH_ELEVATION_HELPER_HOTKEY_PRESSED,
                                   reach_helper_hotkey_modifiers());
        return REACH_HELPER_KEY_CONSUME;
    }

    if (event->key == REACH_ELEVATION_HELPER_HOTKEY_ESCAPE && event->down &&
        g_hotkeys.alt_tab_active)
    {
        g_hotkeys.alt_tab_active = 0;
        reach_helper_append_hotkey(event->key, REACH_ELEVATION_HELPER_HOTKEY_PRESSED,
                                   reach_helper_hotkey_modifiers());
        return REACH_HELPER_KEY_CONSUME;
    }

    if (event->key == REACH_ELEVATION_HELPER_HOTKEY_ALT && event->up && g_hotkeys.alt_tab_active)
    {
        reach_helper_append_hotkey(event->key, REACH_ELEVATION_HELPER_HOTKEY_RELEASED,
                                   reach_helper_hotkey_modifiers());
        g_hotkeys.alt_tab_active = 0;
        reach_helper_update_modifier_state(event->key, 0);
        return REACH_HELPER_KEY_PASS;
    }

    if (g_hotkeys.alt_tab_active)
    {
        if (reach_helper_hotkey_is_modifier(event->key))
        {
            reach_helper_update_modifier_state(event->key, event->down ? 1 : 0);
        }
        return REACH_HELPER_KEY_PASS;
    }

    return REACH_HELPER_KEY_CONTINUE;
}

static reach_helper_key_decision
reach_helper_handle_windows_key(const reach_helper_key_event *event)
{
    if (!reach_helper_hotkey_is_windows_key(event->key))
    {
        return REACH_HELPER_KEY_CONTINUE;
    }

    if (event->down)
    {
        if (!reach_helper_windows_key_down())
        {
            g_hotkeys.windows_key_used_as_chord = 0;
        }

        reach_helper_update_modifier_state(event->key, 1);
        reach_helper_append_hotkey(event->key, REACH_ELEVATION_HELPER_HOTKEY_PRESSED,
                                   reach_helper_hotkey_modifiers());
        return REACH_HELPER_KEY_PASS;
    }

    if (event->up)
    {
        uint32_t modifiers = reach_helper_hotkey_modifiers();
        if (g_hotkeys.windows_key_used_as_chord)
        {
            modifiers |= REACH_ELEVATION_HELPER_MODIFIER_CHORD;
        }

        reach_helper_append_hotkey(event->key, REACH_ELEVATION_HELPER_HOTKEY_RELEASED, modifiers);

        reach_helper_update_modifier_state(event->key, 0);

        if (!reach_helper_windows_key_down())
        {
            g_hotkeys.windows_key_used_as_chord = 0;
        }

        return REACH_HELPER_KEY_PASS;
    }

    return REACH_HELPER_KEY_CONTINUE;
}

static reach_helper_key_decision
reach_helper_handle_owned_win_chord(const reach_helper_key_event *event)
{
    if (!reach_helper_is_owned_win_chord_key(event->key))
    {
        return REACH_HELPER_KEY_CONTINUE;
    }

    const int32_t captured = reach_helper_captured_win_chord_key_down(event->key);
    if (!reach_helper_windows_key_down() && !captured)
    {
        return REACH_HELPER_KEY_CONTINUE;
    }

    g_hotkeys.windows_key_used_as_chord = 1;

    if (event->down)
    {
        if (!captured)
        {
            reach_helper_set_captured_win_chord_key(event->key);
            reach_helper_append_hotkey(event->key, REACH_ELEVATION_HELPER_HOTKEY_PRESSED,
                                       reach_helper_hotkey_modifiers());
        }

        return REACH_HELPER_KEY_CONSUME;
    }

    if (event->up)
    {
        if (captured)
        {
            reach_helper_clear_captured_win_chord_key(event->key);
            reach_helper_append_hotkey(event->key, REACH_ELEVATION_HELPER_HOTKEY_RELEASED,
                                       reach_helper_hotkey_modifiers());
        }

        return REACH_HELPER_KEY_CONSUME;
    }

    return REACH_HELPER_KEY_CONTINUE;
}

static reach_helper_key_decision
reach_helper_handle_modifier_key(const reach_helper_key_event *event)
{
    if (!reach_helper_hotkey_is_modifier(event->key))
    {
        return REACH_HELPER_KEY_CONTINUE;
    }

    reach_helper_update_modifier_state(event->key, event->down ? 1 : 0);
    reach_helper_append_hotkey(event->key,
                               event->down ? REACH_ELEVATION_HELPER_HOTKEY_PRESSED
                                           : REACH_ELEVATION_HELPER_HOTKEY_RELEASED,
                               reach_helper_hotkey_modifiers());
    return REACH_HELPER_KEY_PASS;
}

static LRESULT CALLBACK reach_helper_keyboard_proc(int code, WPARAM wparam, LPARAM lparam)
{
    if (code != HC_ACTION)
    {
        return reach_helper_pass_key_event(code, wparam, lparam);
    }

    const KBDLLHOOKSTRUCT *keyboard = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lparam);
    if (keyboard == nullptr)
    {
        return reach_helper_pass_key_event(code, wparam, lparam);
    }

    if ((keyboard->flags & LLKHF_INJECTED) != 0)
    {
        return reach_helper_pass_key_event(code, wparam, lparam);
    }

    const int32_t key_down = reach_helper_is_key_down_message(wparam);
    const int32_t key_up = reach_helper_is_key_up_message(wparam);
    if (!key_down && !key_up)
    {
        return reach_helper_pass_key_event(code, wparam, lparam);
    }

    reach_helper_key_event event = {};
    event.vk = keyboard->vkCode;
    event.key = reach_helper_hotkey_key_from_vk(keyboard->vkCode);
    event.down = key_down;
    event.up = key_up;

    reach_helper_key_decision decision = reach_helper_handle_game_mode_key(&event);
    if (decision == REACH_HELPER_KEY_CONSUME)
    {
        return 1;
    }
    if (decision == REACH_HELPER_KEY_PASS)
    {
        return reach_helper_pass_key_event(code, wparam, lparam);
    }

    if (!reach_helper_hotkey_is_modifier(event.key))
    {
        reach_helper_reconcile_modifier_state();
    }

    decision = reach_helper_handle_alt_tab_key(&event);
    if (decision == REACH_HELPER_KEY_CONSUME)
    {
        return 1;
    }
    if (decision == REACH_HELPER_KEY_PASS)
    {
        return reach_helper_pass_key_event(code, wparam, lparam);
    }

    if (event.key == 0)
    {
        if (event.down && reach_helper_windows_key_down())
        {
            g_hotkeys.windows_key_used_as_chord = 1;
        }

        return reach_helper_pass_key_event(code, wparam, lparam);
    }

    decision = reach_helper_handle_windows_key(&event);
    if (decision == REACH_HELPER_KEY_CONSUME)
    {
        return 1;
    }
    if (decision == REACH_HELPER_KEY_PASS)
    {
        return reach_helper_pass_key_event(code, wparam, lparam);
    }

    if (event.down && reach_helper_windows_key_down())
    {
        g_hotkeys.windows_key_used_as_chord = 1;
    }

    decision = reach_helper_handle_owned_win_chord(&event);
    if (decision == REACH_HELPER_KEY_CONSUME)
    {
        return 1;
    }
    if (decision == REACH_HELPER_KEY_PASS)
    {
        return reach_helper_pass_key_event(code, wparam, lparam);
    }

    decision = reach_helper_handle_modifier_key(&event);
    if (decision == REACH_HELPER_KEY_CONSUME)
    {
        return 1;
    }
    if (decision == REACH_HELPER_KEY_PASS)
    {
        return reach_helper_pass_key_event(code, wparam, lparam);
    }

    reach_helper_append_hotkey(event.key,
                               event.down ? REACH_ELEVATION_HELPER_HOTKEY_PRESSED
                                          : REACH_ELEVATION_HELPER_HOTKEY_RELEASED,
                               reach_helper_hotkey_modifiers());

    return reach_helper_pass_key_event(code, wparam, lparam);
}

static DWORD WINAPI reach_helper_hotkey_thread(void *param)
{
    (void)param;

    MSG message = {};
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

    g_hotkeys.thread_id = GetCurrentThreadId();
    g_hotkeys.hook =
        SetWindowsHookExW(WH_KEYBOARD_LL, reach_helper_keyboard_proc, GetModuleHandleW(nullptr), 0);

    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
    }

    if (g_hotkeys.hook != nullptr)
    {
        UnhookWindowsHookEx(g_hotkeys.hook);
        g_hotkeys.hook = nullptr;
    }

    return 0;
}

reach_result reach_helper_start_hotkeys(void)
{
    if (g_hotkeys.thread != nullptr)
    {
        return REACH_OK;
    }

    g_hotkeys.thread = CreateThread(nullptr, 0, reach_helper_hotkey_thread, nullptr, 0, nullptr);
    for (int attempt = 0; g_hotkeys.thread != nullptr &&
                          (g_hotkeys.thread_id == 0 || g_hotkeys.hook == nullptr) && attempt < 20;
         ++attempt)
    {
        Sleep(10);
    }

    if (g_hotkeys.thread == nullptr || g_hotkeys.hook == nullptr)
    {
        reach_helper_stop_hotkeys();
        return REACH_ERROR;
    }

    return REACH_OK;
}

void reach_helper_stop_hotkeys(void)
{
    if (g_hotkeys.thread != nullptr)
    {
        if (g_hotkeys.thread_id != 0)
        {
            PostThreadMessageW(g_hotkeys.thread_id, WM_QUIT, 0, 0);
        }

        WaitForSingleObject(g_hotkeys.thread, 1000);
        CloseHandle(g_hotkeys.thread);
        g_hotkeys.thread = nullptr;
    }

    g_hotkeys = {};
}
