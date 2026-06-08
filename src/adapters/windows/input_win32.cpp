#include "windows_adapters_internal.h"

#include "window_management/elevation_helper_shared_state_win32.h"

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
    int32_t windows_d_down;
};

static const wchar_t *REACH_INPUT_WINDOW_CLASS = L"ReachInputMessageWindow";
static const UINT REACH_INPUT_WM_UI_EVENT = WM_APP + 21;
static const UINT REACH_INPUT_WM_SHARED_HOTKEYS = WM_APP + 23;
static const UINT REACH_INPUT_WM_HELPER_DISCONNECTED = WM_APP + 24;

static void reach_input_post_ui_event(reach_input_source *source, reach_ui_event_type type,
                                      uint32_t id)
{
    if (source != nullptr && source->window != nullptr)
    {
        PostMessageW(source->window, REACH_INPUT_WM_UI_EVENT, type, id);
    }
}

static void reach_input_reset_hotkey_state(reach_input_source *source)
{
    if (source == nullptr)
    {
        return;
    }
    if (source->alt_tab_active)
    {
        reach_input_post_ui_event(source, REACH_UI_EVENT_ALT_TAB_CANCEL, 0);
    }
    source->alt_down = 0;
    source->shift_down = 0;
    source->alt_tab_active = 0;
    source->windows_key_down = 0;
    source->windows_key_chord = 0;
    source->windows_d_down = 0;
}

static void reach_input_handle_hotkey_record(reach_input_source *source,
                                             const reach_elevation_helper_hotkey_record *record)
{
    if (source == nullptr || record == nullptr)
    {
        return;
    }

    int32_t pressed = record->action == REACH_ELEVATION_HELPER_HOTKEY_PRESSED;
    switch (record->key)
    {
    case REACH_ELEVATION_HELPER_HOTKEY_ALT:
        source->alt_down = pressed;
        if (!pressed && source->alt_tab_active)
        {
            source->alt_tab_active = 0;
            reach_input_post_ui_event(source, REACH_UI_EVENT_ALT_TAB_COMMIT, 0);
        }
        break;
    case REACH_ELEVATION_HELPER_HOTKEY_SHIFT:
        source->shift_down = pressed;
        break;
    case REACH_ELEVATION_HELPER_HOTKEY_TAB:
        if (pressed &&
            (source->alt_down ||
             (record->modifiers & REACH_ELEVATION_HELPER_MODIFIER_ALT) != 0))
        {
            if (!source->alt_tab_active)
            {
                reach_input_post_ui_event(source, REACH_UI_EVENT_ALT_TAB_BEGIN, 0);
                source->alt_tab_active = 1;
                break;
            }
            int32_t shift_down =
                source->shift_down ||
                ((record->modifiers & REACH_ELEVATION_HELPER_MODIFIER_SHIFT) != 0);
            reach_ui_event_type direction =
                shift_down ? REACH_UI_EVENT_ALT_TAB_PREVIOUS : REACH_UI_EVENT_ALT_TAB_NEXT;
            reach_input_post_ui_event(source, direction, 0);
        }
        break;
    case REACH_ELEVATION_HELPER_HOTKEY_ESCAPE:
        if (pressed && source->alt_tab_active)
        {
            source->alt_tab_active = 0;
            reach_input_post_ui_event(source, REACH_UI_EVENT_ALT_TAB_CANCEL, 0);
        }
        break;
    case REACH_ELEVATION_HELPER_HOTKEY_D:
        if (source->windows_key_down ||
            (record->modifiers & (REACH_ELEVATION_HELPER_MODIFIER_LEFT_WIN |
                                  REACH_ELEVATION_HELPER_MODIFIER_RIGHT_WIN)) != 0)
        {
            source->windows_key_chord = 1;
            if (pressed && !source->windows_d_down)
            {
                source->windows_d_down = 1;
                reach_input_post_ui_event(source, REACH_UI_EVENT_WINDOWS_D_MINIMIZE_ALL, 0);
            }
            else if (!pressed)
            {
                source->windows_d_down = 0;
            }
        }
        break;
    case REACH_ELEVATION_HELPER_HOTKEY_LEFT_WIN:
    case REACH_ELEVATION_HELPER_HOTKEY_RIGHT_WIN:
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
                            ((record->modifiers & REACH_ELEVATION_HELPER_MODIFIER_CHORD) != 0);
            source->windows_key_down = 0;
            source->windows_key_chord = 0;
            source->windows_d_down = 0;
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

    reach_elevation_helper_hotkey_record records[REACH_ELEVATION_HELPER_HOTKEY_QUEUE_CAPACITY] = {};
    uint32_t record_count = 0;
    int32_t missed = 0;
    uint64_t first_available = 0;
    uint64_t last_available = 0;
    if (reach_elevation_helper_shared_copy_hotkeys_since(
            source->last_hotkey_event, records, REACH_ELEVATION_HELPER_HOTKEY_QUEUE_CAPACITY,
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

static void reach_input_shared_callback(void *user, reach_elevation_helper_shared_reader_event event)
{
    reach_input_source *source = static_cast<reach_input_source *>(user);
    if (source == nullptr || source->window == nullptr)
    {
        return;
    }

    if (event == REACH_ELEVATION_HELPER_SHARED_EVENT_WINDOWS_CHANGED ||
        event == REACH_ELEVATION_HELPER_SHARED_EVENT_GAME_MODE_CHANGED)
    {
        reach_input_post_ui_event(source, REACH_UI_EVENT_WINDOW_STATE_CHANGED, 0);
    }
    else if (event == REACH_ELEVATION_HELPER_SHARED_EVENT_HOTKEYS_CHANGED)
    {
        PostMessageW(source->window, REACH_INPUT_WM_SHARED_HOTKEYS, 0, 0);
    }
    else if (event == REACH_ELEVATION_HELPER_SHARED_EVENT_DISCONNECTED)
    {
        PostMessageW(source->window, REACH_INPUT_WM_HELPER_DISCONNECTED, 0, 0);
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
    if (message == REACH_INPUT_WM_HELPER_DISCONNECTED && source != nullptr)
    {
        reach_input_reset_hotkey_state(source);
        return 0;
    }
    if (message == REACH_INPUT_WM_SHARED_HOTKEYS && source != nullptr)
    {
        reach_input_process_shared_hotkeys(source);
        return 0;
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

    (void)reach_elevation_helper_shared_reader_subscribe(reach_input_shared_callback, source);
    return REACH_OK;
}

static reach_result reach_input_stop(reach_input_source *source)
{
    REACH_ASSERT(source != nullptr);
    if (source == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_elevation_helper_shared_reader_unsubscribe(source);
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
