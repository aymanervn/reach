#include "reach/features/launcher.h"

#include "launcher_common.h"

#include <math.h>
#include <new>

#include "reach/features/common/section_reveal.h"
#include "reach/support/animation.h"

enum
{
    REACH_LAUNCHER_ANIMATION_RESULTS_EXPANSION = 0,
    REACH_LAUNCHER_ANIMATION_COUNT
};

struct reach_launcher
{
    reach_animation_manager animations;
    reach_animation_track animation_tracks[REACH_LAUNCHER_ANIMATION_COUNT];
    reach_launcher_state state;
    reach_search_service *search;
    uint32_t search_generation;
    reach_icon_service *icons;
    uint16_t terminal_icon_ref[REACH_SEARCH_RESULT_PATH_CAPACITY];
    reach_launcher_layout pointer_layout;
    int32_t pointer_layout_valid;
    reach_transform_f32 pointer_transform;
};

static int32_t reach_launcher_results_attached(const reach_launcher_state *state)
{
    return state != nullptr && (state->model.result_count > 0 ||
                                (state->model.search_error && state->model.query_length > 0));
}

float reach_launcher_results_expansion(const reach_launcher *launcher)
{
    if (launcher == nullptr)
    {
        return 0.0f;
    }
    float value = reach_animation_manager_value(&launcher->animations,
                                                REACH_LAUNCHER_ANIMATION_RESULTS_EXPANSION);
    if (value < 0.0f)
    {
        return 0.0f;
    }
    return value > 1.0f ? 1.0f : value;
}

static void reach_launcher_sync_results_expansion(reach_launcher *launcher, int32_t was_attached)
{
    if (launcher == nullptr)
    {
        return;
    }
    int32_t attached = reach_launcher_results_attached(&launcher->state);
    if (attached && !was_attached)
    {
        reach_animation_manager_start(&launcher->animations,
                                      REACH_LAUNCHER_ANIMATION_RESULTS_EXPANSION, 0.0f, 1.0f,
                                      REACH_SECTION_REVEAL_SECONDS, REACH_EASING_EASE_OUT);
    }
    else if (!attached)
    {
        reach_animation_manager_set(&launcher->animations,
                                    REACH_LAUNCHER_ANIMATION_RESULTS_EXPANSION, 0.0f);
    }
}

uint32_t reach_launcher_search_generation(const reach_launcher *launcher)
{
    return launcher != nullptr ? launcher->search_generation : 0;
}

void reach_launcher_set_pointer_context(reach_launcher *launcher,
                                        const reach_launcher_layout *layout)
{
    if (launcher == nullptr)
    {
        return;
    }
    launcher->pointer_layout_valid = layout != nullptr;
    if (layout != nullptr)
    {
        launcher->pointer_layout = *layout;
    }
}

int32_t reach_launcher_arrange(reach_launcher *launcher, const reach_launcher_arrange_context *ctx)
{
    if (launcher == nullptr || ctx == nullptr)
    {
        return 0;
    }

    reach_ui_layout_input input = {};
    input.monitor_bounds = ctx->monitor_bounds;
    input.work_area = ctx->monitor_bounds;
    input.dpi_scale = ctx->dpi_scale;
    input.border_thickness = reach_theme_border_thickness(
        ctx->theme != nullptr ? ctx->theme : reach_theme_default(), ctx->dpi_scale);

    reach_launcher_layout layout = {};
    if (reach_launcher_layout_compute(&launcher->state.model, &input, &layout) != REACH_OK)
    {
        return 0;
    }

    int32_t changed = !launcher->pointer_layout_valid ||
                      !reach_rect_equal(launcher->pointer_layout.bounds, layout.bounds);
    reach_launcher_set_pointer_context(launcher, &layout);
    return changed;
}

void reach_launcher_set_pointer_transform(reach_launcher *launcher, reach_transform_f32 transform)
{
    if (launcher != nullptr)
    {
        launcher->pointer_transform = transform;
    }
}

reach_result
reach_launcher_append_surface_render_commands(reach_launcher *launcher, const reach_theme *theme,
                                              float dpi_scale,
                                              reach_render_command_buffer *out_commands)
{
    if (launcher == nullptr || !launcher->pointer_layout_valid)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_launcher_render_context render = {};
    render.theme = theme;
    render.layout = &launcher->pointer_layout;
    render.dpi_scale = dpi_scale;
    return reach_launcher_append_render_commands(launcher, &render, out_commands);
}

void reach_launcher_attach_icons(reach_launcher *launcher, reach_icon_service *icons)
{
    if (launcher != nullptr)
    {
        launcher->icons = icons;
    }
}

void reach_launcher_set_terminal_icon_ref(reach_launcher *launcher, const uint16_t *icon_ref)
{
    if (launcher != nullptr)
    {
        reach_copy_utf16(launcher->terminal_icon_ref, REACH_SEARCH_RESULT_PATH_CAPACITY,
                         icon_ref != nullptr ? icon_ref : (const uint16_t *)L"");
    }
}

reach_icon_service *reach_launcher_icons(reach_launcher *launcher)
{
    return launcher != nullptr ? launcher->icons : nullptr;
}

static size_t reach_launcher_visible_count(const reach_launcher_state *state)
{
    if (state == 0)
    {
        return 0;
    }
    return state->model.result_count < REACH_SEARCH_VISIBLE_RESULTS ? state->model.result_count
                                                                    : REACH_SEARCH_VISIBLE_RESULTS;
}

static size_t reach_launcher_max_scroll_offset(const reach_launcher_state *state)
{
    if (state == 0 || state->model.result_count <= REACH_SEARCH_VISIBLE_RESULTS)
    {
        return 0;
    }
    return state->model.result_count - REACH_SEARCH_VISIBLE_RESULTS;
}

size_t reach_launcher_model_result_scroll_offset(const reach_launcher_model *launcher)
{
    return launcher != 0 && launcher->result_scrollbar.offset > 0.0f
               ? (size_t)(launcher->result_scrollbar.offset + 0.5f)
               : 0;
}

size_t reach_launcher_result_scroll_offset_state(const reach_launcher_state *state)
{
    return state != 0 ? reach_launcher_model_result_scroll_offset(&state->model) : 0;
}

static void reach_launcher_set_scroll_immediate(reach_launcher_state *state, size_t offset)
{
    if (state == 0)
    {
        return;
    }
    reach_scrollbar_set_target(&state->model.result_scrollbar, (float)offset);
    state->model.result_scrollbar.offset = state->model.result_scrollbar.target;
}

static void reach_launcher_clamp_result_scroll(reach_launcher_state *state)
{
    if (state == 0)
    {
        return;
    }

    size_t max_offset = reach_launcher_max_scroll_offset(state);
    if (reach_launcher_result_scroll_offset_state(state) > max_offset)
    {
        reach_launcher_set_scroll_immediate(state, max_offset);
    }
}

static void reach_launcher_keep_selected_visible(reach_launcher_state *state)
{
    if (state == 0)
    {
        return;
    }

    reach_launcher_clamp_result_scroll(state);
    size_t visible_count = reach_launcher_visible_count(state);
    if (visible_count == 0)
    {
        reach_launcher_set_scroll_immediate(state, 0);
        return;
    }

    size_t offset = reach_launcher_result_scroll_offset_state(state);
    if (state->model.selected_result_index < offset)
    {
        reach_launcher_set_scroll_immediate(state, state->model.selected_result_index);
    }
    else if (state->model.selected_result_index >= offset + visible_count)
    {
        reach_launcher_set_scroll_immediate(state,
                                            state->model.selected_result_index - visible_count + 1);
    }

    reach_launcher_clamp_result_scroll(state);
}

void reach_launcher_state_init(reach_launcher_state *state)
{
    if (state == 0)
    {
        return;
    }

    state->model.open = 0;
    state->model.query[0] = 0;
    state->model.query_length = 0;
    state->model.result_count = 0;
    state->model.selected_result_index = 0;
    state->model.search_error = 0;
    reach_scrollbar_model_init(&state->model.result_scrollbar, REACH_SCROLLBAR_DRAG_STEPPED, 1.0f);
    reach_pressable_init(&state->pressable);
    state->launcher_scrollbar_drag = reach_scrollbar_drag{};
    reach_text_edit_init(&state->launcher_text_edit, REACH_MAX_SEARCH_CHARS);
    state->launcher_caret_blink_seconds = 0.0;
    state->launcher_caret_visible = 1;
}

reach_result reach_launcher_open_state(reach_launcher_state *state)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    state->model.open = 1;
    return REACH_OK;
}

reach_result reach_launcher_close_state(reach_launcher_state *state)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    state->model.open = 0;
    return REACH_OK;
}

reach_result reach_launcher_set_query_state(reach_launcher_state *state, const uint16_t *query)
{
    if (state == 0 || query == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    size_t length = reach_strlen_utf16(query);
    if (length > REACH_MAX_SEARCH_CHARS)
    {
        length = REACH_MAX_SEARCH_CHARS;
    }

    for (size_t index = 0; index < length; ++index)
    {
        state->model.query[index] = query[index];
    }
    state->model.query[length] = 0;
    state->model.query_length = length;
    return REACH_OK;
}

reach_result reach_launcher_clear_results_state(reach_launcher_state *state)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < REACH_SEARCH_MAX_RESULTS; ++index)
    {
        state->model.results[index] = reach_launcher_result{};
    }
    state->model.result_count = 0;
    state->model.selected_result_index = 0;
    state->model.search_error = 0;
    reach_scrollbar_set_extents(&state->model.result_scrollbar, 0.0f, 0.0f);
    reach_launcher_set_scroll_immediate(state, 0);
    return REACH_OK;
}

reach_result reach_launcher_set_results_state(reach_launcher_state *state,
                                              const reach_search_candidate *results, size_t count)
{
    if (state == 0 || (results == 0 && count != 0))
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (count > REACH_SEARCH_MAX_RESULTS)
    {
        count = REACH_SEARCH_MAX_RESULTS;
    }

    (void)reach_launcher_clear_results_state(state);
    for (size_t index = 0; index < count; ++index)
    {
        reach_launcher_result *result = &state->model.results[index];
        reach_copy_utf16(result->title, REACH_SEARCH_RESULT_NAME_CAPACITY, results[index].name);
        reach_copy_utf16(result->subtitle, REACH_SEARCH_RESULT_PATH_CAPACITY, results[index].path);
        reach_copy_utf16(result->icon_path, REACH_SEARCH_RESULT_PATH_CAPACITY, results[index].path);
        result->visual_kind = results[index].kind;
        result->action = REACH_LAUNCHER_RESULT_OPEN_SEARCH;
        result->payload.search = results[index];
    }
    state->model.result_count = count;
    reach_scrollbar_set_extents(&state->model.result_scrollbar, (float)count,
                                (float)reach_launcher_visible_count(state));
    state->model.selected_result_index = 0;
    reach_launcher_set_scroll_immediate(state, 0);
    return REACH_OK;
}

reach_result reach_launcher_set_terminal_command_result_state(reach_launcher_state *state,
                                                              const uint16_t *command,
                                                              const uint16_t *icon_ref)
{
    if (state == nullptr || command == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    (void)reach_launcher_clear_results_state(state);
    reach_launcher_result *result = &state->model.results[0];
    reach_copy_utf16(result->title, REACH_SEARCH_RESULT_NAME_CAPACITY,
                     (const uint16_t *)L"Run in Windows Terminal");
    reach_copy_utf16(result->subtitle, REACH_SEARCH_RESULT_PATH_CAPACITY, command);
    reach_copy_utf16(result->icon_path, REACH_SEARCH_RESULT_PATH_CAPACITY,
                     icon_ref != nullptr ? icon_ref : (const uint16_t *)L"");
    result->visual_kind = REACH_SEARCH_RESULT_APP;
    result->action = REACH_LAUNCHER_RESULT_RUN_TERMINAL_COMMAND;
    reach_copy_utf16(result->payload.terminal_command, REACH_MAX_SEARCH_CHARS + 1, command);
    state->model.result_count = 1;
    state->model.selected_result_index = 0;
    reach_scrollbar_set_extents(&state->model.result_scrollbar, 1.0f, 1.0f);
    reach_launcher_set_scroll_immediate(state, 0);
    return REACH_OK;
}

reach_result reach_launcher_select_next_result_state(reach_launcher_state *state)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (state->model.result_count == 0)
    {
        state->model.selected_result_index = 0;
        return REACH_OK;
    }
    if (state->model.selected_result_index + 1 < state->model.result_count)
    {
        state->model.selected_result_index += 1;
    }
    reach_launcher_keep_selected_visible(state);
    return REACH_OK;
}

reach_result reach_launcher_select_previous_result_state(reach_launcher_state *state)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (state->model.result_count == 0 || state->model.selected_result_index == 0)
    {
        state->model.selected_result_index = 0;
        return REACH_OK;
    }
    state->model.selected_result_index -= 1;
    reach_launcher_keep_selected_visible(state);
    return REACH_OK;
}

reach_result reach_launcher_set_result_scroll_offset_state(reach_launcher_state *state,
                                                           size_t offset)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_scrollbar_set_target(&state->model.result_scrollbar, (float)offset);
    state->model.result_scrollbar.offset = state->model.result_scrollbar.target;
    reach_launcher_clamp_result_scroll(state);
    return REACH_OK;
}

reach_result reach_launcher_scroll_results_state(reach_launcher_state *state, int32_t delta)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    size_t offset = reach_launcher_result_scroll_offset_state(state);
    if (delta < 0)
    {
        size_t amount = (size_t)(-delta);
        offset = amount > offset ? 0 : offset - amount;
    }
    else
    {
        offset += (size_t)delta;
    }

    return reach_launcher_set_result_scroll_offset_state(state, offset);
}

reach_result reach_launcher_set_selected_result_state(reach_launcher_state *state, size_t index)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (state->model.result_count == 0)
    {
        state->model.selected_result_index = 0;
        reach_launcher_set_scroll_immediate(state, 0);
        return REACH_OK;
    }
    if (index >= state->model.result_count)
    {
        index = state->model.result_count - 1;
    }
    state->model.selected_result_index = index;
    reach_launcher_keep_selected_visible(state);
    return REACH_OK;
}

reach_result reach_launcher_create(reach_launcher **out_launcher)
{
    if (out_launcher == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_launcher *launcher = new (std::nothrow) reach_launcher();
    if (launcher == nullptr)
    {
        return REACH_ERROR;
    }
    reach_animation_manager_init(&launcher->animations, launcher->animation_tracks,
                                 REACH_LAUNCHER_ANIMATION_COUNT);
    reach_animation_manager_set(&launcher->animations, REACH_LAUNCHER_ANIMATION_RESULTS_EXPANSION,
                                0.0f);
    reach_launcher_state_init(&launcher->state);
    *out_launcher = launcher;
    return REACH_OK;
}

void reach_launcher_destroy(reach_launcher *launcher)
{
    delete launcher;
}

const reach_launcher_state *reach_launcher_state_ptr(reach_launcher *launcher)
{
    return launcher != nullptr ? &launcher->state : nullptr;
}

reach_launcher_state *reach_launcher_state_mut(reach_launcher *launcher)
{
    return launcher != nullptr ? &launcher->state : nullptr;
}

int32_t reach_launcher_is_open(reach_launcher *launcher)
{
    return launcher != nullptr && reach_launcher_state_ptr(launcher)->model.open;
}

size_t reach_launcher_result_count(reach_launcher *launcher)
{
    return launcher != nullptr ? reach_launcher_state_ptr(launcher)->model.result_count : 0;
}

const reach_launcher_result *reach_launcher_result_at(reach_launcher *launcher, size_t index)
{
    if (launcher == nullptr || index >= reach_launcher_state_ptr(launcher)->model.result_count)
    {
        return nullptr;
    }
    return &reach_launcher_state_ptr(launcher)->model.results[index];
}

size_t reach_launcher_selected_result_index(reach_launcher *launcher)
{
    return launcher != nullptr ? reach_launcher_state_ptr(launcher)->model.selected_result_index
                               : 0;
}

const uint16_t *reach_launcher_query_text(reach_launcher *launcher)
{
    return launcher != nullptr ? reach_launcher_state_ptr(launcher)->model.query : nullptr;
}

static size_t reach_launcher_query_length(reach_launcher *launcher)
{
    return launcher != nullptr ? reach_launcher_state_ptr(launcher)->model.query_length : 0;
}

static int32_t reach_launcher_terminal_command_mode(const reach_launcher *launcher)
{
    return launcher != nullptr && launcher->state.model.query[0] == '!';
}

static int32_t reach_launcher_has_terminal_command_result(const reach_launcher *launcher)
{
    return launcher != nullptr && launcher->state.model.result_count == 1 &&
           launcher->state.model.results[0].action == REACH_LAUNCHER_RESULT_RUN_TERMINAL_COMMAND;
}

void reach_launcher_clear_query(reach_launcher *launcher)
{
    if (launcher != nullptr)
    {
        reach_launcher_state_mut(launcher)->model.query[0] = 0;
        reach_launcher_state_mut(launcher)->model.query_length = 0;
    }
}

int32_t reach_launcher_set_open(reach_launcher *launcher, int32_t open)
{
    if (launcher == nullptr || launcher->state.model.open == (open ? 1 : 0))
    {
        return 0;
    }
    if (open)
    {
        (void)reach_launcher_open_state(&launcher->state);
        reach_launcher_reset_text_edit(launcher);
    }
    else
    {
        (void)reach_launcher_close_state(&launcher->state);
    }
    return 1;
}

void reach_launcher_surface_hidden(reach_launcher *launcher)
{
    if (launcher == nullptr)
    {
        return;
    }
    reach_launcher_cancel_search(launcher);
    reach_launcher_clear_query(launcher);
    (void)reach_launcher_clear_results(launcher);
    reach_launcher_reset_text_edit(launcher);
}

reach_result reach_launcher_set_query(reach_launcher *launcher, const uint16_t *query)
{
    return reach_launcher_set_query_state(reach_launcher_state_mut(launcher), query);
}

reach_result reach_launcher_set_results(reach_launcher *launcher,
                                        const reach_search_candidate *results, size_t count)
{
    if (launcher == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    int32_t was_attached = reach_launcher_results_attached(&launcher->state);
    reach_result result = reach_launcher_set_results_state(&launcher->state, results, count);
    if (result == REACH_OK)
    {
        reach_launcher_sync_results_expansion(launcher, was_attached);
    }
    return result;
}

reach_result reach_launcher_clear_results(reach_launcher *launcher)
{
    if (launcher == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    int32_t was_attached = reach_launcher_results_attached(&launcher->state);
    reach_result result = reach_launcher_clear_results_state(&launcher->state);
    if (result == REACH_OK)
    {
        reach_launcher_sync_results_expansion(launcher, was_attached);
    }
    return result;
}

void reach_launcher_set_search_error(reach_launcher *launcher, int32_t error)
{
    if (launcher != nullptr)
    {
        int32_t was_attached = reach_launcher_results_attached(&launcher->state);
        launcher->state.model.search_error = error ? 1 : 0;
        reach_launcher_sync_results_expansion(launcher, was_attached);
    }
}

void reach_launcher_attach_search(reach_launcher *launcher, reach_search_service *search)
{
    if (launcher != nullptr)
    {
        launcher->search = search;
    }
}

static reach_result reach_launcher_submit_search(reach_launcher *launcher)
{
    if (launcher == nullptr || launcher->search == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    uint32_t generation = ++launcher->search_generation;
    return reach_search_service_submit(launcher->search, launcher->state.model.query, generation);
}

void reach_launcher_cancel_search(reach_launcher *launcher)
{
    if (launcher == nullptr)
    {
        return;
    }
    ++launcher->search_generation;
    if (launcher->search != nullptr)
    {
        reach_search_service_cancel(launcher->search);
    }
}

int32_t reach_launcher_take_search_results(reach_launcher *launcher,
                                           reach_search_candidate *out_results, size_t *out_count,
                                           int32_t *out_error)
{
    if (launcher == nullptr || launcher->search == nullptr || out_results == nullptr ||
        out_count == nullptr)
    {
        return 0;
    }
    uint32_t generation = 0;
    if (!reach_search_service_take_completed(launcher->search, &generation, out_results, out_count,
                                             out_error))
    {
        return 0;
    }

    if (generation != launcher->search_generation || !launcher->state.model.open)
    {
        return 0;
    }
    return 1;
}

const reach_ui_event_type *reach_launcher_activation_events(size_t *out_count)
{
    static const reach_ui_event_type events[] = {REACH_UI_EVENT_WINDOWS_KEY};
    if (out_count != nullptr)
    {
        *out_count = sizeof(events) / sizeof(events[0]);
    }
    return events;
}

const reach_ui_event_type *reach_launcher_routed_events(size_t *out_count)
{
    static const reach_ui_event_type events[] = {
        REACH_UI_EVENT_ESCAPE,    REACH_UI_EVENT_ENTER,     REACH_UI_EVENT_ARROW_UP,
        REACH_UI_EVENT_ARROW_DOWN, REACH_UI_EVENT_TEXT_CHAR, REACH_UI_EVENT_TEXT_EDIT};
    if (out_count != nullptr)
    {
        *out_count = sizeof(events) / sizeof(events[0]);
    }
    return events;
}

#define REACH_LAUNCHER_CARET_BLINK_SECONDS 0.53

static void reach_launcher_reset_caret(reach_launcher *launcher)
{
    if (launcher == nullptr)
    {
        return;
    }
    launcher->state.launcher_caret_blink_seconds = 0.0;
    launcher->state.launcher_caret_visible = 1;
}

static int32_t reach_launcher_tick_caret(reach_launcher *launcher, double delta_seconds)
{
    if (launcher == nullptr)
    {
        return 0;
    }
    if (!launcher->state.model.open)
    {
        reach_launcher_reset_caret(launcher);
        return 0;
    }
    launcher->state.launcher_caret_blink_seconds += delta_seconds;
    if (launcher->state.launcher_caret_blink_seconds >= REACH_LAUNCHER_CARET_BLINK_SECONDS)
    {
        launcher->state.launcher_caret_blink_seconds = 0.0;
        launcher->state.launcher_caret_visible = !launcher->state.launcher_caret_visible;
        return 1;
    }
    return 0;
}

static void reach_launcher_reset_scrollbar_drag(reach_launcher *launcher)
{
    if (launcher == nullptr)
    {
        return;
    }
    launcher->state.launcher_scrollbar_drag.active = 0;
    launcher->state.launcher_scrollbar_drag.grab_offset = 0.0f;
}

void reach_launcher_reset_text_edit(reach_launcher *launcher)
{
    if (launcher == nullptr)
    {
        return;
    }
    reach_text_edit_init(&launcher->state.launcher_text_edit, REACH_MAX_SEARCH_CHARS);
    reach_launcher_reset_caret(launcher);
}

void reach_launcher_handle_text_event(reach_launcher *launcher, const reach_ui_event *event,
                                      reach_launcher_text_event_result *out_result)
{
    if (out_result != nullptr)
    {
        *out_result = {};
    }
    if (launcher == nullptr || event == nullptr || out_result == nullptr ||
        !reach_launcher_is_open(launcher))
    {
        return;
    }

    reach_text_edit *edit = &launcher->state.launcher_text_edit;
    reach_text_edit_event te = REACH_TEXT_EDIT_EVENT_NONE;

    if (event->type == REACH_UI_EVENT_TEXT_CHAR)
    {
        te = reach_text_edit_insert_char(edit, (uint16_t)event->id);
    }
    else if (event->type == REACH_UI_EVENT_TEXT_EDIT)
    {
        reach_text_edit_modifiers mods = {};
        mods.shift = (event->modifiers & REACH_UI_EVENT_MODIFIER_SHIFT) ? 1 : 0;
        mods.ctrl = (event->modifiers & REACH_UI_EVENT_MODIFIER_CTRL) ? 1 : 0;
        switch ((reach_ui_edit_key)event->id)
        {
        case REACH_UI_EDIT_KEY_BACKSPACE:
            te = reach_text_edit_handle_key(edit, REACH_TEXT_EDIT_KEY_BACKSPACE, mods);
            break;
        case REACH_UI_EDIT_KEY_DELETE:
            te = reach_text_edit_handle_key(edit, REACH_TEXT_EDIT_KEY_DELETE, mods);
            break;
        case REACH_UI_EDIT_KEY_LEFT:
            te = reach_text_edit_handle_key(edit, REACH_TEXT_EDIT_KEY_LEFT, mods);
            break;
        case REACH_UI_EDIT_KEY_RIGHT:
            te = reach_text_edit_handle_key(edit, REACH_TEXT_EDIT_KEY_RIGHT, mods);
            break;
        case REACH_UI_EDIT_KEY_HOME:
            te = reach_text_edit_handle_key(edit, REACH_TEXT_EDIT_KEY_HOME, mods);
            break;
        case REACH_UI_EDIT_KEY_END:
            te = reach_text_edit_handle_key(edit, REACH_TEXT_EDIT_KEY_END, mods);
            break;
        case REACH_UI_EDIT_KEY_SELECT_ALL:
            reach_text_edit_select_all(edit);
            break;
        case REACH_UI_EDIT_KEY_NONE:
        default:
            return;
        }
    }
    else
    {
        return;
    }

    reach_launcher_reset_caret(launcher);
    out_result->redraw = 1;

    if (te == REACH_TEXT_EDIT_EVENT_TEXT_CHANGED)
    {
        (void)reach_launcher_set_query_state(&launcher->state, edit->text);
        if (reach_launcher_query_length(launcher) == 0)
        {
            reach_launcher_cancel_search(launcher);
            (void)reach_launcher_clear_results(launcher);
        }
        else if (reach_launcher_terminal_command_mode(launcher))
        {
            reach_launcher_cancel_search(launcher);
            int32_t was_attached = reach_launcher_results_attached(&launcher->state);
            (void)reach_launcher_set_terminal_command_result_state(&launcher->state, edit->text + 1,
                                                                   launcher->terminal_icon_ref);
            reach_launcher_sync_results_expansion(launcher, was_attached);
        }
        else
        {
            if (reach_launcher_has_terminal_command_result(launcher))
            {
                (void)reach_launcher_clear_results(launcher);
            }
            (void)reach_launcher_submit_search(launcher);
        }
        out_result->relayout = 1;
    }
}

static void reach_launcher_capsule_reset(void *capsule)
{
    reach_launcher *launcher = static_cast<reach_launcher *>(capsule);
    reach_launcher_reset_scrollbar_drag(launcher);
    if (launcher != nullptr)
    {
        reach_pressable_reset(&launcher->state.pressable, nullptr);
        launcher->pointer_layout_valid = 0;
        launcher->pointer_transform = {1.0f, 1.0f, 0.0f, 0.0f};
    }
}

static int32_t reach_launcher_drain_search_results(reach_launcher *launcher)
{
    reach_search_candidate results[REACH_SEARCH_MAX_RESULTS] = {};
    size_t count = 0;
    int32_t error = 0;
    if (!reach_launcher_take_search_results(launcher, results, &count, &error))
    {
        return 0;
    }
    (void)reach_launcher_set_results(launcher, results, count);
    reach_launcher_set_search_error(launcher, error);
    return 1;
}

static void reach_launcher_capsule_tick(void *capsule, double delta_seconds,
                                        reach_feature_tick_result *out)
{
    reach_launcher *launcher = static_cast<reach_launcher *>(capsule);
    if (out != nullptr && reach_launcher_drain_search_results(launcher))
    {
        out->redraw = 1;
        out->relayout = 1;
        out->request_update = 1;
    }
    int32_t expansion_was_active =
        launcher != nullptr &&
        reach_animation_manager_active(&launcher->animations,
                                       REACH_LAUNCHER_ANIMATION_RESULTS_EXPANSION);
    if (launcher != nullptr)
    {
        reach_animation_manager_tick(&launcher->animations, delta_seconds);
    }
    int32_t expansion_active =
        launcher != nullptr &&
        reach_animation_manager_active(&launcher->animations,
                                       REACH_LAUNCHER_ANIMATION_RESULTS_EXPANSION);
    if (out != nullptr && (reach_launcher_tick_caret(launcher, delta_seconds) ||
                           expansion_was_active || expansion_active))
    {
        out->redraw = 1;
    }
}

static int32_t reach_launcher_capsule_is_open(const void *capsule)
{
    return reach_launcher_is_open(
        const_cast<reach_launcher *>(static_cast<const reach_launcher *>(capsule)));
}

static int32_t reach_launcher_capsule_needs_frame(const void *capsule)
{
    return reach_launcher_capsule_is_open(capsule);
}

static int32_t reach_launcher_capsule_wants_pointer_move(const void *capsule)
{
    const reach_launcher *launcher = static_cast<const reach_launcher *>(capsule);
    if (launcher == nullptr)
    {
        return 0;
    }
    const reach_launcher_state *state =
        reach_launcher_state_ptr(const_cast<reach_launcher *>(launcher));
    return state->launcher_scrollbar_drag.active || reach_pressable_tracking(&state->pressable);
}

static int32_t reach_launcher_capsule_pointer_sequence_active(const void *capsule)
{
    return reach_launcher_capsule_wants_pointer_move(capsule);
}

static reach_launcher_event_context
reach_launcher_capsule_event_context(const reach_launcher *launcher)
{
    reach_launcher_event_context ctx = {};
    if (launcher != nullptr)
    {
        ctx.layout = launcher->pointer_layout_valid ? &launcher->pointer_layout : nullptr;
    }
    return ctx;
}

static int32_t reach_launcher_query_starts_with_ascii(const uint16_t *text, const char *prefix)
{
    if (text == nullptr || prefix == nullptr)
    {
        return 0;
    }

    size_t index = 0;
    while (prefix[index] != 0)
    {
        uint16_t current = text[index];
        char expected = prefix[index];
        if (current >= 'A' && current <= 'Z')
        {
            current = (uint16_t)(current - 'A' + 'a');
        }
        if (expected >= 'A' && expected <= 'Z')
        {
            expected = (char)(expected - 'A' + 'a');
        }
        if (current != (uint16_t)expected)
        {
            return 0;
        }
        ++index;
    }
    return 1;
}

reach_feature_target reach_launcher_open_target(const reach_launcher *launcher)
{
    reach_feature_target target = {};
    if (launcher == nullptr)
    {
        return target;
    }

    const reach_launcher_model *model = &launcher->state.model;
    if (model->result_count > 0 && model->selected_result_index < model->result_count)
    {
        const reach_launcher_result *result = &model->results[model->selected_result_index];
        if (result->action == REACH_LAUNCHER_RESULT_RUN_TERMINAL_COMMAND)
        {
            target.kind = REACH_FEATURE_TARGET_TERMINAL_COMMAND;
            target.path = result->payload.terminal_command;
            return target;
        }

        const reach_search_candidate *search = &result->payload.search;
        if (search->path[0] == 0)
        {
            return target;
        }
        target.kind = search->kind == REACH_SEARCH_RESULT_APP ? REACH_FEATURE_TARGET_APP
                                                              : REACH_FEATURE_TARGET_PATH;
        target.path = search->path;
        target.arguments = search->arguments[0] != 0 ? search->arguments : nullptr;
        return target;
    }

    if (model->query[0] == 0)
    {
        target.kind = REACH_FEATURE_TARGET_DEFAULT_LOCATION;
        return target;
    }

    target.kind = reach_launcher_query_starts_with_ascii(model->query, "shell:")
                      ? REACH_FEATURE_TARGET_SHELL_LOCATION
                      : REACH_FEATURE_TARGET_LOCATION;
    target.path = model->query;
    return target;
}

reach_feature_target reach_launcher_reveal_target(const reach_launcher *launcher, size_t index)
{
    reach_feature_target target = {};
    if (launcher == nullptr || index >= launcher->state.model.result_count)
    {
        return target;
    }

    const reach_launcher_result *result = &launcher->state.model.results[index];
    const reach_search_candidate *search = &result->payload.search;
    if (result->action != REACH_LAUNCHER_RESULT_OPEN_SEARCH ||
        search->kind != REACH_SEARCH_RESULT_APP || search->path[0] == 0)
    {
        return target;
    }

    target.kind = REACH_FEATURE_TARGET_APP;
    target.path = search->path;
    return target;
}

static void reach_launcher_set_open_action(const reach_launcher *launcher,
                                           reach_capsule_action *out)
{
    out->kind = REACH_FEATURE_ACTION_OPEN_TARGET;
    out->target = reach_launcher_open_target(launcher);
    if ((out->target.kind == REACH_FEATURE_TARGET_APP ||
         out->target.kind == REACH_FEATURE_TARGET_PATH) &&
        launcher->state.model.open)
    {
        out->flags |= REACH_FEATURE_ACTION_FLAG_DEFER_UNTIL_CLOSED;
    }
}

static void reach_launcher_capsule_handle_event(void *capsule, const reach_ui_event *event,
                                                reach_capsule_event_result *out)
{
    reach_launcher *launcher = static_cast<reach_launcher *>(capsule);
    if (launcher == nullptr || event == nullptr || out == nullptr ||
        !launcher->state.model.open)
    {
        return;
    }

    switch (event->type)
    {
    case REACH_UI_EVENT_ESCAPE:
        if (reach_launcher_set_open(launcher, 0))
        {
            out->handled = 1;
            out->redraw = 1;
            out->relayout = 1;
            out->request_update = 1;
        }
        return;
    case REACH_UI_EVENT_ENTER:
        reach_launcher_set_open_action(launcher, &out->action);
        out->handled = 1;
        return;
    case REACH_UI_EVENT_ARROW_UP:
    case REACH_UI_EVENT_ARROW_DOWN:
    {
        reach_result result =
            event->type == REACH_UI_EVENT_ARROW_UP
                ? reach_launcher_select_previous_result_state(&launcher->state)
                : reach_launcher_select_next_result_state(&launcher->state);
        if (result == REACH_OK)
        {
            out->handled = 1;
            out->redraw = 1;
            out->relayout = 1;
            out->request_update = 1;
        }
        return;
    }
    case REACH_UI_EVENT_TEXT_CHAR:
    case REACH_UI_EVENT_TEXT_EDIT:
    {
        reach_launcher_text_event_result text = {};
        reach_launcher_handle_text_event(launcher, event, &text);
        out->handled = 1;
        out->redraw = text.redraw;
        out->relayout = text.relayout;
        out->request_update = text.redraw || text.relayout;
        return;
    }
    default:
        return;
    }
}

static void
reach_launcher_capsule_apply_event_result(const reach_launcher *launcher,
                                          const reach_launcher_event_result *event_result,
                                          reach_capsule_pointer_result *out)
{
    if (event_result == nullptr || out == nullptr)
    {
        return;
    }
    out->handled = event_result->handled;
    out->redraw = event_result->redraw;
    out->relayout = event_result->viewport_changed;
    out->capture = event_result->capture_pointer;
    out->sync_pointer_subscriptions = event_result->sync_pointer_subscriptions;
    if (event_result->action.type == REACH_LAUNCHER_ACTION_OPEN_RESULT)
    {
        reach_launcher_set_open_action(launcher, &out->action);
    }
    else if (event_result->action.type == REACH_LAUNCHER_ACTION_REVEAL_RESULT)
    {
        out->action.kind = REACH_FEATURE_ACTION_REVEAL_TARGET;
        out->action.target =
            reach_launcher_reveal_target(launcher, event_result->action.result_index);
    }
}

static void reach_launcher_capsule_handle_pointer(void *capsule, const reach_pointer_event *event,
                                                  reach_capsule_pointer_result *out)
{
    if (out != nullptr)
    {
        *out = {};
    }
    reach_launcher *launcher = static_cast<reach_launcher *>(capsule);
    if (launcher == nullptr || event == nullptr || out == nullptr ||
        !launcher->pointer_layout_valid)
    {
        return;
    }

    reach_launcher_event_context ctx = reach_launcher_capsule_event_context(launcher);
    reach_launcher_event_result event_result = {};
    reach_pointer_event mapped = *event;
    if (launcher->pointer_transform.scale_x > 0.0f && launcher->pointer_transform.scale_y > 0.0f)
    {
        mapped.x = (int32_t)lroundf(((float)event->x - launcher->pointer_transform.offset_x) /
                                    launcher->pointer_transform.scale_x);
        mapped.y = (int32_t)lroundf(((float)event->y - launcher->pointer_transform.offset_y) /
                                    launcher->pointer_transform.scale_y);
    }
    switch (mapped.kind)
    {
    case REACH_POINTER_EVENT_DOWN:
        reach_launcher_pointer_down(launcher, mapped.x, mapped.y, mapped.button, &ctx,
                                    &event_result);
        break;
    case REACH_POINTER_EVENT_UP:
        if (mapped.button == REACH_POINTER_BUTTON_PRIMARY &&
            launcher->state.launcher_scrollbar_drag.active)
        {
            reach_launcher_scrollbar_release(launcher, &event_result);
        }
        else
        {
            reach_launcher_pointer_up(launcher, mapped.x, mapped.y, mapped.button, &ctx,
                                      &event_result);
        }
        break;
    case REACH_POINTER_EVENT_MOVE:
        reach_launcher_pointer_move(launcher, mapped.x, mapped.y, &ctx, &event_result);
        break;
    case REACH_POINTER_EVENT_WHEEL:
        reach_launcher_wheel(launcher, mapped.x, mapped.y, mapped.wheel_delta, &ctx, &event_result);
        break;
    case REACH_POINTER_EVENT_CANCEL:
        reach_launcher_pointer_cancel(launcher, &event_result);
        break;
    case REACH_POINTER_EVENT_LEAVE:
        reach_launcher_pointer_leave(launcher, &event_result);
        break;
    case REACH_POINTER_EVENT_MIDDLE:
    default:
        return;
    }
    reach_launcher_capsule_apply_event_result(launcher, &event_result, out);
}

static void reach_launcher_capsule_surface_geometry(const void *capsule,
                                                    reach_feature_surface_geometry *out)
{
    if (out == nullptr)
    {
        return;
    }
    const reach_launcher *launcher = static_cast<const reach_launcher *>(capsule);
    if (launcher == nullptr || !launcher->pointer_layout_valid)
    {
        return;
    }

    const reach_launcher_layout *layout = &launcher->pointer_layout;
    out->visible_bounds = layout->bounds;
    out->envelope_bounds = layout->envelope_bounds;
    float collapsed_height = layout->search_box.height;
    float expanded_height = out->visible_bounds.height > collapsed_height
                                ? out->visible_bounds.height
                                : collapsed_height;
    out->visible_bounds.height = collapsed_height + (expanded_height - collapsed_height) *
                                                        reach_launcher_results_expansion(launcher);
}

const reach_feature_capsule_ops *reach_launcher_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {
        reach_launcher_capsule_reset,
        reach_launcher_capsule_tick,
        reach_launcher_capsule_is_open,
        nullptr,
        reach_launcher_capsule_needs_frame,
        reach_launcher_capsule_wants_pointer_move,
        reach_launcher_capsule_handle_pointer,
        reach_launcher_capsule_pointer_sequence_active,
        nullptr,
        reach_launcher_capsule_surface_geometry,
        reach_launcher_capsule_wants_pointer_move,
        reach_launcher_capsule_handle_event,
    };
    return &ops;
}
