#include "reach/core/render_commands.h"

#include <math.h>

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
        buffer->content_transform = (reach_transform_f32){1.0f, 1.0f, 0.0f, 0.0f};
    }
}

void reach_render_command_buffer_set_content_rect(reach_render_command_buffer *buffer,
                                                  reach_rect_f32 content_rect)
{
    if (buffer != 0)
    {
        buffer->content_rect = content_rect;
        buffer->content_transform =
            (reach_transform_f32){1.0f, 1.0f, content_rect.x, content_rect.y};
    }
}

void reach_render_command_buffer_set_content_transform(reach_render_command_buffer *buffer,
                                                       reach_rect_f32 content_rect,
                                                       reach_transform_f32 content_transform)
{
    if (buffer != 0)
    {
        buffer->content_rect = content_rect;
        buffer->content_transform = content_transform;
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

void reach_render_command_buffer_multiply_opacity(reach_render_command_buffer *buffer,
                                                  float opacity)
{
    size_t index;
    if (buffer == 0)
    {
        return;
    }
    if (opacity < 0.0f)
    {
        opacity = 0.0f;
    }
    if (opacity > 1.0f)
    {
        opacity = 1.0f;
    }
    for (index = 0; index < buffer->count; ++index)
    {
        reach_render_command *command = &buffer->commands[index];
        command->color.a *= opacity;
        command->text_color.a *= opacity;
        command->placeholder_color.a *= opacity;
        command->selection_color.a *= opacity;
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

static reach_rect_f32 reach_render_pixel_aligned_rect(reach_rect_f32 rect)
{
    float right = roundf(rect.x + rect.width);
    float bottom = roundf(rect.y + rect.height);
    rect.x = roundf(rect.x);
    rect.y = roundf(rect.y);
    rect.width = right > rect.x ? right - rect.x : 0.0f;
    rect.height = bottom > rect.y ? bottom - rect.y : 0.0f;
    return rect;
}

static reach_render_command reach_render_inset_shape(const reach_render_command *shape, float inset)
{
    reach_render_command inner = *shape;
    inner.rect.x += inset;
    inner.rect.y += inset;
    inner.rect.width -= inset * 2.0f;
    inner.rect.height -= inset * 2.0f;
    inner.radius = inner.radius > inset ? inner.radius - inset : 0.0f;
    inner.stroke_width = 0.0f;
    return inner;
}

reach_result reach_render_push_bordered_background(
    reach_render_command_buffer *buffer, const reach_render_command *shape,
    reach_color background_color, reach_color border_color, float border_thickness,
    const reach_shadow *shadow, float dpi_scale)
{
    reach_render_command outer;
    reach_render_command inner;
    reach_result result;

    if (buffer == 0 || shape == 0 ||
        (shape->type != REACH_RENDER_COMMAND_RECT &&
         shape->type != REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT))
    {
        return REACH_INVALID_ARGUMENT;
    }

    outer = *shape;
    outer.rect = reach_render_pixel_aligned_rect(outer.rect);
    outer.stroke_width = 0.0f;
    if (outer.rect.width <= 0.0f || outer.rect.height <= 0.0f)
    {
        return REACH_OK;
    }

    if (shadow != 0)
    {
        result = reach_render_push_shadow(buffer, &outer, shadow, dpi_scale);
        if (result != REACH_OK)
        {
            return result;
        }
    }

    background_color.a = 1.0f;
    if (border_thickness <= 0.0f || border_color.a <= 0.0f)
    {
        outer.color = background_color;
        return reach_render_command_buffer_push(buffer, &outer);
    }

    border_color.a = 1.0f;
    outer.color = border_color;
    result = reach_render_command_buffer_push(buffer, &outer);
    if (result != REACH_OK)
    {
        return result;
    }

    inner = reach_render_inset_shape(&outer, border_thickness);
    if (inner.rect.width <= 0.0f || inner.rect.height <= 0.0f)
    {
        return REACH_OK;
    }
    inner.color = background_color;
    return reach_render_command_buffer_push(buffer, &inner);
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
