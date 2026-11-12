#include "reach/features/launcher.h"

#include "launcher_common.h"

#include <math.h>
#include <new>

#include "reach/features/common/presentation.h"
#include "reach/core/blink.h"
#include "reach/support/util.h"

struct reach_launcher
{
    reach_motion body_height;
    reach_motion results_opacity;
    double results_deadline;
    reach_motion_spec height_spec;
    reach_motion_spec fade_in_spec;
    reach_motion_spec fade_out_spec;
    reach_theme theme;
    float dpi_scale;
    float row_pitch;
    reach_keyed_row rows[REACH_LAUNCHER_ROW_CAPACITY];
    reach_launcher_result row_results[REACH_LAUNCHER_ROW_CAPACITY];
    reach_selection_pill selection;
    int32_t selection_valid;
    reach_motion row_selected[REACH_LAUNCHER_ROW_CAPACITY];
    reach_motion press_feedback;
    reach_motion hover_opacity;
    size_t hover_index;
    int32_t hover_valid;
    reach_motion search_icon_opacity;
    uint32_t results_revision;
    uint32_t synced_revision;
    size_t synced_scroll;
    size_t synced_first;
    reach_scroll_view scroll;
    size_t synced_selected;
    float synced_pitch;
    reach_feature_transition surface_transition;
    reach_blink caret;
    int32_t caret_retained;
    reach_launcher_state state;
    reach_search_service *search;
    uint32_t search_generation;
    reach_icon_service *icons;
    uint16_t terminal_icon_ref[REACH_SEARCH_RESULT_PATH_CAPACITY];
    reach_launcher_layout pointer_layout;
    int32_t pointer_layout_valid;
};

static int32_t reach_launcher_results_attached(const reach_launcher_state *state)
{
    return state != nullptr && (state->model.result_count > 0 ||
                                (state->model.search_error && state->model.query_length > 0));
}

reach_motion reach_launcher_body_height_motion(const reach_launcher *launcher)
{
    return launcher != nullptr ? launcher->body_height : reach_motion{};
}

reach_motion reach_launcher_results_opacity_motion(const reach_launcher *launcher)
{
    return launcher != nullptr ? launcher->results_opacity : reach_motion{};
}

static void reach_launcher_load_motion_specs(reach_launcher *launcher, const reach_theme *theme)
{
    launcher->height_spec = reach_theme_motion_preset(theme, REACH_MOTION_PRESET_SMOOTH);
    launcher->fade_in_spec = reach_theme_motion_preset(theme, REACH_MOTION_PRESET_FADE_IN);
    launcher->fade_out_spec = reach_theme_motion_preset(theme, REACH_MOTION_PRESET_FADE_OUT);
    launcher->theme = *theme;
}

static int32_t reach_launcher_results_animated(const reach_launcher *launcher)
{
    return launcher->state.model.open && launcher->pointer_layout_valid &&
           reach_feature_transition_visible(&launcher->surface_transition);
}

reach_pressable_feedback_style reach_launcher_pressable_feedback(reach_launcher *launcher)
{
    reach_pressable_feedback_style feedback = {};
    feedback.motion = launcher != nullptr ? &launcher->press_feedback : nullptr;
    feedback.pressed_value = 1.0f;
    return feedback;
}

static int32_t reach_launcher_fade_to(reach_launcher *launcher, reach_motion *motion,
                                      int32_t visible)
{
    float target = visible ? 1.0f : 0.0f;
    if (motion->generation != 0 && motion->to == target)
        return 0;
    if (!reach_launcher_results_animated(launcher))
        reach_motion_set(motion, target);
    else
        reach_motion_animate_spec_to_at(motion, target,
                                        visible ? launcher->fade_in_spec : launcher->fade_out_spec,
                                        reach_motion_now_seconds());
    return 1;
}

static void reach_launcher_sync_search_icon(reach_launcher *launcher)
{
    (void)reach_launcher_fade_to(launcher, &launcher->search_icon_opacity,
                                 launcher->state.model.query[0] == 0);
}

static int32_t reach_launcher_set_hover(reach_launcher *launcher, size_t index, int32_t hovered)
{
    int32_t changed = 0;
    if (hovered && (!launcher->hover_valid || launcher->hover_index != index))
    {
        launcher->hover_index = index;
        launcher->hover_valid = 1;
        changed = 1;
    }
    return reach_launcher_fade_to(launcher, &launcher->hover_opacity, hovered) || changed;
}

static void reach_launcher_refresh_results_deadline(reach_launcher *launcher)
{
    launcher->results_deadline = 0;
    if (reach_motion_active(&launcher->body_height))
        launcher->results_deadline = reach_motion_deadline(&launcher->body_height);
    if (reach_motion_active(&launcher->results_opacity))
        launcher->results_deadline =
            fmax(launcher->results_deadline, reach_motion_deadline(&launcher->results_opacity));
    if (reach_scroll_view_active(&launcher->scroll))
        launcher->results_deadline =
            fmax(launcher->results_deadline, reach_scroll_view_deadline(&launcher->scroll));
    if (reach_keyed_rows_active(launcher->rows, REACH_LAUNCHER_ROW_CAPACITY))
        launcher->results_deadline =
            fmax(launcher->results_deadline,
                 reach_keyed_rows_deadline(launcher->rows, REACH_LAUNCHER_ROW_CAPACITY));
    const reach_motion *selection[] = {&launcher->selection.rect.x, &launcher->selection.rect.y,
                                       &launcher->selection.rect.width,
                                       &launcher->selection.rect.height};
    for (const reach_motion *motion : selection)
    {
        if (reach_motion_active(motion))
            launcher->results_deadline =
                fmax(launcher->results_deadline, reach_motion_deadline(motion));
    }
}

struct reach_launcher_row_match
{
    const reach_launcher *launcher;
    size_t start;
};

static int32_t reach_launcher_row_same(const void *context, size_t slot, size_t item_index)
{
    const reach_launcher_row_match *match = static_cast<const reach_launcher_row_match *>(context);
    const reach_launcher_result &held = match->launcher->row_results[slot];
    const reach_launcher_result &item =
        match->launcher->state.model.results[match->start + item_index];
    if (held.action != item.action)
        return 0;
    if (item.action != REACH_LAUNCHER_RESULT_OPEN_SEARCH)
        return 1;
    return reach_utf16_equal(held.payload.search.path, item.payload.search.path) &&
           reach_utf16_equal(held.payload.search.arguments, item.payload.search.arguments) &&
           reach_utf16_equal(held.payload.search.app_user_model_id,
                             item.payload.search.app_user_model_id);
}

static int32_t reach_launcher_rows_held(const reach_launcher *launcher)
{
    for (const reach_keyed_row &row : launcher->rows)
    {
        if (row.lifecycle != REACH_KEYED_ROW_EMPTY)
            return 1;
    }
    return 0;
}

static reach_rect_f32 reach_launcher_selection_rect(const reach_launcher *launcher)
{
    const reach_launcher_layout &layout = launcher->pointer_layout;
    float scale = launcher->dpi_scale;
    reach_rect_f32 rect = {};
    rect.x = layout.search_result_items.x - layout.bounds.x + 6.0f * scale;
    rect.y = layout.search_result_items.y - layout.bounds.y +
             (float)launcher->state.model.selected_result_index * launcher->row_pitch +
             5.0f * scale;
    rect.width = fmaxf(0.0f, layout.search_result_items.width - 12.0f * scale);
    rect.height = fmaxf(0.0f, launcher->row_pitch - 10.0f * scale);
    return rect;
}

static void reach_launcher_sync_rows(reach_launcher *launcher)
{
    if (launcher == nullptr || !launcher->pointer_layout_valid || launcher->row_pitch <= 0.0f)
        return;
    const reach_launcher_model &model = launcher->state.model;
    size_t start = reach_launcher_model_result_scroll_offset(&model);
    if (start > model.result_count)
        start = model.result_count;
    size_t count = reach_launcher_visible_result_count(&model);
    if (count > model.result_count - start)
        count = model.result_count - start;
    if (count > REACH_LAUNCHER_ROW_CAPACITY)
        count = REACH_LAUNCHER_ROW_CAPACITY;
    int32_t results_changed = launcher->synced_revision != launcher->results_revision;
    int32_t pitch_changed = launcher->synced_pitch != launcher->row_pitch;
    int32_t scroll_changed = launcher->synced_scroll != start;
    int32_t selected_changed = launcher->synced_selected != model.selected_result_index;
    if (!results_changed && !pitch_changed && !scroll_changed && !selected_changed)
        return;
    double now = reach_motion_now_seconds();
    int32_t animated = !pitch_changed && reach_launcher_results_animated(launcher);
    reach_keyed_row_change changes[REACH_KEYED_ROWS_MAX_CHANGES(REACH_LAUNCHER_ROW_CAPACITY)];
    (void)reach_keyed_rows_release_settled(launcher->rows, REACH_LAUNCHER_ROW_CAPACITY, changes,
                                           REACH_LAUNCHER_ROW_CAPACITY);
    reach_scroll_view_mode scroll_mode = REACH_SCROLL_VIEW_SET;
    if (animated && (results_changed || selected_changed))
        scroll_mode = REACH_SCROLL_VIEW_GLIDE;
    else if (animated && !launcher->state.launcher_scrollbar_drag.active)
        scroll_mode = REACH_SCROLL_VIEW_WHEEL;
    (void)reach_scroll_view_retarget(&launcher->scroll, &launcher->theme,
                                     (float)start * launcher->row_pitch, scroll_mode, now);
    reach_scroll_view_window window =
        reach_scroll_view_item_window(&launcher->scroll, launcher->row_pitch, model.result_count,
                                      count, 1, REACH_LAUNCHER_ROW_CAPACITY - 4);
    size_t first = window.first;
    if (results_changed || pitch_changed || scroll_changed || first != launcher->synced_first)
    {
        count = window.count;
        float targets[REACH_LAUNCHER_ROW_CAPACITY] = {};
        for (size_t index = 0; index < count; ++index)
            targets[index] = (float)(first + index) * launcher->row_pitch;
        reach_launcher_row_match match = {launcher, first};
        reach_keyed_rows_sync_input input = {};
        input.count = count;
        input.targets = targets;
        input.same = reach_launcher_row_same;
        input.context = &match;
        input.animate = animated;
        input.theme = &launcher->theme;
        input.now_seconds = now;
        (void)reach_keyed_rows_sync(launcher->rows, REACH_LAUNCHER_ROW_CAPACITY, &input, changes,
                                    REACH_KEYED_ROWS_MAX_CHANGES(REACH_LAUNCHER_ROW_CAPACITY));
        for (size_t slot = 0; slot < REACH_LAUNCHER_ROW_CAPACITY; ++slot)
        {
            const reach_keyed_row &row = launcher->rows[slot];
            if (row.lifecycle == REACH_KEYED_ROW_LIVE && first + row.index < model.result_count)
                launcher->row_results[slot] = model.results[first + row.index];
        }
        launcher->synced_first = first;
    }
    first = launcher->synced_first;
    if (model.result_count > 0)
    {
        reach_selection_pill_mode mode = REACH_SELECTION_PILL_SET;
        if (animated && launcher->selection_valid)
            mode = results_changed ? REACH_SELECTION_PILL_JUMP
                                   : reach_selection_pill_mode_for_distance(
                                         launcher->synced_selected, model.selected_result_index);
        reach_selection_pill_retarget(&launcher->selection, &launcher->theme,
                                      reach_launcher_selection_rect(launcher),
                                      model.selected_result_index, mode, now);
        launcher->selection_valid = 1;
    }
    else if (!reach_launcher_rows_held(launcher))
        launcher->selection_valid = 0;
    for (size_t slot = 0; slot < REACH_LAUNCHER_ROW_CAPACITY; ++slot)
    {
        const reach_keyed_row &row = launcher->rows[slot];
        int32_t selected = row.lifecycle == REACH_KEYED_ROW_LIVE &&
                           first + row.index == model.selected_result_index;
        if (row.lifecycle == REACH_KEYED_ROW_EMPTY || !animated)
            reach_motion_set(&launcher->row_selected[slot], selected ? 1.0f : 0.0f);
        else
            (void)reach_launcher_fade_to(launcher, &launcher->row_selected[slot], selected);
    }
    launcher->synced_revision = launcher->results_revision;
    launcher->synced_pitch = launcher->row_pitch;
    launcher->synced_scroll = start;
    launcher->synced_selected = model.selected_result_index;
    reach_launcher_refresh_results_deadline(launcher);
}

void reach_launcher_fill_row_render_input(const reach_launcher *launcher,
                                          reach_launcher_render_input *input)
{
    if (launcher == nullptr || input == nullptr)
        return;
    input->rows = launcher->rows;
    input->row_results = launcher->row_results;
    input->row_start = launcher->synced_first;
    input->row_pitch = launcher->row_pitch;
    input->content_offset = reach_scroll_view_content_offset(&launcher->scroll);
    reach_motion_set(&input->scrollbar_thumb_offset, 0.0f);
    const reach_launcher_layout &layout = launcher->pointer_layout;
    float travel =
        layout.search_result_scrollbar_track.height - layout.search_result_scrollbar_thumb.height;
    float maximum = launcher->state.model.result_scrollbar.maximum * launcher->row_pitch;
    if (launcher->pointer_layout_valid && travel > 0.0f && maximum > 0.0f)
    {
        float ratio = travel / maximum;
        input->scrollbar_thumb_offset = reach_motion_affine(
            &launcher->scroll.offset, ratio, -ratio * reach_scroll_view_target(&launcher->scroll));
    }
    input->selection = launcher->selection.rect;
    input->has_selection = launcher->selection_valid;
    input->row_selected = launcher->row_selected;
    input->press_feedback = launcher->press_feedback;
    input->press_index = reach_pressable_feedback_index(&launcher->state.pressable);
    input->has_press = input->press_index < launcher->state.model.result_count &&
                       launcher->press_feedback.generation != 0;
    input->hover_opacity = launcher->hover_opacity;
    input->hover_index = launcher->hover_index;
    input->has_hover =
        launcher->hover_valid && launcher->hover_index < launcher->state.model.result_count;
    input->search_icon_opacity = launcher->search_icon_opacity;
}

static void reach_launcher_sync_results_opacity(reach_launcher *launcher, int32_t was_attached)
{
    if (launcher == nullptr)
    {
        return;
    }
    int32_t attached = reach_launcher_results_attached(&launcher->state);
    float target = attached ? 1.0f : 0.0f;
    launcher->results_revision += 1;
    if (!reach_launcher_results_animated(launcher))
        reach_motion_set(&launcher->results_opacity, target);
    else if (attached != was_attached || launcher->results_opacity.to != target)
        reach_motion_animate_spec_to_at(&launcher->results_opacity, target,
                                        attached ? launcher->fade_in_spec : launcher->fade_out_spec,
                                        reach_motion_now_seconds());
    reach_launcher_refresh_results_deadline(launcher);
}

static void reach_launcher_sync_body_height(reach_launcher *launcher, float target, int32_t animate)
{
    REACH_ASSERT(isfinite(target) && target >= 0.0f);
    if (!animate || launcher->body_height.generation == 0)
        reach_motion_set(&launcher->body_height, target);
    else if (launcher->body_height.to != target)
        reach_motion_animate_spec_to_at(&launcher->body_height, target, launcher->height_spec,
                                        reach_motion_now_seconds());
    reach_launcher_refresh_results_deadline(launcher);
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
    reach_feature_transition_configure(&launcher->surface_transition, ctx->theme, ctx->dpi_scale,
                                       REACH_FEATURE_TRANSITION_FROM_ABOVE);
    reach_launcher_load_motion_specs(launcher,
                                     ctx->theme != nullptr ? ctx->theme : reach_theme_default());

    float scale = ctx->dpi_scale > 0.0f ? ctx->dpi_scale : 1.0f;
    float border = reach_theme_border_thickness(
        ctx->theme != nullptr ? ctx->theme : reach_theme_default(), scale);
    float available_results_height = ctx->monitor_bounds.height - 48.0f * scale -
                                     (52.0f * scale + border * 2.0f) - 16.0f * scale;
    size_t visible_capacity =
        available_results_height > 0.0f ? (size_t)(available_results_height / (56.0f * scale)) : 1;
    if (visible_capacity < 1)
    {
        visible_capacity = 1;
    }
    if (visible_capacity > REACH_SEARCH_VISIBLE_RESULTS)
    {
        visible_capacity = REACH_SEARCH_VISIBLE_RESULTS;
    }
    launcher->state.model.visible_result_capacity = visible_capacity;

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
    int32_t animate = reach_launcher_results_animated(launcher) &&
                      reach_rect_equal(launcher->pointer_layout.search_box, layout.search_box);
    reach_launcher_set_pointer_context(launcher, &layout);
    launcher->dpi_scale = scale;
    launcher->row_pitch = 56.0f * scale;
    reach_launcher_sync_body_height(launcher, layout.bounds.height, animate);
    reach_launcher_sync_rows(launcher);
    reach_launcher_sync_search_icon(launcher);
    return changed;
}

reach_result
reach_launcher_append_surface_render_commands(reach_launcher *launcher, const reach_theme *theme,
                                              float dpi_scale, int32_t retained_supported,
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
    render.retained_supported = retained_supported;
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
    size_t capacity = state->model.visible_result_capacity > 0
                          ? state->model.visible_result_capacity
                          : REACH_SEARCH_VISIBLE_RESULTS;
    return state->model.result_count < capacity ? state->model.result_count : capacity;
}

static size_t reach_launcher_max_scroll_offset(const reach_launcher_state *state)
{
    size_t capacity = state != 0 && state->model.visible_result_capacity > 0
                          ? state->model.visible_result_capacity
                          : REACH_SEARCH_VISIBLE_RESULTS;
    if (state == 0 || state->model.result_count <= capacity)
    {
        return 0;
    }
    return state->model.result_count - capacity;
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
    reach_feature_transition_init(&launcher->surface_transition, REACH_FEATURE_PRESENTATION_POPUP,
                                  REACH_FEATURE_TRANSITION_FROM_ABOVE);
    reach_launcher_load_motion_specs(launcher, reach_theme_default());
    reach_motion_set(&launcher->body_height, 0);
    reach_motion_set(&launcher->results_opacity, 0);
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
        reach_blink_stop(&launcher->caret);
    }
    (void)reach_feature_transition_set_open(&launcher->surface_transition, open);
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
    reach_motion_set(&launcher->results_opacity, 0);
    if (launcher->pointer_layout_valid)
        reach_motion_set(&launcher->body_height, launcher->pointer_layout.search_box.height);
    for (reach_keyed_row &row : launcher->rows)
        row = {};
    reach_scroll_view_init(&launcher->scroll);
    launcher->synced_first = 0;
    launcher->selection_valid = 0;
    launcher->hover_valid = 0;
    reach_motion_set(&launcher->hover_opacity, 0);
    reach_motion_set(&launcher->search_icon_opacity, 1);
    launcher->synced_pitch = 0.0f;
    launcher->results_deadline = 0;
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
        reach_launcher_sync_results_opacity(launcher, was_attached);
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
        reach_launcher_sync_results_opacity(launcher, was_attached);
    }
    return result;
}

void reach_launcher_set_search_error(reach_launcher *launcher, int32_t error)
{
    if (launcher != nullptr)
    {
        int32_t was_attached = reach_launcher_results_attached(&launcher->state);
        launcher->state.model.search_error = error ? 1 : 0;
        reach_launcher_sync_results_opacity(launcher, was_attached);
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
        REACH_UI_EVENT_ESCAPE,     REACH_UI_EVENT_ENTER,     REACH_UI_EVENT_ARROW_UP,
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
    if (launcher->state.model.open)
        reach_blink_reset_at(&launcher->caret, REACH_LAUNCHER_CARET_BLINK_SECONDS,
                             REACH_LAUNCHER_CARET_BLINK_SECONDS, reach_motion_now_seconds());
    else
        reach_blink_stop(&launcher->caret);
    launcher->state.launcher_caret_visible = 1;
}

static int32_t reach_launcher_update_caret(reach_launcher *launcher)
{
    if (launcher == nullptr)
        return 0;
    if (!launcher->state.model.open)
    {
        reach_blink_stop(&launcher->caret);
        launcher->state.launcher_caret_visible = 1;
        return 0;
    }
    if (launcher->caret.deadline_seconds <= 0.0)
        reach_launcher_reset_caret(launcher);
    if (launcher->caret_retained)
        return 0;
    int32_t changed = reach_blink_update_at(&launcher->caret, reach_motion_now_seconds());
    launcher->state.launcher_caret_visible = launcher->caret.visible;
    return changed;
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
            reach_launcher_sync_results_opacity(launcher, was_attached);
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
        reach_pressable_feedback_style feedback = reach_launcher_pressable_feedback(launcher);
        reach_pressable_reset(&launcher->state.pressable, &feedback);
        launcher->pointer_layout_valid = 0;
        reach_motion_set(&launcher->results_opacity,
                         reach_launcher_results_attached(&launcher->state) ? 1.0f : 0.0f);
        reach_motion_set(&launcher->body_height, launcher->body_height.to);
        launcher->synced_pitch = 0.0f;
        launcher->results_deadline = 0;
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

static void reach_launcher_capsule_tick(void *capsule, double, reach_feature_tick_result *out)
{
    reach_launcher *launcher = static_cast<reach_launcher *>(capsule);
    if (out != nullptr && reach_launcher_drain_search_results(launcher))
    {
        out->redraw = 1;
        out->relayout = 1;
        out->request_update = 1;
    }
    int32_t surface_was_visible =
        launcher != nullptr && reach_feature_transition_visible(&launcher->surface_transition);
    int32_t expansion_complete = 0;
    if (launcher != nullptr)
    {
        if (launcher->results_deadline > 0 && reach_motion_wait_ms(launcher->results_deadline) == 0)
        {
            reach_motion_set(&launcher->body_height, launcher->body_height.to);
            reach_motion_set(&launcher->results_opacity, launcher->results_opacity.to);
            reach_scroll_view_settle(&launcher->scroll);
            reach_keyed_row_change released[REACH_LAUNCHER_ROW_CAPACITY];
            (void)reach_keyed_rows_release_settled(launcher->rows, REACH_LAUNCHER_ROW_CAPACITY,
                                                   released, REACH_LAUNCHER_ROW_CAPACITY);
            if (launcher->state.model.result_count == 0 && !reach_launcher_rows_held(launcher))
                launcher->selection_valid = 0;
            reach_launcher_refresh_results_deadline(launcher);
            expansion_complete = 1;
        }
        (void)reach_feature_transition_tick(&launcher->surface_transition, 0.0);
    }
    int32_t surface_visible =
        launcher != nullptr && reach_feature_transition_visible(&launcher->surface_transition);
    if (out != nullptr && (reach_launcher_update_caret(launcher) || expansion_complete ||
                           surface_was_visible != surface_visible))
        out->redraw = 1;
    if (out != nullptr && expansion_complete)
        out->request_update = 1;
}

static int32_t reach_launcher_capsule_is_open(const void *capsule)
{
    return reach_launcher_is_open(
        const_cast<reach_launcher *>(static_cast<const reach_launcher *>(capsule)));
}

static int32_t reach_launcher_capsule_presentation_visible(const void *capsule)
{
    const reach_launcher *launcher = static_cast<const reach_launcher *>(capsule);
    return launcher != nullptr && reach_feature_transition_visible(&launcher->surface_transition);
}

static int32_t reach_launcher_capsule_needs_frame(const void *)
{
    return 0;
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
    return state->model.open || state->launcher_scrollbar_drag.active ||
           reach_pressable_tracking(&state->pressable);
}

static int32_t reach_launcher_capsule_pointer_sequence_active(const void *capsule)
{
    return reach_launcher_capsule_wants_pointer_move(capsule);
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
        if (search->path[0] == 0 && search->app_user_model_id[0] == 0)
        {
            return target;
        }
        target.kind = search->kind == REACH_SEARCH_RESULT_APP ? REACH_FEATURE_TARGET_APP
                                                              : REACH_FEATURE_TARGET_PATH;
        target.path = search->path;
        target.arguments = search->arguments[0] != 0 ? search->arguments : nullptr;
        target.app_user_model_id =
            search->app_user_model_id[0] != 0 ? search->app_user_model_id : nullptr;
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
    if (launcher == nullptr || event == nullptr || out == nullptr || !launcher->state.model.open)
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
        reach_result result = event->type == REACH_UI_EVENT_ARROW_UP
                                  ? reach_launcher_select_previous_result_state(&launcher->state)
                                  : reach_launcher_select_next_result_state(&launcher->state);
        if (result == REACH_OK)
        {
            if (launcher->synced_revision == launcher->results_revision)
                reach_launcher_sync_rows(launcher);
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

    auto layout = launcher->pointer_layout;
    double now = reach_motion_now_seconds();
    float height = reach_motion_value_at(&launcher->body_height, now);
    float revealed = reach_motion_value_at(&launcher->results_opacity, now);
    layout.bounds.height = height;
    layout.search_results.height =
        revealed > 0 ? fmaxf(0, height - (layout.search_results.y - layout.bounds.y)) : 0;
    reach_launcher_event_context ctx = {};
    ctx.layout = &layout;
    reach_launcher_event_result event_result = {};
    reach_pointer_event mapped = *event;
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
    default:
        return;
    }
    if (launcher->synced_revision == launcher->results_revision)
        reach_launcher_sync_rows(launcher);
    int32_t hover_changed = 0;
    if (mapped.kind == REACH_POINTER_EVENT_LEAVE || mapped.kind == REACH_POINTER_EVENT_CANCEL)
        hover_changed = reach_launcher_set_hover(launcher, 0, 0);
    else if (mapped.kind != REACH_POINTER_EVENT_WHEEL)
    {
        reach_launcher_hit_result hover =
            reach_launcher_hit_test(&launcher->state.model, &layout, mapped.x, mapped.y);
        hover_changed =
            reach_launcher_set_hover(launcher, hover.index,
                                     hover.type == REACH_LAUNCHER_HIT_SEARCH_RESULT &&
                                         hover.index < launcher->state.model.result_count);
    }
    event_result.redraw = event_result.redraw || hover_changed;
    reach_launcher_capsule_apply_event_result(launcher, &event_result, out);
}

static void reach_launcher_capsule_surface_geometry(const void *capsule,
                                                    reach_feature_surface_geometry *out)
{
    if (out == nullptr)
    {
        return;
    }
    *out = {};
    const reach_launcher *launcher = static_cast<const reach_launcher *>(capsule);
    if (launcher == nullptr || !launcher->pointer_layout_valid)
    {
        return;
    }

    const reach_launcher_layout *layout = &launcher->pointer_layout;
    out->visible_bounds = layout->bounds;
    float minimum_height = 0, maximum_height = 0;
    if (reach_motion_range(&launcher->body_height, &minimum_height, &maximum_height) == REACH_OK &&
        maximum_height > out->visible_bounds.height)
        out->visible_bounds.height = maximum_height;
    out->envelope_bounds = layout->envelope_bounds;
    out->has_input_bounds = 1;
    out->input_bounds = reach_launcher_body_motion(layout, &launcher->body_height);
    reach_feature_transition_presentation(&launcher->surface_transition, out);
    if (launcher->results_deadline > 0 &&
        (out->deadline_seconds <= 0 || launcher->results_deadline < out->deadline_seconds))
        out->deadline_seconds = launcher->results_deadline;
    if (!launcher->caret_retained && launcher->state.model.open &&
        launcher->caret.deadline_seconds > 0.0 &&
        (out->deadline_seconds <= 0.0 || launcher->caret.deadline_seconds < out->deadline_seconds))
    {
        out->deadline_seconds = launcher->caret.deadline_seconds;
    }
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
        nullptr,
        reach_launcher_capsule_presentation_visible,
    };
    return &ops;
}

reach_motion reach_launcher_caret_opacity(const reach_launcher *launcher)
{
    return reach_blink_opacity(launcher != nullptr ? &launcher->caret : nullptr);
}

void reach_launcher_set_retained_supported(reach_launcher *launcher, int32_t supported)
{
    if (launcher != nullptr)
        launcher->caret_retained = supported != 0;
}
