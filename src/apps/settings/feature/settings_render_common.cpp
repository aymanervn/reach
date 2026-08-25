#include "settings_render_internal.h"

#include "reach/core/typography.h"

float reach_settings_scale(const reach_settings_render_input *input, float value)
{
    return value * (input->dpi_scale > 0.0f ? input->dpi_scale : 1.0f);
}

reach_color reach_settings_color_with_alpha(reach_color color, float alpha)
{
    color.a = alpha;
    return color;
}

void reach_settings_push_rect(reach_render_command_buffer *commands, reach_rect_f32 rect,
                              float radius, reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = rect;
    command.radius = radius;
    command.color = color;
    (void)reach_render_command_buffer_push(commands, &command);
}

void reach_settings_push_masked_rect(reach_render_command_buffer *commands, reach_rect_f32 rect,
                                     float radius, int32_t corner_mask, reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = rect;
    command.radius = radius;
    command.corner_mask = corner_mask;
    command.color = color;
    (void)reach_render_command_buffer_push(commands, &command);
}

void reach_settings_push_bordered_background(reach_render_command_buffer *commands,
                                             reach_rect_f32 rect, float radius, float width,
                                             reach_color background, reach_color border)
{
    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_RECT;
    shape.rect = rect;
    shape.radius = radius;
    (void)reach_render_push_bordered_background(commands, &shape, background, border, width,
                                                nullptr, 1.0f);
}

void reach_settings_push_text(reach_render_command_buffer *commands, reach_rect_f32 rect,
                              const uint16_t *text, float size, int32_t weight, int32_t alignment,
                              reach_color color, int32_t ellipsis)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = rect;
    command.text_size = size;
    command.text_weight = weight;
    command.text_alignment = alignment;
    command.text_ellipsis = ellipsis;
    command.color = color;
    reach_copy_utf16(command.text, 260, text);
    (void)reach_render_command_buffer_push(commands, &command);
}

void reach_settings_push_icon(reach_render_command_buffer *commands, reach_rect_f32 rect,
                              reach_color color, reach_vector_icon_id icon_id, float inset_ratio)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    float inset = rect.width * inset_ratio;
    command.rect = {rect.x + inset, rect.y + inset, rect.width - inset * 2.0f,
                    rect.height - inset * 2.0f};
    command.icon_id = icon_id;
    command.color = color;
    (void)reach_render_command_buffer_push(commands, &command);
}

void reach_settings_push_app_icon(reach_render_command_buffer *commands, reach_rect_f32 rect,
                                  uint64_t icon_id, float alpha)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_ICON;
    command.rect = rect;
    command.icon_id = icon_id;
    command.color = {1.0f, 1.0f, 1.0f, alpha};
    (void)reach_render_command_buffer_push(commands, &command);
}

reach_ui_button_style reach_settings_button_style(const reach_settings_render_input *input,
                                                  reach_color background)
{
    reach_ui_button_style style = {};
    style.background = background;
    style.radius = reach_settings_scale(input, input->theme->radius_small);
    style.disabled_background = input->theme->settings_button_disabled_background;
    style.text = input->theme->settings_button_text;
    style.disabled_text = input->theme->settings_secondary_text;
    style.text_size = reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM);
    style.pressed_darken = input->theme->button_pressed_darken;
    style.text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
    return style;
}

reach_ui_selection_item_style reach_settings_pill_style(const reach_settings_render_input *input,
                                                        reach_color accent)
{
    reach_ui_selection_item_style style = {};
    style.background = input->theme->settings_pill_background;
    style.accent = accent;
    style.text = input->theme->settings_secondary_text;
    style.border_width = reach_settings_scale(input, 1.0f);
    style.text_size = reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM);
    style.text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
    return style;
}
