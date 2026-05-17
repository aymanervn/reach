#include "reach/core/ui_state.h"

static size_t reach_core_strlen_utf16(const uint16_t *text)
{
    size_t length = 0;
    if (text == 0) {
        return 0;
    }
    while (text[length] != 0) {
        ++length;
    }
    return length;
}

void reach_ui_state_init(reach_ui_state *state)
{
    if (state == 0) {
        return;
    }

    state->dock.height = 64.0f;
    state->dock.width = 560.0f;
    state->dock.icon_size = 40.0f;
    state->dock.gap = 12.0f;
    state->dock.visible = 1;
    state->dock.auto_hide = 1;
    state->launcher.open = 0;
    state->launcher.query[0] = 0;
    state->launcher.query_length = 0;
    state->pinned_app_count = 0;
}

reach_result reach_ui_state_set_pinned_apps(reach_ui_state *state, const reach_pinned_app_model *apps, size_t count)
{
    if (state == 0 || (apps == 0 && count != 0)) {
        return REACH_INVALID_ARGUMENT;
    }

    if (count > REACH_MAX_PINNED_APPS) {
        count = REACH_MAX_PINNED_APPS;
    }

    for (size_t index = 0; index < count; ++index) {
        state->pinned_apps[index] = apps[index];
    }
    state->pinned_app_count = count;
    return REACH_OK;
}

reach_result reach_ui_state_open_launcher(reach_ui_state *state)
{
    if (state == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    state->launcher.open = 1;
    return REACH_OK;
}

reach_result reach_ui_state_close_launcher(reach_ui_state *state)
{
    if (state == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    state->launcher.open = 0;
    state->launcher.query[0] = 0;
    state->launcher.query_length = 0;
    return REACH_OK;
}

reach_result reach_ui_state_toggle_launcher(reach_ui_state *state)
{
    if (state == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    return state->launcher.open ? reach_ui_state_close_launcher(state) : reach_ui_state_open_launcher(state);
}

reach_result reach_ui_state_set_query(reach_ui_state *state, const uint16_t *query)
{
    if (state == 0 || query == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    size_t length = reach_core_strlen_utf16(query);
    if (length > REACH_MAX_SEARCH_CHARS) {
        length = REACH_MAX_SEARCH_CHARS;
    }

    for (size_t index = 0; index < length; ++index) {
        state->launcher.query[index] = query[index];
    }
    state->launcher.query[length] = 0;
    state->launcher.query_length = length;
    return REACH_OK;
}

int32_t reach_ui_state_should_show_pinned_apps(const reach_ui_state *state)
{
    return state != 0 && state->launcher.open && state->launcher.query_length == 0;
}

int32_t reach_ui_state_should_show_search_placeholder(const reach_ui_state *state)
{
    return state != 0 && state->launcher.open && state->launcher.query_length > 0;
}
