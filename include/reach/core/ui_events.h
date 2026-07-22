#ifndef REACH_CORE_UI_EVENTS_H
#define REACH_CORE_UI_EVENTS_H

#include "reach/core/ui_state.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_ui_event_type
    {
        REACH_UI_EVENT_NONE = 0,
        REACH_UI_EVENT_WINDOWS_KEY = 1,
        REACH_UI_EVENT_ESCAPE = 2,
        REACH_UI_EVENT_DOCK_APP_CLICK = 5,
        REACH_UI_EVENT_TRAY_BUTTON_CLICK = 6,
        REACH_UI_EVENT_POINTER_UP = 7,
        REACH_UI_EVENT_POINTER_CONTEXT = 8,
        REACH_UI_EVENT_WALLPAPER_CHANGED = 9,
        REACH_UI_EVENT_POINTER_MOVE = 10,
        REACH_UI_EVENT_POINTER_LEAVE = 11,
        REACH_UI_EVENT_POINTER_MIDDLE = 12,
        REACH_UI_EVENT_POINTER_DOWN = 13,
        REACH_UI_EVENT_APP_SWITCH_BEGIN = 14,
        REACH_UI_EVENT_APP_SWITCH_NEXT = 15,
        REACH_UI_EVENT_APP_SWITCH_PREVIOUS = 16,
        REACH_UI_EVENT_APP_SWITCH_COMMIT = 17,
        REACH_UI_EVENT_APP_SWITCH_CANCEL = 18,
        REACH_UI_EVENT_ENTER = 19,
        REACH_UI_EVENT_ARROW_UP = 20,
        REACH_UI_EVENT_ARROW_DOWN = 21,
        REACH_UI_EVENT_LAUNCHER_SEARCH_READY = 27,
        REACH_UI_EVENT_CONFIG_CHANGED = 28,
        REACH_UI_EVENT_DISPLAY_CHANGED = 29,
        REACH_UI_EVENT_WINDOW_STATE_CHANGED = 30,
        REACH_UI_EVENT_MINIMIZE_ALL = 31,
        REACH_UI_EVENT_POINTER_CANCEL = 32,
        REACH_UI_EVENT_MEDIA_PREVIOUS = 33,
        REACH_UI_EVENT_MEDIA_PLAY_PAUSE = 34,
        REACH_UI_EVENT_MEDIA_NEXT = 35,
        REACH_UI_EVENT_VOLUME_UP = 36,
        REACH_UI_EVENT_VOLUME_DOWN = 37,
        REACH_UI_EVENT_VOLUME_MUTE = 38,
        REACH_UI_EVENT_BRIGHTNESS_UP = 39,
        REACH_UI_EVENT_BRIGHTNESS_DOWN = 40,
        REACH_UI_EVENT_POINTER_WHEEL = 41,
        REACH_UI_EVENT_WINDOW_BOUNDS_CHANGED = 42,
        REACH_UI_EVENT_CLIPBOARD_CHANGED = 43,
        REACH_UI_EVENT_OPEN_TERMINAL = 44,

        REACH_UI_EVENT_TEXT_CHAR = 45,
        REACH_UI_EVENT_TEXT_EDIT = 46,
        REACH_UI_EVENT_NOW_PLAYING_CHANGED = 47,
        REACH_UI_EVENT_SNAP_LEFT = 48,
        REACH_UI_EVENT_SNAP_RIGHT = 49,
        REACH_UI_EVENT_SNAP_TOP = 50,
        REACH_UI_EVENT_SNAP_BOTTOM = 51,
        REACH_UI_EVENT_WINDOW_FOCUS_LOST = 52,
        REACH_UI_EVENT_FOREGROUND_CHANGED = 53
    } reach_ui_event_type;

    typedef enum reach_ui_edit_key
    {
        REACH_UI_EDIT_KEY_NONE = 0,
        REACH_UI_EDIT_KEY_BACKSPACE = 1,
        REACH_UI_EDIT_KEY_DELETE = 2,
        REACH_UI_EDIT_KEY_LEFT = 3,
        REACH_UI_EDIT_KEY_RIGHT = 4,
        REACH_UI_EDIT_KEY_HOME = 5,
        REACH_UI_EDIT_KEY_END = 6,
        REACH_UI_EDIT_KEY_SELECT_ALL = 7
    } reach_ui_edit_key;

#define REACH_UI_EVENT_MODIFIER_CTRL 0x1u
#define REACH_UI_EVENT_MODIFIER_SHIFT 0x2u

    typedef struct reach_ui_event
    {
        reach_ui_event_type type;
        uint32_t id;
        uint32_t modifiers;
        int32_t x;
        int32_t y;
        int32_t wheel_delta;
        uint16_t text[REACH_MAX_SEARCH_CHARS + 1];
    } reach_ui_event;

    typedef enum reach_ui_intent_type
    {
        REACH_UI_INTENT_NONE = 0,
        REACH_UI_INTENT_LAUNCH_APP = 1,
        REACH_UI_INTENT_RESERVED_2 = 2,
        REACH_UI_INTENT_RESERVED_3 = 3,
        REACH_UI_INTENT_OPEN_LAUNCHER_RESULT = 4
    } reach_ui_intent_type;

    typedef struct reach_ui_intent
    {
        reach_ui_intent_type type;
        uint32_t id;
    } reach_ui_intent;

#ifdef __cplusplus
}
#endif

#endif
