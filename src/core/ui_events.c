#include "reach/core/ui_events.h"

static void reach_ui_copy_query(uint16_t *dst, const reach_ui_state *state)
{
    if (dst == 0 || state == 0)
    {
        return;
    }
    for (size_t index = 0; index <= state->launcher.query_length && index <= REACH_MAX_SEARCH_CHARS;
         ++index)
    {
        dst[index] = state->launcher.query[index];
    }
}

static int32_t reach_ui_query_changed(const uint16_t *old_query, const reach_ui_state *state)
{
    if (old_query == 0 || state == 0)
    {
        return 0;
    }
    for (size_t index = 0; index <= REACH_MAX_SEARCH_CHARS; ++index)
    {
        if (old_query[index] != state->launcher.query[index])
        {
            return 1;
        }
        if (old_query[index] == 0)
        {
            return 0;
        }
    }
    return 0;
}

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
    case REACH_UI_EVENT_TEXT:
        if (!state->launcher.open)
        {
            return REACH_OK;
        }
        {
            uint16_t old_query[REACH_MAX_SEARCH_CHARS + 1] = {0};
            reach_ui_copy_query(old_query, state);
            if (reach_ui_state_insert_launcher_text(state, event->text) != REACH_OK)
            {
                return REACH_ERROR;
            }
            if (out_intent != 0 && reach_ui_query_changed(old_query, state))
            {
                out_intent->type = REACH_UI_INTENT_RUN_SEARCH;
            }
        }
        return REACH_OK;
    case REACH_UI_EVENT_BACKSPACE:
        if (!state->launcher.open)
        {
            return REACH_OK;
        }
        {
            uint16_t old_query[REACH_MAX_SEARCH_CHARS + 1] = {0};
            reach_ui_copy_query(old_query, state);
            if ((event->modifiers & REACH_UI_EVENT_MODIFIER_CTRL) != 0)
            {
                (void)reach_ui_state_delete_launcher_previous_word(state);
            }
            else
            {
                (void)reach_ui_state_delete_launcher_previous_character(state);
            }
            if (out_intent != 0 && reach_ui_query_changed(old_query, state))
            {
                out_intent->type = REACH_UI_INTENT_RUN_SEARCH;
            }
        }
        return REACH_OK;
    case REACH_UI_EVENT_DELETE:
        if (!state->launcher.open)
        {
            return REACH_OK;
        }
        {
            uint16_t old_query[REACH_MAX_SEARCH_CHARS + 1] = {0};
            reach_ui_copy_query(old_query, state);
            (void)reach_ui_state_delete_launcher_next_character(state);
            if (out_intent != 0 && reach_ui_query_changed(old_query, state))
            {
                out_intent->type = REACH_UI_INTENT_RUN_SEARCH;
            }
        }
        return REACH_OK;
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
    case REACH_UI_EVENT_ARROW_LEFT:
        return state->launcher.open
                   ? reach_ui_state_move_launcher_caret_left(
                         state, (event->modifiers & REACH_UI_EVENT_MODIFIER_CTRL) != 0)
                   : REACH_OK;
    case REACH_UI_EVENT_ARROW_RIGHT:
        return state->launcher.open
                   ? reach_ui_state_move_launcher_caret_right(
                         state, (event->modifiers & REACH_UI_EVENT_MODIFIER_CTRL) != 0)
                   : REACH_OK;
    case REACH_UI_EVENT_HOME:
        return state->launcher.open ? reach_ui_state_move_launcher_caret_home(state) : REACH_OK;
    case REACH_UI_EVENT_END:
        return state->launcher.open ? reach_ui_state_move_launcher_caret_end(state) : REACH_OK;
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
