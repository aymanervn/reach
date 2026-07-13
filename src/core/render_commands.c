#include "reach/core/render_commands.h"

static void reach_copy_text(uint16_t *dst, size_t dst_count, const uint16_t *src)
{
    size_t index = 0;
    if (dst == 0 || dst_count == 0)
    {
        return;
    }
    if (src == 0)
    {
        dst[0] = 0;
        return;
    }
    while (index + 1 < dst_count && src[index] != 0)
    {
        dst[index] = src[index];
        ++index;
    }
    dst[index] = 0;
}

static reach_rect_f32 reach_dock_child_to_screen(const reach_ui_layout *layout, reach_rect_f32 rect)
{
    rect.x += layout->dock.bounds.x;
    rect.y += layout->dock.bounds.y;
    return rect;
}

void reach_render_command_buffer_clear(reach_render_command_buffer *buffer)
{
    if (buffer != 0)
    {
        buffer->count = 0;
    }
}

reach_result reach_render_command_buffer_push(reach_render_command_buffer *buffer,
                                              const reach_render_command *command)
{
    if (buffer == 0 || command == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (buffer->count >= REACH_MAX_RENDER_COMMANDS)
    {
        return REACH_ERROR;
    }

    buffer->commands[buffer->count] = *command;
    buffer->count += 1;
    return REACH_OK;
}

