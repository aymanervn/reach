#include "reach/core/ui_events.h"

reach_result reach_ui_handle_event(reach_ui_state *state, const reach_ui_event *event, reach_ui_intent *out_intent)
{
    if (state == 0 || event == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    if (out_intent != 0) {
        out_intent->type = REACH_UI_INTENT_NONE;
        out_intent->id = 0;
    }

    switch (event->type) {
    case REACH_UI_EVENT_WINDOWS_KEY:
        return reach_ui_state_toggle_launcher(state);
    case REACH_UI_EVENT_ESCAPE:
        if (out_intent != 0 && state->launcher.open) {
            out_intent->type = REACH_UI_INTENT_CLOSE_LAUNCHER;
        }
        return reach_ui_state_close_launcher(state);
    case REACH_UI_EVENT_TEXT:
        if (!state->launcher.open) {
            return REACH_OK;
        }
        if (out_intent != 0) {
            out_intent->type = REACH_UI_INTENT_RUN_SEARCH;
        }
        {
            uint16_t merged[REACH_MAX_SEARCH_CHARS + 1];
            size_t index = 0;
            while (index < state->launcher.query_length && index < REACH_MAX_SEARCH_CHARS) {
                merged[index] = state->launcher.query[index];
                ++index;
            }
            for (size_t text_index = 0; event->text[text_index] != 0 && index < REACH_MAX_SEARCH_CHARS; ++text_index) {
                merged[index++] = event->text[text_index];
            }
            merged[index] = 0;
            return reach_ui_state_set_query(state, merged);
        }
    case REACH_UI_EVENT_BACKSPACE:
        if (state->launcher.query_length > 0) {
            state->launcher.query_length -= 1;
            state->launcher.query[state->launcher.query_length] = 0;
        }
        if (out_intent != 0 && state->launcher.query_length > 0) {
            out_intent->type = REACH_UI_INTENT_RUN_SEARCH;
        }
        return REACH_OK;
    case REACH_UI_EVENT_DOCK_APP_CLICK:
        if (out_intent != 0) {
            out_intent->type = REACH_UI_INTENT_LAUNCH_APP;
            out_intent->id = event->id;
        }
        return REACH_OK;
    case REACH_UI_EVENT_TRAY_BUTTON_CLICK:
        if (out_intent != 0) {
            out_intent->type = REACH_UI_INTENT_OPEN_TRAY_MENU;
        }
        return REACH_OK;
    case REACH_UI_EVENT_NONE:
    default:
        return REACH_OK;
    }
}
