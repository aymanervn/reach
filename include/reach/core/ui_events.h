#ifndef REACH_CORE_UI_EVENTS_H
#define REACH_CORE_UI_EVENTS_H

#include "reach/core/ui_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum reach_ui_event_type {
    REACH_UI_EVENT_NONE = 0,
    REACH_UI_EVENT_WINDOWS_KEY = 1,
    REACH_UI_EVENT_ESCAPE = 2,
    REACH_UI_EVENT_TEXT = 3,
    REACH_UI_EVENT_BACKSPACE = 4,
    REACH_UI_EVENT_DOCK_APP_CLICK = 5,
    REACH_UI_EVENT_TRAY_BUTTON_CLICK = 6,
    REACH_UI_EVENT_POINTER_UP = 7,
    REACH_UI_EVENT_POINTER_CONTEXT = 8,
    REACH_UI_EVENT_WALLPAPER_CHANGED = 9
} reach_ui_event_type;

typedef struct reach_ui_event {
    reach_ui_event_type type;
    uint32_t id;
    int32_t x;
    int32_t y;
    uint16_t text[REACH_MAX_SEARCH_CHARS + 1];
} reach_ui_event;

typedef enum reach_ui_intent_type {
    REACH_UI_INTENT_NONE = 0,
    REACH_UI_INTENT_LAUNCH_APP = 1,
    REACH_UI_INTENT_OPEN_TRAY_MENU = 2,
    REACH_UI_INTENT_CLOSE_LAUNCHER = 3,
    REACH_UI_INTENT_RUN_SEARCH = 4
} reach_ui_intent_type;

typedef struct reach_ui_intent {
    reach_ui_intent_type type;
    uint32_t id;
} reach_ui_intent;

reach_result reach_ui_handle_event(reach_ui_state *state, const reach_ui_event *event, reach_ui_intent *out_intent);

#ifdef __cplusplus
}
#endif

#endif
