#include "reach/features/common/ui_controls.h"

#include "reach/support/util.h"

static reach_color reach_ui_color_with_alpha(reach_color color, float alpha)
{
    color.a = alpha;
    return color;
}

static reach_color reach_ui_color_mix(reach_color from, reach_color to, float t)
{
    reach_color mixed;
    mixed.r = from.r + (to.r - from.r) * t;
    mixed.g = from.g + (to.g - from.g) * t;
    mixed.b = from.b + (to.b - from.b) * t;
    mixed.a = from.a + (to.a - from.a) * t;
    return mixed;
}

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

static void reach_ui_push_stroke(reach_render_command_buffer *commands, reach_rect_f32 rect,
                                 float radius, float width, reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE;
    command.rect = rect;
    command.radius = radius;
    command.stroke_width = width;
    command.color = color;
    (void)reach_render_command_buffer_push(commands, &command);
}

static void reach_ui_push_text(reach_render_command_buffer *commands, reach_rect_f32 rect,
                               const uint16_t *text, float size, int32_t weight,
                               int32_t alignment, reach_color color)
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
                            int32_t enabled)
{
    if (commands == nullptr || style == nullptr)
    {
        return;
    }
    reach_ui_push_rect(commands, bounds, style->radius,
                       enabled ? style->background : style->disabled_background);
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
    reach_ui_push_rect(commands, bounds, radius, style->background);
    if (selection > 0.0f)
    {
        reach_ui_push_rect(commands, bounds, radius,
                           reach_ui_color_with_alpha(style->accent, 0.22f * selection));
        reach_ui_push_stroke(commands, bounds, radius, style->stroke_width,
                             reach_ui_color_with_alpha(style->accent, 0.85f * selection));
    }
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
                       reach_ui_color_mix(style->text, style->accent, selection));
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
                       reach_ui_color_mix(style->track_off, style->track_on, position));
    float inset = bounds.height * 0.15f;
    float knob = bounds.height - inset * 2.0f;
    float knob_off_x = bounds.x + inset;
    float knob_on_x = bounds.x + bounds.width - inset - knob;
    float knob_x = knob_off_x + (knob_on_x - knob_off_x) * position;
    reach_ui_push_rect(commands, {knob_x, bounds.y + inset, knob, knob}, knob * 0.5f,
                       style->knob);
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
                           style->text_weight, REACH_TEXT_ALIGNMENT_LEADING,
                           state->suffix_color);
    }

    reach_color transparent = {};
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXTBOX;
    command.rect = box;
    command.color = transparent;
    command.radius = 0.0f;
    command.stroke_width = 0.0f;
    command.text_size = style->text_size;
    command.text_weight = style->text_weight;
    command.text_alignment = state->text_alignment;
    command.text_color = reach_ui_color_mix(state->text_color, style->accent, selection);
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
