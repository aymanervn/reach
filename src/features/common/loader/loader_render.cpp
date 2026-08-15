#include "reach/features/common/loader_render.h"

static reach_rect_f32 reach_loader_local(reach_rect_f32 rect, reach_rect_f32 origin)
{
    rect.x -= origin.x;
    rect.y -= origin.y;
    return rect;
}

static float reach_loader_min(float a, float b)
{
    return a < b ? a : b;
}

reach_result reach_loader_build_render_commands(reach_rect_f32 container, reach_rect_f32 bar,
                                                reach_rect_f32 origin, reach_color bar_color,
                                                reach_render_command_buffer *out)
{
    if (bar.width <= 0.0f || container.width <= 0.0f)
    {
        return REACH_OK;
    }

    reach_rect_f32 local_container = reach_loader_local(container, origin);
    float container_radius = local_container.height * 0.5f;

    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_CLIPPED_ROUNDED_RECT;
    command.rect = reach_loader_local(bar, origin);
    command.clip_rect = local_container;
    command.radius = reach_loader_min(container_radius, command.rect.width * 0.5f);
    command.clip_radius = container_radius;
    command.color = bar_color;
    reach_render_command_buffer_push(out, &command);

    return REACH_OK;
}
