#include "reach/features/popup.h"

float reach_popup_radius(const reach_theme *theme)
{
    return theme != nullptr ? theme->radius_large : reach_theme_default()->radius_large;
}

static float reach_popup_scale(float value, float dpi_scale)
{
    return value * (dpi_scale > 0.0f ? dpi_scale : 1.0f);
}

float reach_popup_radius_scaled(const reach_theme *theme, float dpi_scale)
{
    return reach_popup_scale(reach_popup_radius(theme), dpi_scale);
}

int32_t reach_popup_notch_side(int32_t direction)
{
    return direction == REACH_POPUP_DROP_DOWN ? REACH_NOTCH_SIDE_TOP : REACH_NOTCH_SIDE_BOTTOM;
}

static float reach_popup_clamp(float value, float min_value, float max_value)
{
    if (max_value < min_value)
    {
        return min_value;
    }
    if (value < min_value)
    {
        return min_value;
    }
    return value > max_value ? max_value : value;
}

static float reach_popup_clamp_to_monitor(float value, float extent, float monitor_origin,
                                          float monitor_extent, float margin)
{
    if (monitor_extent <= 0.0f)
    {
        return value;
    }
    return reach_popup_clamp(value, monitor_origin + margin,
                             monitor_origin + monitor_extent - extent - margin);
}

reach_popup_placement reach_popup_place(const reach_popup_anchor *anchor, float width, float height,
                                        float margin)
{
    reach_popup_placement placement = {};
    if (anchor == nullptr)
    {
        return placement;
    }

    float drop_y = anchor->direction == REACH_POPUP_DROP_DOWN
                       ? anchor->bar_edge_y + margin
                       : anchor->bar_edge_y - height - margin;

    placement.bounds.width = width;
    placement.bounds.height = height;
    placement.bounds.x = reach_popup_clamp_to_monitor(
        anchor->button.x + anchor->button.width * 0.5f - width * 0.5f, width, anchor->monitor.x,
        anchor->monitor.width, margin);
    placement.bounds.y = reach_popup_clamp_to_monitor(drop_y, height, anchor->monitor.y,
                                                      anchor->monitor.height, margin);
    placement.notch_anchor_x = anchor->button.x + anchor->button.width * 0.5f;
    placement.notch_side = reach_popup_notch_side(anchor->direction);
    return placement;
}

float reach_popup_notch_width(void)
{
    return 18.0f;
}

float reach_popup_notch_width_scaled(float dpi_scale)
{
    return reach_popup_scale(reach_popup_notch_width(), dpi_scale);
}

float reach_popup_notch_height(void)
{
    return 8.0f;
}

float reach_popup_notch_height_scaled(float dpi_scale)
{
    return reach_popup_scale(reach_popup_notch_height(), dpi_scale);
}

float reach_popup_clamp_notch_center_scaled(float notch_center_x, float width, float dpi_scale)
{
    float radius = reach_popup_radius_scaled(reach_theme_default(), dpi_scale);
    float notch_width = reach_popup_notch_width_scaled(dpi_scale);
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

    float radius = reach_popup_radius_scaled(input->theme, input->dpi_scale);
    float notch_width = reach_popup_notch_width_scaled(input->dpi_scale);
    float notch_height = reach_popup_notch_height_scaled(input->dpi_scale);
    float notch_center = reach_popup_clamp_notch_center_scaled(
        input->notch_center_x, input->bounds.width, input->dpi_scale);
    reach_color border = input->theme->popup_border;
    float border_thickness = input->theme->border_thickness;
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT;
    command.rect.x = 0.5f;
    command.rect.y = 0.5f;
    command.rect.width = input->bounds.width - 1.0f;
    command.rect.height = input->bounds.height - 1.0f;
    command.color = input->theme->popup_background;
    command.radius = radius;
    command.notch_center_x = notch_center;
    command.notch_width = notch_width;
    command.notch_height = notch_height;
    command.notch_side = input->notch_side;
    reach_render_command_buffer_push(out_commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT;
    command.rect.x = 0.5f;
    command.rect.y = 0.5f;
    command.rect.width = input->bounds.width - 1.0f;
    command.rect.height = input->bounds.height - 1.0f;
    command.color = border;
    command.radius = radius;
    command.stroke_width = border_thickness;
    command.notch_center_x = notch_center;
    command.notch_width = notch_width;
    command.notch_height = notch_height;
    command.notch_side = input->notch_side;
    return reach_render_command_buffer_push(out_commands, &command);
}
