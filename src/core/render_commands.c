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
        reach_rect_f32 empty = {0.0f, 0.0f, 0.0f, 0.0f};
        buffer->count = 0;
        buffer->has_scissor = 0;
        buffer->content_rect = empty;
    }
}

void reach_render_command_buffer_set_content_rect(reach_render_command_buffer *buffer,
                                                  reach_rect_f32 content_rect)
{
    if (buffer != 0)
    {
        buffer->content_rect = content_rect;
    }
}

void reach_render_command_buffer_set_scissor(reach_render_command_buffer *buffer,
                                             reach_rect_f32 scissor_rect)
{
    if (buffer != 0)
    {
        buffer->scissor_rect = scissor_rect;
        buffer->has_scissor = 1;
    }
}

void reach_render_command_buffer_clear_scissor(reach_render_command_buffer *buffer)
{
    if (buffer != 0)
    {
        buffer->has_scissor = 0;
    }
}

reach_result reach_render_push_shadow(reach_render_command_buffer *buffer,
                                      const reach_render_command *shape,
                                      const reach_shadow *shadow, float dpi_scale)
{
    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    reach_render_command command;

    if (buffer == 0 || shape == 0 || shadow == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (shadow->color.a <= 0.0f || shadow->blur <= 0.0f)
    {
        return REACH_OK;
    }

    command = *shape;
    command.type = REACH_RENDER_COMMAND_SHADOW;
    command.color = shadow->color;
    command.stroke_width = 0.0f;
    command.blur_radius = shadow->blur * scale;
    command.shadow_offset_x = shadow->offset_x * scale;
    command.shadow_offset_y = shadow->offset_y * scale;
    return reach_render_command_buffer_push(buffer, &command);
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
    if (buffer->has_scissor && !command->has_scissor)
    {
        buffer->commands[buffer->count].scissor_rect = buffer->scissor_rect;
        buffer->commands[buffer->count].has_scissor = 1;
    }
    buffer->count += 1;
    return REACH_OK;
}

