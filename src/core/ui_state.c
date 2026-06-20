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

static size_t reach_ui_state_launcher_visible_count(const reach_ui_state *state)
{
    if (state == 0)
    {
        return 0;
    }
    return state->launcher.result_count < REACH_SEARCH_VISIBLE_RESULTS
               ? state->launcher.result_count
               : REACH_SEARCH_VISIBLE_RESULTS;
}

static size_t reach_ui_state_launcher_max_scroll_offset(const reach_ui_state *state)
{
    if (state == 0 || state->launcher.result_count <= REACH_SEARCH_VISIBLE_RESULTS)
    {
        return 0;
    }
    return state->launcher.result_count - REACH_SEARCH_VISIBLE_RESULTS;
}

static size_t reach_ui_state_launcher_scroll_offset(const reach_ui_state *state)
{
    return state != 0 && state->launcher.result_scrollbar.offset > 0.0f
               ? (size_t)(state->launcher.result_scrollbar.offset + 0.5f)
               : 0;
}

size_t reach_ui_state_launcher_result_scroll_offset(const reach_ui_state *state)
{
    return reach_ui_state_launcher_scroll_offset(state);
}

static void reach_ui_state_set_launcher_scroll_immediate(reach_ui_state *state, size_t offset)
{
    if (state == 0)
    {
        return;
    }
    reach_scrollbar_set_target(&state->launcher.result_scrollbar, (float)offset);
    state->launcher.result_scrollbar.offset = state->launcher.result_scrollbar.target;
}

static void reach_ui_state_clamp_launcher_result_scroll(reach_ui_state *state)
{
    if (state == 0)
    {
        return;
    }

    size_t max_offset = reach_ui_state_launcher_max_scroll_offset(state);
    if (reach_ui_state_launcher_scroll_offset(state) > max_offset)
    {
        reach_ui_state_set_launcher_scroll_immediate(state, max_offset);
    }
}

static void reach_ui_state_keep_selected_launcher_result_visible(reach_ui_state *state)
{
    if (state == 0)
    {
        return;
    }

    reach_ui_state_clamp_launcher_result_scroll(state);
    size_t visible_count = reach_ui_state_launcher_visible_count(state);
    if (visible_count == 0)
    {
        reach_ui_state_set_launcher_scroll_immediate(state, 0);
        return;
    }

    size_t offset = reach_ui_state_launcher_scroll_offset(state);
    if (state->launcher.selected_result_index < offset)
    {
        reach_ui_state_set_launcher_scroll_immediate(state, state->launcher.selected_result_index);
    }
    else if (state->launcher.selected_result_index >= offset + visible_count)
    {
        reach_ui_state_set_launcher_scroll_immediate(state, state->launcher.selected_result_index -
                                                                visible_count + 1);
    }

    reach_ui_state_clamp_launcher_result_scroll(state);
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
    state->launcher.result_count = 0;
    state->launcher.selected_result_index = 0;
    reach_scrollbar_model_init(&state->launcher.result_scrollbar, REACH_SCROLLBAR_DRAG_STEPPED,
                               1.0f);
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
    reach_scrollbar_set_extents(&state->launcher.result_scrollbar, 0.0f, 0.0f);
    reach_ui_state_set_launcher_scroll_immediate(state, 0);
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
    reach_scrollbar_set_extents(&state->launcher.result_scrollbar, (float)count,
                                (float)reach_ui_state_launcher_visible_count(state));
    state->launcher.selected_result_index = 0;
    reach_ui_state_set_launcher_scroll_immediate(state, 0);
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
    reach_ui_state_keep_selected_launcher_result_visible(state);
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
    reach_ui_state_keep_selected_launcher_result_visible(state);
    return REACH_OK;
}

reach_result reach_ui_state_set_launcher_result_scroll_offset(reach_ui_state *state, size_t offset)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_scrollbar_set_target(&state->launcher.result_scrollbar, (float)offset);
    state->launcher.result_scrollbar.offset = state->launcher.result_scrollbar.target;
    reach_ui_state_clamp_launcher_result_scroll(state);
    return REACH_OK;
}

reach_result reach_ui_state_scroll_launcher_results(reach_ui_state *state, int32_t delta)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    size_t offset = reach_ui_state_launcher_scroll_offset(state);
    if (delta < 0)
    {
        size_t amount = (size_t)(-delta);
        offset = amount > offset ? 0 : offset - amount;
    }
    else
    {
        offset += (size_t)delta;
    }

    return reach_ui_state_set_launcher_result_scroll_offset(state, offset);
}

reach_result reach_ui_state_set_launcher_selected_result(reach_ui_state *state, size_t index)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (state->launcher.result_count == 0)
    {
        state->launcher.selected_result_index = 0;
        reach_ui_state_set_launcher_scroll_immediate(state, 0);
        return REACH_OK;
    }
    if (index >= state->launcher.result_count)
    {
        index = state->launcher.result_count - 1;
    }
    state->launcher.selected_result_index = index;
    reach_ui_state_keep_selected_launcher_result_visible(state);
    return REACH_OK;
}

int32_t reach_ui_state_should_show_pinned_apps(const reach_ui_state *state)
{
    (void)state;
    return 0;
}
