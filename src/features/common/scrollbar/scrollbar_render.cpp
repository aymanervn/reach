#include "reach/features/common/scrollbar_render.h"

static reach_rect_f32 reach_scrollbar_local(reach_rect_f32 rect, reach_rect_f32 origin)
{
    rect.x -= origin.x;
    rect.y -= origin.y;
    return rect;
}

reach_result reach_scrollbar_build_render_commands(reach_rect_f32 track, reach_rect_f32 thumb,
                                                   reach_rect_f32 origin, reach_color track_color,
                                                   reach_color thumb_color,
                                                   reach_render_command_buffer *out)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = reach_scrollbar_local(track, origin);
    command.color = track_color;
    command.radius = command.rect.width * 0.5f;
    reach_render_command_buffer_push(out, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = reach_scrollbar_local(thumb, origin);
    command.color = thumb_color;
    command.radius = command.rect.width * 0.5f;
    reach_render_command_buffer_push(out, &command);

    return REACH_OK;
}
