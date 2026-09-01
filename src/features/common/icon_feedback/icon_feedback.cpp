#include "reach/features/common/icon_feedback.h"

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
