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

void reach_ui_button_render(reach_render_command_buffer *commands, reach_rect_f32 bounds,
                            const uint16_t *label, const reach_ui_button_style *style,
                            int32_t enabled, float pressed)
{
    if (commands == nullptr || style == nullptr)
    {
        return;
    }
    if (pressed < 0.0f)
    {
        pressed = 0.0f;
    }
    if (pressed > 1.0f)
    {
        pressed = 1.0f;
    }
    reach_color background = enabled ? style->background : style->disabled_background;
    if (enabled && pressed > 0.0f)
    {
        const float darken = style->pressed_darken > 0.0f ? style->pressed_darken : 1.0f;
        reach_color darkened = {background.r * darken, background.g * darken, background.b * darken,
                                background.a};
        background = reach_theme_color_mix(background, darkened, pressed);
    }
    reach_ui_push_rect(commands, bounds, style->radius, background);
    reach_ui_push_text(commands, bounds, label, style->text_size, style->text_weight,
                       REACH_TEXT_ALIGNMENT_CENTER, enabled ? style->text : style->disabled_text);
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

void reach_ui_segmented_control_render(reach_render_command_buffer *commands,
                                       reach_rect_f32 bounds, const uint16_t *const *labels,
                                       size_t item_count,
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
