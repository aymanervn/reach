#include "reach/core/ui_events.h"

reach_result reach_ui_handle_event(reach_ui_state *state, const reach_ui_event *event,
                                   reach_ui_intent *out_intent)
{
    if (state == 0 || event == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (out_intent != 0)
    {
        out_intent->type = REACH_UI_INTENT_NONE;
        out_intent->id = 0;
    }

    switch (event->type)
    {
    case REACH_UI_EVENT_WINDOWS_KEY:
        return reach_ui_state_toggle_launcher(state);
    case REACH_UI_EVENT_ESCAPE:
        if (out_intent != 0 && state->launcher.open)
        {
            out_intent->type = REACH_UI_INTENT_CLOSE_LAUNCHER;
        }
        return reach_ui_state_close_launcher(state);
    case REACH_UI_EVENT_ENTER:
        if (out_intent != 0 && state->launcher.open)
        {
            out_intent->type = REACH_UI_INTENT_OPEN_LAUNCHER_RESULT;
        }
        return REACH_OK;
    case REACH_UI_EVENT_ARROW_UP:
        return state->launcher.open ? reach_ui_state_select_previous_launcher_result(state)
                                    : REACH_OK;
    case REACH_UI_EVENT_ARROW_DOWN:
        return state->launcher.open ? reach_ui_state_select_next_launcher_result(state) : REACH_OK;
    case REACH_UI_EVENT_DOCK_APP_CLICK:
        if (out_intent != 0)
        {
            out_intent->type = REACH_UI_INTENT_LAUNCH_APP;
            out_intent->id = event->id;
        }
        return REACH_OK;
    case REACH_UI_EVENT_TRAY_BUTTON_CLICK:
        if (out_intent != 0)
        {
            out_intent->type = REACH_UI_INTENT_OPEN_TRAY_MENU;
        }
        return REACH_OK;
    case REACH_UI_EVENT_POINTER_WHEEL:
        return REACH_OK;
    case REACH_UI_EVENT_NONE:
    default:
        return REACH_OK;
    }
}
