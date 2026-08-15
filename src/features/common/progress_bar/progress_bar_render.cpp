#include "reach/features/common/progress_bar_render.h"

static reach_rect_f32 reach_progress_bar_local(reach_rect_f32 rect, reach_rect_f32 origin)
{
    rect.x -= origin.x;
    rect.y -= origin.y;
    return rect;
}

static float reach_progress_bar_min(float a, float b)
{
    return a < b ? a : b;
}

reach_rect_f32 reach_progress_bar_fill_rect(reach_rect_f32 track, float level)
{
    if (level < 0.0f)
    {
        level = 0.0f;
    }
    if (level > 1.0f)
    {
        level = 1.0f;
    }

    reach_rect_f32 fill = track;
    fill.width = track.width * level;
    return fill;
}

reach_result reach_progress_bar_build_render_commands(reach_rect_f32 track, reach_rect_f32 fill,
                                                      reach_rect_f32 origin,
                                                      reach_color track_color,
                                                      reach_color fill_color,
                                                      reach_render_command_buffer *out)
{
    reach_rect_f32 local_track = reach_progress_bar_local(track, origin);
    float track_radius = local_track.height * 0.5f;

    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = local_track;
    command.color = track_color;
    command.radius = track_radius;
    reach_render_command_buffer_push(out, &command);

    if (fill.width <= 0.0f)
    {
        return REACH_OK;
    }

    command = {};
    command.type = REACH_RENDER_COMMAND_CLIPPED_ROUNDED_RECT;
    command.rect = reach_progress_bar_local(fill, origin);
    command.clip_rect = local_track;
    command.radius = reach_progress_bar_min(track_radius, command.rect.width * 0.5f);
    command.clip_radius = track_radius;
    command.color = fill_color;
    reach_render_command_buffer_push(out, &command);

    return REACH_OK;
}
