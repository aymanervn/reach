#include "reach/core/ui_state.h"

static size_t reach_core_strlen_utf16(const uint16_t *text)
{
    size_t length = 0;
    if (text == 0)
    {
        return 0;
    }
    while (text[length] != 0)
    {
        ++length;
    }
    return length;
}

static int32_t reach_core_is_word_space(uint16_t ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static void reach_ui_state_clamp_launcher_caret(reach_ui_state *state)
{
    if (state != 0 && state->launcher.caret_index > state->launcher.query_length)
    {
        state->launcher.caret_index = state->launcher.query_length;
    }
}

void reach_ui_state_init(reach_ui_state *state)
{
    if (state == 0)
    {
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
    state->launcher.caret_index = 0;
    state->launcher.result_count = 0;
    state->launcher.selected_result_index = 0;
    state->pinned_app_count = 0;
}

reach_result reach_ui_state_set_pinned_apps(reach_ui_state *state,
                                            const reach_pinned_app_model *apps, size_t count)
{
    if (state == 0 || (apps == 0 && count != 0))
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (count > REACH_MAX_PINNED_APPS)
    {
        count = REACH_MAX_PINNED_APPS;
    }

    for (size_t index = 0; index < count; ++index)
    {
        state->pinned_apps[index] = apps[index];
    }
    state->pinned_app_count = count;
    return REACH_OK;
}

reach_result reach_ui_state_open_launcher(reach_ui_state *state)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    state->launcher.open = 1;
    return REACH_OK;
}

reach_result reach_ui_state_close_launcher(reach_ui_state *state)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    state->launcher.open = 0;
    state->launcher.query[0] = 0;
    state->launcher.query_length = 0;
    state->launcher.caret_index = 0;
    reach_ui_state_clear_launcher_results(state);
    return REACH_OK;
}

reach_result reach_ui_state_toggle_launcher(reach_ui_state *state)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    return state->launcher.open ? reach_ui_state_close_launcher(state)
                                : reach_ui_state_open_launcher(state);
}

reach_result reach_ui_state_set_query(reach_ui_state *state, const uint16_t *query)
{
    if (state == 0 || query == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    size_t length = reach_core_strlen_utf16(query);
    if (length > REACH_MAX_SEARCH_CHARS)
    {
        length = REACH_MAX_SEARCH_CHARS;
    }

    for (size_t index = 0; index < length; ++index)
    {
        state->launcher.query[index] = query[index];
    }
    state->launcher.query[length] = 0;
    state->launcher.query_length = length;
    state->launcher.caret_index = length;
    return REACH_OK;
}

reach_result reach_ui_state_insert_launcher_text(reach_ui_state *state, const uint16_t *text)
{
    if (state == 0 || text == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_ui_state_clamp_launcher_caret(state);
    size_t insert_length = reach_core_strlen_utf16(text);
    if (insert_length == 0)
    {
        return REACH_OK;
    }
    if (insert_length > REACH_MAX_SEARCH_CHARS - state->launcher.query_length)
    {
        insert_length = REACH_MAX_SEARCH_CHARS - state->launcher.query_length;
    }
    if (insert_length == 0)
    {
        return REACH_OK;
    }

    size_t caret = state->launcher.caret_index;
    for (size_t index = state->launcher.query_length + 1; index > caret; --index)
    {
        state->launcher.query[index + insert_length - 1] = state->launcher.query[index - 1];
    }
    for (size_t index = 0; index < insert_length; ++index)
    {
        state->launcher.query[caret + index] = text[index];
    }
    state->launcher.query_length += insert_length;
    state->launcher.caret_index = caret + insert_length;
    state->launcher.query[state->launcher.query_length] = 0;
    return REACH_OK;
}

static reach_result reach_ui_state_delete_launcher_range(reach_ui_state *state, size_t start,
                                                         size_t end)
{
    if (state == 0 || start > end || end > state->launcher.query_length)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (start == end)
    {
        state->launcher.caret_index = start;
        return REACH_OK;
    }

    size_t removed = end - start;
    for (size_t index = start; index + removed <= state->launcher.query_length; ++index)
    {
        state->launcher.query[index] = state->launcher.query[index + removed];
    }
    state->launcher.query_length -= removed;
    state->launcher.caret_index = start;
    state->launcher.query[state->launcher.query_length] = 0;
    reach_ui_state_clamp_launcher_caret(state);
    return REACH_OK;
}

reach_result reach_ui_state_delete_launcher_previous_character(reach_ui_state *state)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_ui_state_clamp_launcher_caret(state);
    if (state->launcher.caret_index == 0)
    {
        return REACH_OK;
    }
    return reach_ui_state_delete_launcher_range(state, state->launcher.caret_index - 1,
                                                state->launcher.caret_index);
}

reach_result reach_ui_state_delete_launcher_next_character(reach_ui_state *state)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_ui_state_clamp_launcher_caret(state);
    if (state->launcher.caret_index >= state->launcher.query_length)
    {
        return REACH_OK;
    }
    return reach_ui_state_delete_launcher_range(state, state->launcher.caret_index,
                                                state->launcher.caret_index + 1);
}

reach_result reach_ui_state_delete_launcher_previous_word(reach_ui_state *state)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_ui_state_clamp_launcher_caret(state);
    if (state->launcher.caret_index == 0)
    {
        return REACH_OK;
    }

    size_t end = state->launcher.caret_index;
    size_t start = end;
    while (start > 0 && reach_core_is_word_space(state->launcher.query[start - 1]))
    {
        --start;
    }
    while (start > 0 && !reach_core_is_word_space(state->launcher.query[start - 1]))
    {
        --start;
    }
    while (start > 1 && reach_core_is_word_space(state->launcher.query[start - 1]) &&
           reach_core_is_word_space(state->launcher.query[start - 2]))
    {
        --start;
    }
    return reach_ui_state_delete_launcher_range(state, start, end);
}

reach_result reach_ui_state_move_launcher_caret_left(reach_ui_state *state, int32_t by_word)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_ui_state_clamp_launcher_caret(state);
    if (state->launcher.caret_index == 0)
    {
        return REACH_OK;
    }
    if (!by_word)
    {
        state->launcher.caret_index -= 1;
        return REACH_OK;
    }
    while (state->launcher.caret_index > 0 &&
           reach_core_is_word_space(state->launcher.query[state->launcher.caret_index - 1]))
    {
        state->launcher.caret_index -= 1;
    }
    while (state->launcher.caret_index > 0 &&
           !reach_core_is_word_space(state->launcher.query[state->launcher.caret_index - 1]))
    {
        state->launcher.caret_index -= 1;
    }
    return REACH_OK;
}

reach_result reach_ui_state_move_launcher_caret_right(reach_ui_state *state, int32_t by_word)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_ui_state_clamp_launcher_caret(state);
    if (state->launcher.caret_index >= state->launcher.query_length)
    {
        return REACH_OK;
    }
    if (!by_word)
    {
        state->launcher.caret_index += 1;
        return REACH_OK;
    }
    while (state->launcher.caret_index < state->launcher.query_length &&
           !reach_core_is_word_space(state->launcher.query[state->launcher.caret_index]))
    {
        state->launcher.caret_index += 1;
    }
    while (state->launcher.caret_index < state->launcher.query_length &&
           reach_core_is_word_space(state->launcher.query[state->launcher.caret_index]))
    {
        state->launcher.caret_index += 1;
    }
    return REACH_OK;
}

reach_result reach_ui_state_move_launcher_caret_home(reach_ui_state *state)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    state->launcher.caret_index = 0;
    return REACH_OK;
}

reach_result reach_ui_state_move_launcher_caret_end(reach_ui_state *state)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    state->launcher.caret_index = state->launcher.query_length;
    return REACH_OK;
}

reach_result reach_ui_state_clear_launcher_results(reach_ui_state *state)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < REACH_SEARCH_MAX_RESULTS; ++index)
    {
        state->launcher.results[index] = (reach_search_candidate){0};
        state->launcher.result_icon_ids[index] = 0;
    }
    state->launcher.result_count = 0;
    state->launcher.selected_result_index = 0;
    return REACH_OK;
}

reach_result reach_ui_state_set_launcher_results(reach_ui_state *state,
                                                 const reach_search_candidate *results,
                                                 size_t count)
{
    if (state == 0 || (results == 0 && count != 0))
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (count > REACH_SEARCH_MAX_RESULTS)
    {
        count = REACH_SEARCH_MAX_RESULTS;
    }

    (void)reach_ui_state_clear_launcher_results(state);
    for (size_t index = 0; index < count; ++index)
    {
        state->launcher.results[index] = results[index];
    }
    state->launcher.result_count = count;
    state->launcher.selected_result_index = 0;
    return REACH_OK;
}

reach_result reach_ui_state_set_launcher_result_icon(reach_ui_state *state, size_t index,
                                                     uint64_t icon_id)
{
    if (state == 0 || index >= REACH_SEARCH_MAX_RESULTS)
    {
        return REACH_INVALID_ARGUMENT;
    }

    state->launcher.result_icon_ids[index] = icon_id;
    return REACH_OK;
}

reach_result reach_ui_state_select_next_launcher_result(reach_ui_state *state)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (state->launcher.result_count == 0)
    {
        state->launcher.selected_result_index = 0;
        return REACH_OK;
    }
    if (state->launcher.selected_result_index + 1 < state->launcher.result_count)
    {
        state->launcher.selected_result_index += 1;
    }
    return REACH_OK;
}

reach_result reach_ui_state_select_previous_launcher_result(reach_ui_state *state)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (state->launcher.result_count == 0 || state->launcher.selected_result_index == 0)
    {
        state->launcher.selected_result_index = 0;
        return REACH_OK;
    }
    state->launcher.selected_result_index -= 1;
    return REACH_OK;
}

int32_t reach_ui_state_should_show_pinned_apps(const reach_ui_state *state)
{
    (void)state;
    return 0;
}

int32_t reach_ui_state_should_show_search_placeholder(const reach_ui_state *state)
{
    return state != 0 && state->launcher.open && state->launcher.query_length > 0;
}
