#include "reach/features/launcher.h"

#include "launcher_common.h"

#include <new>

struct reach_launcher
{
    reach_launcher_state state;
    reach_search_service *search;
    uint32_t search_generation;
    reach_icon_service *icons;
    reach_launcher_layout pointer_layout;
    int32_t pointer_layout_valid;
    const reach_pinned_app_model *pointer_pinned_apps;
    size_t pointer_pinned_app_count;
};

void reach_launcher_set_pointer_context(reach_launcher *launcher,
                                        const reach_launcher_layout *layout,
                                        const reach_pinned_app_model *pinned_apps,
                                        size_t pinned_app_count)
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
    launcher->pointer_pinned_apps = pinned_apps;
    launcher->pointer_pinned_app_count = pinned_app_count;
}

void reach_launcher_attach_icons(reach_launcher *launcher, reach_icon_service *icons)
{
    if (launcher != nullptr)
    {
        launcher->icons = icons;
    }
}

reach_icon_service *reach_launcher_icons(reach_launcher *launcher)
{
    return launcher != nullptr ? launcher->icons : nullptr;
}

static size_t reach_launcher_strlen_utf16(const uint16_t *text)
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
    reach_scrollbar_model_init(&state->model.result_scrollbar, REACH_SCROLLBAR_DRAG_STEPPED, 1.0f);
    state->pressed_launcher_hit_type = REACH_LAUNCHER_HIT_NONE;
    state->pressed_launcher_index = REACH_MAX_PINNED_APPS;
    state->launcher_scrollbar_drag = reach_scrollbar_drag{};
    reach_text_edit_init(&state->launcher_text_edit, REACH_MAX_SEARCH_CHARS);
    state->launcher_focused = 0;
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

reach_result reach_launcher_toggle_state(reach_launcher_state *state)
{
    if (state == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    return state->model.open ? reach_launcher_close_state(state) : reach_launcher_open_state(state);
}

reach_result reach_launcher_set_query_state(reach_launcher_state *state, const uint16_t *query)
{
    if (state == 0 || query == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    size_t length = reach_launcher_strlen_utf16(query);
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
        state->model.results[index] = reach_search_candidate{};
    }
    state->model.result_count = 0;
    state->model.selected_result_index = 0;
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
        state->model.results[index] = results[index];
    }
    state->model.result_count = count;
    reach_scrollbar_set_extents(&state->model.result_scrollbar, (float)count,
                               (float)reach_launcher_visible_count(state));
    state->model.selected_result_index = 0;
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

reach_result reach_launcher_set_result_scroll_offset_state(reach_launcher_state *state, size_t offset)
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

int32_t reach_launcher_should_show_pinned_apps_state(const reach_launcher_state *state)
{
    (void)state;
    return 0;
}

reach_result reach_launcher_handle_event_state(reach_launcher_state *state, const reach_ui_event *event,
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
    case REACH_UI_EVENT_ESCAPE:
        return state->model.open ? reach_launcher_close_state(state) : REACH_OK;
    case REACH_UI_EVENT_ENTER:
        if (out_intent != 0 && state->model.open)
        {
            out_intent->type = REACH_UI_INTENT_OPEN_LAUNCHER_RESULT;
        }
        return REACH_OK;
    case REACH_UI_EVENT_ARROW_UP:
        return state->model.open ? reach_launcher_select_previous_result_state(state) : REACH_OK;
    case REACH_UI_EVENT_ARROW_DOWN:
        return state->model.open ? reach_launcher_select_next_result_state(state) : REACH_OK;
    case REACH_UI_EVENT_DOCK_APP_CLICK:
        if (out_intent != 0)
        {
            out_intent->type = REACH_UI_INTENT_LAUNCH_APP;
            out_intent->id = event->id;
        }
        return REACH_OK;
    case REACH_UI_EVENT_POINTER_WHEEL:
        return REACH_OK;
    case REACH_UI_EVENT_NONE:
    default:
        return REACH_OK;
    }
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

const reach_search_candidate *reach_launcher_result_at(reach_launcher *launcher, size_t index)
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

void reach_launcher_clear_query(reach_launcher *launcher)
{
    if (launcher != nullptr)
    {
        reach_launcher_state_mut(launcher)->model.query[0] = 0;
        reach_launcher_state_mut(launcher)->model.query_length = 0;
    }
}

reach_result reach_launcher_close(reach_launcher *launcher)
{
    return reach_launcher_close_state(reach_launcher_state_mut(launcher));
}

reach_result reach_launcher_toggle(reach_launcher *launcher)
{
    return reach_launcher_toggle_state(reach_launcher_state_mut(launcher));
}

reach_result reach_launcher_set_query(reach_launcher *launcher, const uint16_t *query)
{
    return reach_launcher_set_query_state(reach_launcher_state_mut(launcher), query);
}

reach_result reach_launcher_set_results(reach_launcher *launcher,
                                        const reach_search_candidate *results, size_t count)
{
    return reach_launcher_set_results_state(reach_launcher_state_mut(launcher), results, count);
}

reach_result reach_launcher_clear_results(reach_launcher *launcher)
{
    return reach_launcher_clear_results_state(reach_launcher_state_mut(launcher));
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
                                           reach_search_candidate *out_results, size_t *out_count)
{
    if (launcher == nullptr || launcher->search == nullptr || out_results == nullptr ||
        out_count == nullptr)
    {
        return 0;
    }
    uint32_t generation = 0;
    if (!reach_search_service_take_completed(launcher->search, &generation, out_results,
                                             out_count))
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

reach_result reach_launcher_handle_event(reach_launcher *launcher, const reach_ui_event *event,
                                         reach_ui_intent *out_intent)
{
    return reach_launcher_handle_event_state(reach_launcher_state_mut(launcher), event,
                                             out_intent);
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

void reach_launcher_clear_pressed(reach_launcher *launcher)
{
    if (launcher == nullptr)
    {
        return;
    }
    launcher->state.pressed_launcher_hit_type = REACH_LAUNCHER_HIT_NONE;
    launcher->state.pressed_launcher_index = REACH_MAX_PINNED_APPS;
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

void reach_launcher_set_focused(reach_launcher *launcher, int32_t focused)
{
    if (launcher != nullptr)
    {
        launcher->state.launcher_focused = focused ? 1 : 0;
    }
}

void reach_launcher_remember_restore_window(reach_launcher *launcher, uintptr_t window)
{
    if (launcher == nullptr)
    {
        return;
    }
    launcher->state.restore_window = window;
    launcher->state.restore_window_valid = window != 0;
}

void reach_launcher_clear_restore_window(reach_launcher *launcher)
{
    if (launcher == nullptr)
    {
        return;
    }
    launcher->state.restore_window = 0;
    launcher->state.restore_window_valid = 0;
}

uintptr_t reach_launcher_take_restore_window(reach_launcher *launcher)
{
    if (launcher == nullptr || !launcher->state.restore_window_valid)
    {
        return 0;
    }
    uintptr_t window = launcher->state.restore_window;
    reach_launcher_clear_restore_window(launcher);
    return window;
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
            (void)reach_launcher_clear_results_state(&launcher->state);
        }
        else
        {
            (void)reach_launcher_submit_search(launcher);
        }
        out_result->relayout = 1;
    }
}

static void reach_launcher_capsule_reset(void *capsule)
{
    reach_launcher *launcher = static_cast<reach_launcher *>(capsule);
    reach_launcher_clear_pressed(launcher);
    reach_launcher_reset_scrollbar_drag(launcher);
    if (launcher != nullptr)
    {
        launcher->pointer_layout_valid = 0;
    }
}

static void reach_launcher_capsule_tick(void *capsule, double delta_seconds,
                                        reach_feature_tick_result *out)
{
    if (reach_launcher_tick_caret(static_cast<reach_launcher *>(capsule), delta_seconds) &&
        out != nullptr)
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
    return launcher != nullptr &&
           reach_launcher_state_ptr(const_cast<reach_launcher *>(launcher))
               ->launcher_scrollbar_drag.active;
}

static reach_launcher_event_context reach_launcher_capsule_event_context(
    const reach_launcher *launcher)
{
    reach_launcher_event_context ctx = {};
    if (launcher != nullptr)
    {
        ctx.layout = launcher->pointer_layout_valid ? &launcher->pointer_layout : nullptr;
        ctx.pinned_apps = launcher->pointer_pinned_apps;
        ctx.pinned_app_count = launcher->pointer_pinned_app_count;
    }
    return ctx;
}

static void reach_launcher_capsule_apply_event_result(
    const reach_launcher_event_result *event_result, reach_capsule_pointer_result *out)
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
    if (event_result->action.type == REACH_LAUNCHER_ACTION_LAUNCH_PINNED)
    {
        out->action.kind = REACH_LAUNCHER_POINTER_ACTION_LAUNCH_PINNED;
        out->action.index = event_result->action.pinned_index;
        out->action.id = event_result->action.pin_id;
    }
    else if (event_result->action.type == REACH_LAUNCHER_ACTION_OPEN_RESULT)
    {
        out->action.kind = REACH_LAUNCHER_POINTER_ACTION_OPEN_RESULT;
    }
}

static int32_t reach_launcher_capsule_rect_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return rect.width > 0.0f && rect.height > 0.0f && (float)x >= rect.x &&
           (float)x <= rect.x + rect.width && (float)y >= rect.y &&
           (float)y <= rect.y + rect.height;
}

static void reach_launcher_capsule_handle_pointer(void *capsule,
                                                  const reach_pointer_event *event,
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
    switch (event->kind)
    {
    case REACH_POINTER_EVENT_DOWN:
        reach_launcher_pointer_down(launcher, event->x, event->y, &ctx, &event_result);
        break;
    case REACH_POINTER_EVENT_UP:
        if (launcher->state.launcher_scrollbar_drag.active)
        {
            reach_launcher_scrollbar_release(launcher, &event_result);
        }
        else
        {
            reach_launcher_pointer_up(launcher, event->x, event->y, &ctx, &event_result);
        }
        break;
    case REACH_POINTER_EVENT_MOVE:
        reach_launcher_scrollbar_drag_move(launcher, event->y, &ctx, &event_result);
        break;
    case REACH_POINTER_EVENT_WHEEL:
        reach_launcher_wheel(launcher, event->x, event->y, event->wheel_delta, &ctx,
                             &event_result);
        break;
    case REACH_POINTER_EVENT_CANCEL:
        reach_launcher_scrollbar_release(launcher, &event_result);
        reach_launcher_clear_pressed(launcher);
        break;
    case REACH_POINTER_EVENT_CONTEXT:
    {
        reach_launcher_hit_result hit = reach_launcher_hit_test(
            &launcher->state.model, &launcher->pointer_layout, event->x, event->y);
        if (hit.type == REACH_LAUNCHER_HIT_SEARCH_RESULT &&
            hit.index < launcher->state.model.result_count)
        {
            out->handled = 1;
            if (launcher->state.model.results[hit.index].kind == REACH_SEARCH_RESULT_APP)
            {
                out->action.kind = REACH_LAUNCHER_POINTER_ACTION_REVEAL_RESULT;
                out->action.index = hit.index;
            }
            return;
        }
        out->handled = reach_launcher_capsule_rect_contains(launcher->pointer_layout.bounds,
                                                            event->x, event->y);
        return;
    }
    case REACH_POINTER_EVENT_LEAVE:
    case REACH_POINTER_EVENT_MIDDLE:
    default:
        return;
    }
    reach_launcher_capsule_apply_event_result(&event_result, out);
}

const reach_feature_capsule_ops *reach_launcher_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {
        reach_launcher_capsule_reset,
        reach_launcher_capsule_tick,
        reach_launcher_capsule_is_open,
        nullptr  ,
        nullptr  ,
        reach_launcher_capsule_needs_frame,
        reach_launcher_capsule_wants_pointer_move,
        reach_launcher_capsule_handle_pointer,
    };
    return &ops;
}
