#include "reach/features/common/icon_feedback.h"
#include "reach/features/common/ui_controls.h"

reach_result reach_push_icon_press_feedback_motion(reach_render_command_buffer *commands,
                                                   uint64_t id, reach_rect_f32 rect, float radius,
                                                   uint64_t icon_id, reach_color color,
                                                   const reach_motion *opacity,
                                                   const reach_render_group *placement)
{
    color.a = 1.0f;
    if (icon_id == 0)
        return reach_ui_feedback_render(commands, id, rect, radius, color, opacity, placement);
    reach_result result = reach_ui_feedback_begin(commands, id, rect, opacity, placement);
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
