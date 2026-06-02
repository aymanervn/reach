#include "reach/features/popup.h"

float reach_popup_radius(void)
{
    return 14.0f;
}

float reach_popup_notch_width(void)
{
    return 18.0f;
}

float reach_popup_notch_height(void)
{
    return 8.0f;
}

float reach_popup_clamp_notch_center(float notch_center_x, float width)
{
    float radius = reach_popup_radius();
    float notch_width = reach_popup_notch_width();
    float min_center = radius + notch_width;
    float max_center = width - radius - notch_width;

    if (max_center < min_center)
    {
        return width * 0.5f;
    }
    if (notch_center_x < min_center)
    {
        return min_center;
    }
    if (notch_center_x > max_center)
    {
        return max_center;
    }
    return notch_center_x;
}

reach_result reach_popup_push_background(const reach_popup_background_input *input,
                                         reach_render_command_buffer *out_commands)
{
    if (input == nullptr || input->theme == nullptr || input->bounds.width <= 0.0f ||
        input->bounds.height <= 0.0f || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    float radius = reach_popup_radius();
    float notch_width = reach_popup_notch_width();
    float notch_height = reach_popup_notch_height();
    float notch_center = reach_popup_clamp_notch_center(input->notch_center_x, input->bounds.width);
    reach_color border = input->theme->dock_border;
    border.a = 1.0f;

    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT;
    command.rect.x = 0.5f;
    command.rect.y = 0.5f;
    command.rect.width = input->bounds.width - 1.0f;
    command.rect.height = input->bounds.height - 1.0f;
    command.color = input->theme->tray_popup_background;
    command.radius = radius;
    command.notch_center_x = notch_center;
    command.notch_width = notch_width;
    command.notch_height = notch_height;
    reach_render_command_buffer_push(out_commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT;
    command.rect.x = 0.5f;
    command.rect.y = 0.5f;
    command.rect.width = input->bounds.width - 1.0f;
    command.rect.height = input->bounds.height - 1.0f;
    command.color = border;
    command.radius = radius;
    command.stroke_width = 1.0f;
    command.notch_center_x = notch_center;
    command.notch_width = notch_width;
    command.notch_height = notch_height;
    return reach_render_command_buffer_push(out_commands, &command);
}
