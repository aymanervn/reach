#include "windows_adapters_internal.h"

#include "window_management/reach_service_shared_state_win32.h"

#include "reach/ports/input_source.h"

#include <windows.h>
#include <new>

struct reach_input_source
{
    reach_input_event_callback callback;
    void *user;
    HWND window;
    ATOM window_class;
    uint64_t last_hotkey_event;
    int32_t alt_down;
    int32_t shift_down;
    int32_t alt_tab_active;
    int32_t windows_key_down;
    int32_t windows_key_chord;
    uint32_t registered_media_hotkeys;
};

static const wchar_t *REACH_INPUT_WINDOW_CLASS = L"ReachInputMessageWindow";
static const UINT REACH_INPUT_WM_UI_EVENT = WM_APP + 21;
static const UINT REACH_INPUT_WM_SHARED_HOTKEYS = WM_APP + 23;
static const UINT REACH_INPUT_WM_HELPER_CONNECTED = WM_APP + 24;
static const int REACH_INPUT_HOTKEY_MEDIA_PREVIOUS = 1;
static const int REACH_INPUT_HOTKEY_MEDIA_PLAY_PAUSE = 2;
static const int REACH_INPUT_HOTKEY_MEDIA_NEXT = 3;
static const int REACH_INPUT_HOTKEY_VOLUME_UP = 4;
static const int REACH_INPUT_HOTKEY_VOLUME_DOWN = 5;
static const int REACH_INPUT_HOTKEY_VOLUME_MUTE = 6;
static const int REACH_INPUT_HOTKEY_BRIGHTNESS_UP = 7;
static const int REACH_INPUT_HOTKEY_BRIGHTNESS_DOWN = 8;

static void reach_input_reset_hotkey_state(reach_input_source *source);

static uint32_t reach_input_media_hotkey_mask(int hotkey_id)
{
    return 1u << (uint32_t)hotkey_id;
}

static void reach_input_reset_helper_hotkey_state(reach_input_source *source)
{
    if (source == nullptr)
    {
        return;
    }

    reach_input_reset_hotkey_state(source);
    source->last_hotkey_event = 0;
}

static UINT reach_input_media_hotkey_vk(int hotkey_id)
{
    switch (hotkey_id)
    {
    case REACH_INPUT_HOTKEY_MEDIA_PREVIOUS:
        return VK_MEDIA_PREV_TRACK;
    case REACH_INPUT_HOTKEY_MEDIA_PLAY_PAUSE:
        return VK_MEDIA_PLAY_PAUSE;
    case REACH_INPUT_HOTKEY_MEDIA_NEXT:
        return VK_MEDIA_NEXT_TRACK;
    case REACH_INPUT_HOTKEY_VOLUME_UP:
        return VK_VOLUME_UP;
    case REACH_INPUT_HOTKEY_VOLUME_DOWN:
        return VK_VOLUME_DOWN;
    case REACH_INPUT_HOTKEY_VOLUME_MUTE:
        return VK_VOLUME_MUTE;
    case REACH_INPUT_HOTKEY_BRIGHTNESS_UP:
#ifdef VK_BRIGHTNESS_UP
        return VK_BRIGHTNESS_UP;
#else
        return 0;
#endif
    case REACH_INPUT_HOTKEY_BRIGHTNESS_DOWN:
#ifdef VK_BRIGHTNESS_DOWN
        return VK_BRIGHTNESS_DOWN;
#else
        return 0;
#endif
    default:
        return 0;
    }
}

static void reach_input_register_media_hotkey(reach_input_source *source, int hotkey_id)
{
    if (source == nullptr || source->window == nullptr)
    {
        return;
    }

    uint32_t mask = reach_input_media_hotkey_mask(hotkey_id);
    if ((source->registered_media_hotkeys & mask) != 0)
    {
        return;
    }

    UINT vk = reach_input_media_hotkey_vk(hotkey_id);
    if (vk != 0 && RegisterHotKey(source->window, hotkey_id, 0, vk))
    {
        source->registered_media_hotkeys |= mask;
    }
}

static void reach_input_register_media_hotkeys(reach_input_source *source)
{
    reach_input_register_media_hotkey(source, REACH_INPUT_HOTKEY_MEDIA_PREVIOUS);
    reach_input_register_media_hotkey(source, REACH_INPUT_HOTKEY_MEDIA_PLAY_PAUSE);
    reach_input_register_media_hotkey(source, REACH_INPUT_HOTKEY_MEDIA_NEXT);
    reach_input_register_media_hotkey(source, REACH_INPUT_HOTKEY_VOLUME_UP);
    reach_input_register_media_hotkey(source, REACH_INPUT_HOTKEY_VOLUME_DOWN);
    reach_input_register_media_hotkey(source, REACH_INPUT_HOTKEY_VOLUME_MUTE);
    reach_input_register_media_hotkey(source, REACH_INPUT_HOTKEY_BRIGHTNESS_UP);
    reach_input_register_media_hotkey(source, REACH_INPUT_HOTKEY_BRIGHTNESS_DOWN);
}

static void reach_input_unregister_media_hotkey(reach_input_source *source, int hotkey_id)
{
    if (source == nullptr || source->window == nullptr)
    {
        return;
    }

    uint32_t mask = reach_input_media_hotkey_mask(hotkey_id);
    if ((source->registered_media_hotkeys & mask) == 0)
    {
        return;
    }

    (void)UnregisterHotKey(source->window, hotkey_id);
    source->registered_media_hotkeys &= ~mask;
}

static void reach_input_unregister_media_hotkeys(reach_input_source *source)
{
    if (source == nullptr || source->window == nullptr)
    {
        return;
    }

    for (int hotkey_id = REACH_INPUT_HOTKEY_MEDIA_PREVIOUS;
         hotkey_id <= REACH_INPUT_HOTKEY_BRIGHTNESS_DOWN; ++hotkey_id)
    {
        reach_input_unregister_media_hotkey(source, hotkey_id);
    }
}

static reach_ui_event_type reach_input_media_hotkey_event(WPARAM hotkey_id)
{
    switch ((int)hotkey_id)
    {
    case REACH_INPUT_HOTKEY_MEDIA_PREVIOUS:
        return REACH_UI_EVENT_MEDIA_PREVIOUS;
    case REACH_INPUT_HOTKEY_MEDIA_PLAY_PAUSE:
        return REACH_UI_EVENT_MEDIA_PLAY_PAUSE;
    case REACH_INPUT_HOTKEY_MEDIA_NEXT:
        return REACH_UI_EVENT_MEDIA_NEXT;
    case REACH_INPUT_HOTKEY_VOLUME_UP:
        return REACH_UI_EVENT_VOLUME_UP;
    case REACH_INPUT_HOTKEY_VOLUME_DOWN:
        return REACH_UI_EVENT_VOLUME_DOWN;
    case REACH_INPUT_HOTKEY_VOLUME_MUTE:
        return REACH_UI_EVENT_VOLUME_MUTE;
    case REACH_INPUT_HOTKEY_BRIGHTNESS_UP:
        return REACH_UI_EVENT_BRIGHTNESS_UP;
    case REACH_INPUT_HOTKEY_BRIGHTNESS_DOWN:
        return REACH_UI_EVENT_BRIGHTNESS_DOWN;
    default:
        return REACH_UI_EVENT_NONE;
    }
}

static void reach_input_post_ui_event(reach_input_source *source, reach_ui_event_type type,
                                      uint32_t id)
{
    if (source != nullptr && source->window != nullptr)
    {
        PostMessageW(source->window, REACH_INPUT_WM_UI_EVENT, type, id);
    }
}

typedef struct reach_input_win_chord_event
{
    uint32_t key;
    reach_ui_event_type type;
} reach_input_win_chord_event;

static const reach_input_win_chord_event REACH_INPUT_WIN_CHORD_EVENTS[] = {
    {REACH_SERVICE_HOTKEY_D, REACH_UI_EVENT_MINIMIZE_ALL},
    {REACH_SERVICE_HOTKEY_T, REACH_UI_EVENT_OPEN_TERMINAL},
    {REACH_SERVICE_HOTKEY_ARROW_LEFT, REACH_UI_EVENT_SNAP_LEFT},
    {REACH_SERVICE_HOTKEY_ARROW_RIGHT, REACH_UI_EVENT_SNAP_RIGHT},
    {REACH_SERVICE_HOTKEY_ARROW_UP, REACH_UI_EVENT_SNAP_TOP},
    {REACH_SERVICE_HOTKEY_ARROW_DOWN, REACH_UI_EVENT_SNAP_BOTTOM},
};

static int32_t
reach_input_hotkey_has_win_modifier(const reach_service_hotkey_record *record)
{
    return record != nullptr &&
           (record->modifiers & (REACH_SERVICE_MODIFIER_LEFT_WIN |
                                 REACH_SERVICE_MODIFIER_RIGHT_WIN)) != 0;
}

static size_t reach_input_win_chord_event_count(void)
{
    return sizeof(REACH_INPUT_WIN_CHORD_EVENTS) / sizeof(REACH_INPUT_WIN_CHORD_EVENTS[0]);
}

static int32_t reach_input_try_post_win_chord_event(
    reach_input_source *source, const reach_service_hotkey_record *record, int32_t pressed)
{
    if (source == nullptr || record == nullptr || !pressed ||
        !reach_input_hotkey_has_win_modifier(record))
    {
        return 0;
    }

    for (size_t index = 0; index < reach_input_win_chord_event_count(); ++index)
    {
        const reach_input_win_chord_event *entry = &REACH_INPUT_WIN_CHORD_EVENTS[index];
        if (entry->key == record->key)
        {
            source->windows_key_chord = 1;
            reach_input_post_ui_event(source, entry->type, 0);
            return 1;
        }
    }

    return 0;
}

static void reach_input_reset_hotkey_state(reach_input_source *source)
{
    if (source == nullptr)
    {
        return;
    }
    if (source->alt_tab_active)
    {
        reach_input_post_ui_event(source, REACH_UI_EVENT_APP_SWITCH_CANCEL, 0);
    }
    source->alt_down = 0;
    source->shift_down = 0;
    source->alt_tab_active = 0;
    source->windows_key_down = 0;
    source->windows_key_chord = 0;
}

static void reach_input_handle_hotkey_record(reach_input_source *source,
                                             const reach_service_hotkey_record *record)
{
    if (source == nullptr || record == nullptr)
    {
        return;
    }

    int32_t pressed = record->action == REACH_SERVICE_HOTKEY_PRESSED;
    switch (record->key)
    {
    case REACH_SERVICE_HOTKEY_ALT:
        source->alt_down = pressed;
        if (!pressed && source->alt_tab_active)
        {
            source->alt_tab_active = 0;
            reach_input_post_ui_event(source, REACH_UI_EVENT_APP_SWITCH_COMMIT, 0);
        }
        break;
    case REACH_SERVICE_HOTKEY_SHIFT:
        source->shift_down = pressed;
        break;
    case REACH_SERVICE_HOTKEY_TAB:
        if (pressed &&
            (source->alt_down || (record->modifiers & REACH_SERVICE_MODIFIER_ALT) != 0))
        {
            if (!source->alt_tab_active)
            {
                reach_input_post_ui_event(source, REACH_UI_EVENT_APP_SWITCH_BEGIN, 0);
                source->alt_tab_active = 1;
                break;
            }
            int32_t shift_down = source->shift_down ||
                                 ((record->modifiers & REACH_SERVICE_MODIFIER_SHIFT) != 0);
            reach_ui_event_type direction =
                shift_down ? REACH_UI_EVENT_APP_SWITCH_PREVIOUS : REACH_UI_EVENT_APP_SWITCH_NEXT;
            reach_input_post_ui_event(source, direction, 0);
        }
        break;
    case REACH_SERVICE_HOTKEY_ESCAPE:
        if (pressed && source->alt_tab_active)
        {
            source->alt_tab_active = 0;
            reach_input_post_ui_event(source, REACH_UI_EVENT_APP_SWITCH_CANCEL, 0);
        }
        break;
    case REACH_SERVICE_HOTKEY_D:
    case REACH_SERVICE_HOTKEY_T:
    case REACH_SERVICE_HOTKEY_ARROW_LEFT:
    case REACH_SERVICE_HOTKEY_ARROW_RIGHT:
    case REACH_SERVICE_HOTKEY_ARROW_UP:
    case REACH_SERVICE_HOTKEY_ARROW_DOWN:
        if (!reach_input_try_post_win_chord_event(source, record, pressed) && !pressed &&
            reach_input_hotkey_has_win_modifier(record))
        {
            source->windows_key_chord = 1;
        }
        break;
    case REACH_SERVICE_HOTKEY_LEFT_WIN:
    case REACH_SERVICE_HOTKEY_RIGHT_WIN:
        if (source->alt_tab_active)
        {
            source->windows_key_down = 0;
            source->windows_key_chord = 0;
            break;
        }
        if (pressed)
        {
            source->windows_key_down = 1;
            source->windows_key_chord = 0;
        }
        else if (source->windows_key_down)
        {
            int32_t chord = source->windows_key_chord ||
                            ((record->modifiers & REACH_SERVICE_MODIFIER_CHORD) != 0);
            source->windows_key_down = 0;
            source->windows_key_chord = 0;
            if (!chord)
            {
                reach_input_post_ui_event(source, REACH_UI_EVENT_WINDOWS_KEY, 0);
            }
        }
        break;
    default:
        if (pressed && source->windows_key_down)
        {
            source->windows_key_chord = 1;
        }
        break;
    }
}

static void reach_input_process_shared_hotkeys(reach_input_source *source)
{
    if (source == nullptr)
    {
        return;
    }

    reach_service_hotkey_record records[REACH_SERVICE_HOTKEY_QUEUE_CAPACITY] = {};
    uint32_t record_count = 0;
    int32_t missed = 0;
    uint64_t first_available = 0;
    uint64_t last_available = 0;
    if (reach_service_shared_copy_hotkeys_since(
            source->last_hotkey_event, records, REACH_SERVICE_HOTKEY_QUEUE_CAPACITY,
            &record_count, &missed, &first_available, &last_available) != REACH_OK)
    {
        return;
    }

    if (missed)
    {
        reach_input_reset_hotkey_state(source);
        source->last_hotkey_event = first_available > 0 ? first_available - 1 : 0;
    }

    for (uint32_t index = 0; index < record_count; ++index)
    {
        reach_input_handle_hotkey_record(source, &records[index]);
        source->last_hotkey_event = records[index].event_number;
    }
}

static void reach_input_shared_callback(void *user,
                                        reach_service_shared_reader_event event)
{
    reach_input_source *source = static_cast<reach_input_source *>(user);
    if (source == nullptr || source->window == nullptr)
    {
        return;
    }

    if (event == REACH_SERVICE_SHARED_EVENT_CONNECTED)
    {
        PostMessageW(source->window, REACH_INPUT_WM_HELPER_CONNECTED, 0, 0);
        return;
    }

    if (event == REACH_SERVICE_SHARED_EVENT_WINDOWS_CHANGED ||
        event == REACH_SERVICE_SHARED_EVENT_GAME_MODE_CHANGED)
    {
        reach_input_post_ui_event(source, REACH_UI_EVENT_WINDOW_STATE_CHANGED, 0);
        return;
    }

    if (event == REACH_SERVICE_SHARED_EVENT_HOTKEYS_CHANGED)
    {
        PostMessageW(source->window, REACH_INPUT_WM_SHARED_HOTKEYS, 0, 0);
        return;
    }
}

static LRESULT CALLBACK reach_input_window_proc(HWND hwnd, UINT message, WPARAM wparam,
                                                LPARAM lparam)
{
    if (message == WM_NCCREATE)
    {
        CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return TRUE;
    }

    reach_input_source *source =
        reinterpret_cast<reach_input_source *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == REACH_INPUT_WM_UI_EVENT && source != nullptr && source->callback != nullptr)
    {
        reach_ui_event event = {};
        event.type = static_cast<reach_ui_event_type>(wparam);
        event.id = static_cast<uint32_t>(lparam);
        source->callback(source->user, &event);
        return 0;
    }
    if (message == REACH_INPUT_WM_HELPER_CONNECTED && source != nullptr)
    {
        reach_input_reset_helper_hotkey_state(source);
        return 0;
    }
    if (message == REACH_INPUT_WM_SHARED_HOTKEYS && source != nullptr)
    {
        reach_input_process_shared_hotkeys(source);
        return 0;
    }
    if (message == WM_HOTKEY && source != nullptr)
    {
        reach_ui_event_type type = reach_input_media_hotkey_event(wparam);
        if (type != REACH_UI_EVENT_NONE)
        {
            reach_input_post_ui_event(source, type, 0);
            return 0;
        }
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static reach_result reach_input_create_window(reach_input_source *source)
{
    REACH_ASSERT(source != nullptr);
    if (source == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = reach_input_window_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = REACH_INPUT_WINDOW_CLASS;
    source->window_class = RegisterClassExW(&wc);
    if (source->window_class == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return REACH_ERROR;
    }

    source->window = CreateWindowExW(0, REACH_INPUT_WINDOW_CLASS, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                                     nullptr, GetModuleHandleW(nullptr), source);
    return source->window != nullptr ? REACH_OK : REACH_ERROR;
}

static reach_result reach_input_start(reach_input_source *source,
                                      reach_input_event_callback callback, void *user)
{
    REACH_ASSERT(source != nullptr);
    REACH_ASSERT(callback != nullptr);
    if (source == nullptr || callback == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    source->callback = callback;
    source->user = user;
    if (source->window == nullptr && reach_input_create_window(source) != REACH_OK)
    {
        return REACH_ERROR;
    }

    (void)reach_service_shared_reader_subscribe(reach_input_shared_callback, source);
    reach_input_register_media_hotkeys(source);
    return REACH_OK;
}

static reach_result reach_input_stop(reach_input_source *source)
{
    REACH_ASSERT(source != nullptr);
    if (source == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_service_shared_reader_unsubscribe(source);
    reach_input_unregister_media_hotkeys(source);
    reach_input_reset_hotkey_state(source);
    source->callback = nullptr;
    source->user = nullptr;
    return REACH_OK;
}

static reach_result reach_input_get_pointer_position(reach_input_source *source,
                                                     reach_point_i32 *out_position)
{
    (void)source;
    if (out_position == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    POINT cursor = {};
    if (!GetCursorPos(&cursor))
    {
        return REACH_ERROR;
    }

    out_position->x = cursor.x;
    out_position->y = cursor.y;
    return REACH_OK;
}

static void reach_input_destroy(reach_input_source *source)
{
    if (source != nullptr)
    {
        (void)reach_input_stop(source);
        if (source->window != nullptr)
        {
            DestroyWindow(source->window);
            source->window = nullptr;
        }
    }
    delete source;
}

reach_result reach_windows_create_input_source(reach_input_source_port *out_port)
{
    REACH_ASSERT(out_port != nullptr);
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_input_source *source = new (std::nothrow) reach_input_source();
    if (source == nullptr)
    {
        return REACH_ERROR;
    }

    out_port->source = source;
    out_port->ops.start = reach_input_start;
    out_port->ops.stop = reach_input_stop;
    out_port->ops.get_pointer_position = reach_input_get_pointer_position;
    out_port->ops.destroy = reach_input_destroy;
    return REACH_OK;
}
