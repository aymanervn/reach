#include "reach/features/common/ui_controls.h"

#include "reach/support/util.h"

#include <math.h>

static void reach_ui_push_rect(reach_render_command_buffer *commands, reach_rect_f32 rect,
                               float radius, reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = rect;
    command.radius = radius;
    command.color = color;
    (void)reach_render_command_buffer_push(commands, &command);
}

reach_result reach_ui_feedback_begin(reach_render_command_buffer *commands, uint64_t id,
                                     reach_rect_f32 bounds, const reach_motion *opacity,
                                     const reach_render_group *placement)
{
    if (commands == nullptr || opacity == nullptr)
        return REACH_INVALID_ARGUMENT;
    reach_render_group group = placement != nullptr ? *placement : reach_render_group{};
    group.id = id;
    if (placement == nullptr)
    {
        group.bounds = bounds;
        reach_motion_set(&group.scale, 1.0f);
    }
    group.has_tint = 0;
    group.opacity = *opacity;
    if (group.opacity.generation == 0)
        reach_motion_set(&group.opacity, reach_motion_value(opacity));
    return reach_render_command_buffer_begin_group(commands, &group);
}

reach_result reach_ui_feedback_render(reach_render_command_buffer *commands, uint64_t id,
                                      reach_rect_f32 bounds, float radius, reach_color color,
                                      const reach_motion *opacity,
                                      const reach_render_group *placement)
{
    reach_result result = reach_ui_feedback_begin(commands, id, bounds, opacity, placement);
    if (result != REACH_OK)
        return result;
    reach_render_command overlay = {};
    overlay.type = REACH_RENDER_COMMAND_RECT;
    overlay.rect = bounds;
    overlay.radius = radius;
    overlay.color = color;
    result = reach_render_command_buffer_push(commands, &overlay);
    reach_render_command_buffer_end_group(commands);
    return result;
}

static void reach_ui_push_text(reach_render_command_buffer *commands, reach_rect_f32 rect,
                               const uint16_t *text, float size, int32_t weight, int32_t alignment,
                               reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = rect;
    command.text_size = size;
    command.text_weight = weight;
    command.text_alignment = alignment;
    command.text_ellipsis = 1;
    command.color = color;
    reach_copy_utf16(command.text, 260, text);
    (void)reach_render_command_buffer_push(commands, &command);
}

static reach_render_group reach_ui_color_group(uint64_t id, reach_rect_f32 bounds, reach_color from,
                                               reach_color to, const reach_motion *progress)
{
    reach_render_group group = {};
    group.id = id;
    group.bounds = bounds;
    group.has_tint = 1;
    float source[] = {from.r, from.g, from.b, from.a};
    float target[] = {to.r, to.g, to.b, to.a};
    for (size_t index = 0; index < REACH_RENDER_COLOR_CHANNEL_COUNT; ++index)
        group.tint[index] =
            reach_motion_affine(progress, target[index] - source[index], source[index]);
    reach_motion_set(&group.opacity, 1.0f);
    reach_motion_set(&group.scale, 1.0f);
    return group;
}

int32_t reach_ui_color_feedback_begin(reach_render_command_buffer *commands, uint64_t id,
                                      reach_rect_f32 bounds, reach_color from, reach_color to,
                                      const reach_motion *pressed)
{
    if (pressed == nullptr)
        return 0;
    reach_render_group group = reach_ui_color_group(id, bounds, from, to, pressed);
    return reach_render_command_buffer_begin_group(commands, &group) == REACH_OK;
}

reach_result reach_ui_bordered_color_feedback_render(reach_render_command_buffer *commands,
                                                     uint64_t first_id, reach_rect_f32 bounds,
                                                     float radius, float border, reach_color from,
                                                     reach_color to, reach_color outline,
                                                     const reach_motion *progress)
{
    if (commands == nullptr || progress == nullptr || first_id == 0 ||
        first_id > UINT64_C(0x7ffffffffffffffb))
        return REACH_INVALID_ARGUMENT;
    if (bounds.width <= 0 || bounds.height <= 0)
        return REACH_OK;
    uint64_t parent_id = commands->current_group_id;
    reach_motion hover = reach_motion_clamp(progress, 0, 1);
    from.a = to.a = 1;
    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_RECT;
    shape.rect = bounds;
    shape.radius = radius;
    reach_render_command shapes[2];
    size_t count =
        reach_render_bordered_background_shapes(&shape, {1, 1, 1, 1}, {1, 1, 1, 1}, border, shapes);
    for (size_t index = 0; index < count; ++index)
    {
        reach_rect_f32 source = {};
        if (!reach_render_retained_command_bounds(&shapes[index], &source))
            return REACH_INVALID_ARGUMENT;
        reach_render_group group = reach_ui_color_group(first_id + index, source, from, to, &hover);
        group.parent_id = parent_id;
        if (index == 0 && border > 0)
        {
            group.tint_rgb_target = outline;
            reach_motion weight = reach_motion_affine(&hover, outline.a, 0);
            group.tint_rgb_mix = reach_motion_clamp(&weight, 0, 1);
        }
        reach_result result = reach_render_command_buffer_begin_group(commands, &group);
        if (result != REACH_OK)
            return result;
        result = reach_render_command_buffer_push(commands, &shapes[index]);
        reach_render_command_buffer_end_group(commands);
        commands->current_group_id = parent_id;
        if (result != REACH_OK)
            return result;
    }
    return REACH_OK;
}

void reach_ui_button_render(reach_render_command_buffer *commands, reach_rect_f32 bounds,
                            const uint16_t *label, const reach_ui_button_style *style,
                            int32_t enabled, uint64_t feedback_id, const reach_motion *pressed)
{
    if (commands == nullptr || style == nullptr)
    {
        return;
    }
    reach_color background = enabled ? style->background : style->disabled_background;
    const float darken = style->pressed_darken > 0.0f ? style->pressed_darken : 1.0f;
    reach_color target = style->has_pressed_colors
                             ? style->pressed_background
                             : reach_color{background.r * darken, background.g * darken,
                                           background.b * darken, background.a};
    const reach_motion *feedback = enabled || style->has_pressed_colors ? pressed : nullptr;
    int32_t retained =
        reach_ui_color_feedback_begin(commands, feedback_id, bounds, background, target, feedback);
    reach_color white = {1.0f, 1.0f, 1.0f, 1.0f};
    reach_ui_push_rect(commands, bounds, style->radius, retained ? white : background);
    if (retained)
        reach_render_command_buffer_end_group(commands);
    reach_color text = enabled ? style->text : style->disabled_text;
    retained = style->has_pressed_colors &&
               reach_ui_color_feedback_begin(commands, feedback_id + 1, bounds, text,
                                             style->pressed_text, feedback);
    reach_ui_push_text(commands, bounds, label, style->text_size, style->text_weight,
                       REACH_TEXT_ALIGNMENT_CENTER, retained ? white : text);
    if (retained)
        reach_render_command_buffer_end_group(commands);
}

void reach_ui_selection_item_backdrop_render(reach_render_command_buffer *commands,
                                             reach_rect_f32 bounds,
                                             const reach_ui_selection_item_style *style,
                                             float selection)
{
    if (commands == nullptr || style == nullptr)
    {
        return;
    }
    float radius = bounds.height * 0.5f;
    if (selection <= 0.0f || style->border_width <= 0.0f)
    {
        reach_ui_push_rect(commands, bounds, radius, style->background);
        return;
    }

    reach_color background =
        reach_theme_color_mix(style->background, style->accent, 0.22f * selection);
    reach_color border = reach_theme_color_mix(background, style->accent, 0.85f * selection);
    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_RECT;
    shape.rect = bounds;
    shape.radius = radius;
    (void)reach_render_push_bordered_background(commands, &shape, background, border,
                                                style->border_width, nullptr, 1.0f);
}

void reach_ui_selection_item_render(reach_render_command_buffer *commands, reach_rect_f32 bounds,
                                    const uint16_t *label,
                                    const reach_ui_selection_item_style *style, float selection)
{
    if (commands == nullptr || style == nullptr)
    {
        return;
    }
    reach_ui_selection_item_backdrop_render(commands, bounds, style, selection);
    reach_ui_push_text(commands, bounds, label, style->text_size, style->text_weight,
                       REACH_TEXT_ALIGNMENT_CENTER,
                       reach_theme_color_mix(style->text, style->accent, selection));
}

reach_rect_f32 reach_ui_segmented_control_item_bounds(reach_rect_f32 bounds, size_t item_count,
                                                      size_t index)
{
    if (item_count == 0 || index >= item_count || bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return {};
    }
    float item_width = bounds.width / (float)item_count;
    return {bounds.x + item_width * (float)index, bounds.y, item_width, bounds.height};
}

int32_t reach_ui_segmented_control_index_at(reach_rect_f32 bounds, size_t item_count, float x,
                                            float y)
{
    if (item_count == 0 || bounds.width <= 0.0f || bounds.height <= 0.0f || x < bounds.x ||
        x >= bounds.x + bounds.width || y < bounds.y || y >= bounds.y + bounds.height)
    {
        return -1;
    }
    size_t index = (size_t)((x - bounds.x) * (float)item_count / bounds.width);
    return index < item_count ? (int32_t)index : -1;
}

void reach_ui_segmented_control_render(reach_render_command_buffer *commands, reach_rect_f32 bounds,
                                       const uint16_t *const *labels, size_t item_count,
                                       const reach_ui_segmented_control_style *style,
                                       float selection_position)
{
    if (commands == nullptr || labels == nullptr || item_count == 0 || style == nullptr ||
        bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return;
    }
    float maximum = (float)(item_count - 1);
    if (selection_position < 0.0f)
    {
        selection_position = 0.0f;
    }
    if (selection_position > maximum)
    {
        selection_position = maximum;
    }
    float radius = bounds.height * 0.5f;
    reach_ui_push_rect(commands, bounds, radius, style->track);
    reach_rect_f32 selected = reach_ui_segmented_control_item_bounds(bounds, item_count, 0);
    selected.x += selected.width * selection_position;
    float maximum_inset = fminf(selected.width, selected.height) * 0.5f;
    float inset = style->selection_inset;
    if (inset < 0.0f)
    {
        inset = 0.0f;
    }
    if (inset > maximum_inset)
    {
        inset = maximum_inset;
    }
    selected.x += inset;
    selected.y += inset;
    selected.width -= inset * 2.0f;
    selected.height -= inset * 2.0f;
    reach_ui_push_rect(commands, selected, selected.height * 0.5f, style->selection);

    for (size_t index = 0; index < item_count; ++index)
    {
        float distance = fabsf(selection_position - (float)index);
        float selection = distance < 1.0f ? 1.0f - distance : 0.0f;
        reach_rect_f32 item = reach_ui_segmented_control_item_bounds(bounds, item_count, index);
        reach_ui_push_text(commands, item, labels[index], style->text_size, style->text_weight,
                           REACH_TEXT_ALIGNMENT_CENTER,
                           reach_theme_color_mix(style->text, style->selected_text, selection));
    }
}

void reach_ui_toggle_render(reach_render_command_buffer *commands, reach_rect_f32 bounds,
                            const reach_ui_toggle_style *style, float position)
{
    if (commands == nullptr || style == nullptr)
    {
        return;
    }
    if (position < 0.0f)
    {
        position = 0.0f;
    }
    if (position > 1.0f)
    {
        position = 1.0f;
    }
    float radius = bounds.height * 0.5f;
    reach_ui_push_rect(commands, bounds, radius,
                       reach_theme_color_mix(style->track_off, style->track_on, position));
    float inset = bounds.height * 0.15f;
    float knob = bounds.height - inset * 2.0f;
    float knob_off_x = bounds.x + inset;
    float knob_on_x = bounds.x + bounds.width - inset - knob;
    float knob_x = knob_off_x + (knob_on_x - knob_off_x) * position;
    reach_ui_push_rect(commands, {knob_x, bounds.y + inset, knob, knob}, knob * 0.5f, style->knob);
}

reach_result reach_ui_toggle_motion_render(reach_render_command_buffer *commands,
                                           reach_rect_f32 bounds,
                                           const reach_ui_toggle_style *style, uint64_t id,
                                           const reach_motion *position)
{
    if (commands == nullptr || style == nullptr || position == nullptr || id == 0 ||
        id > UINT64_MAX - 2)
        return REACH_INVALID_ARGUMENT;
    float inset = bounds.height * 0.15f;
    float diameter = bounds.height - inset * 2.0f;
    reach_rect_f32 knob_bounds = {bounds.x + inset, bounds.y + inset, diameter, diameter};
    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_RECT;
    shape.rect = bounds;
    reach_rect_f32 track_source = {}, knob_source = {};
    if (!reach_render_retained_command_bounds(&shape, &track_source))
        return REACH_INVALID_ARGUMENT;
    shape.rect = knob_bounds;
    if (!reach_render_retained_command_bounds(&shape, &knob_source))
        return REACH_INVALID_ARGUMENT;
    uint64_t parent = commands->current_group_id;
    int32_t clipped = commands->has_scissor;
    reach_rect_f32 clip = commands->scissor_rect;
    reach_motion progress = reach_motion_clamp(position, 0, 1);
    reach_render_group viewport = {};
    viewport.id = id;
    viewport.parent_id = parent;
    viewport.bounds = bounds;
    viewport.has_clip = clipped;
    viewport.clip = clip;
    commands->current_group_id = 0;
    reach_result result = reach_render_command_buffer_begin_group(commands, &viewport);
    reach_render_command_buffer_end_group(commands);
    if (result != REACH_OK)
    {
        commands->current_group_id = parent;
        return result;
    }
    reach_render_command_buffer_clear_scissor(commands);
    reach_render_group track =
        reach_ui_color_group(id + 1, track_source, style->track_off, style->track_on, &progress);
    track.parent_id = id;
    result = reach_render_command_buffer_begin_group(commands, &track);
    if (result == REACH_OK)
    {
        reach_ui_push_rect(commands, bounds, bounds.height * 0.5f, {1, 1, 1, 1});
        reach_render_command_buffer_end_group(commands);
        reach_render_group knob = {};
        knob.id = id + 2;
        knob.parent_id = id;
        knob.bounds = knob_source;
        knob.x = reach_motion_affine(&progress, bounds.width - inset * 2.0f - diameter, 0);
        result = reach_render_command_buffer_begin_group(commands, &knob);
        if (result == REACH_OK)
        {
            reach_ui_push_rect(commands, knob_bounds, diameter * 0.5f, style->knob);
            reach_render_command_buffer_end_group(commands);
        }
    }
    commands->current_group_id = parent;
    if (clipped)
        reach_render_command_buffer_set_scissor(commands, clip);
    return result;
}

void reach_ui_textbox_render(reach_render_command_buffer *commands, reach_rect_f32 bounds,
                             const reach_ui_selection_item_style *style, float selection,
                             const reach_ui_textbox_state *state)
{
    if (commands == nullptr || style == nullptr || state == nullptr)
    {
        return;
    }
    reach_rect_f32 box = bounds;
    if (state->suffix != nullptr && state->suffix_width > 0.0f &&
        state->suffix_width < bounds.width)
    {
        box.width -= state->suffix_width;
        reach_rect_f32 suffix_rect = {bounds.x + box.width, bounds.y, state->suffix_width,
                                      bounds.height};
        reach_ui_push_text(commands, suffix_rect, state->suffix, style->text_size,
                           style->text_weight, REACH_TEXT_ALIGNMENT_LEADING, state->suffix_color);
    }

    reach_color transparent = {};
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXTBOX;
    command.rect = box;
    command.color = transparent;
    command.radius = 0.0f;
    command.text_size = style->text_size;
    command.text_weight = style->text_weight;
    command.text_alignment = state->text_alignment;
    command.text_color = reach_theme_color_mix(state->text_color, style->accent, selection);
    command.placeholder_color = state->placeholder_color;
    command.selection_color = state->selection_color;
    command.caret_index = state->caret_index;
    command.caret_visible = state->caret_visible;
    command.selection_start = state->selection_start;
    command.selection_end = state->selection_end;
    reach_copy_utf16(command.text, 260, state->text);
    reach_copy_utf16(command.placeholder, 128, state->placeholder);
    (void)reach_render_command_buffer_push(commands, &command);
}
