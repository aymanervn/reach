#include "reach/features/common/icon_feedback.h"
#include "reach/features/common/ui_controls.h"

reach_result reach_push_icon_press_feedback_motion(reach_render_command_buffer *commands,
                                                   uint64_t id, reach_rect_f32 rect, float radius,
                                                   uint64_t icon_id, reach_color color,
                                                   const reach_motion *opacity)
{
    if (icon_id == 0)
        return reach_ui_feedback_render(commands, id, rect, radius, color, opacity);
    if (commands == nullptr || opacity == nullptr)
        return REACH_INVALID_ARGUMENT;
    reach_render_group group = {};
    group.id = id;
    group.bounds = rect;
    group.opacity = *opacity;
    if (group.opacity.generation == 0)
        reach_motion_set(&group.opacity, reach_motion_value(opacity));
    reach_motion_set(&group.scale, 1.0f);
    reach_result result = reach_render_command_buffer_begin_group(commands, &group);
    if (result != REACH_OK)
        return result;
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_ICON_TINT;
    command.rect = rect;
    command.icon_id = icon_id;
    command.color = color;
    result = reach_render_command_buffer_push(commands, &command);
    reach_render_command_buffer_end_group(commands);
    return result;
}

void reach_push_icon_press_feedback(reach_render_command_buffer *commands, reach_rect_f32 rect,
                                    float radius, uint64_t icon_id, reach_color color,
                                    float opacity, float minimum_opacity)
{
    if (commands == nullptr || opacity <= minimum_opacity)
    {
        return;
    }

    reach_render_command command = {};
    command.rect = rect;
    command.color = reach_theme_color_alpha(color, opacity);
    if (icon_id != 0)
    {
        command.type = REACH_RENDER_COMMAND_ICON_TINT;
        command.icon_id = icon_id;
    }
    else
    {
        command.type = REACH_RENDER_COMMAND_RECT;
        command.radius = radius;
    }
    reach_render_command_buffer_push(commands, &command);
}
