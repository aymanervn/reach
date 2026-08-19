#include "render_d2d_internal.h"

static reach_result reach_d2d_execute_command(reach_render_backend *backend,
                                              ID2D1RenderTarget *target,
                                              const reach_render_command *command)
{
    if (command->type == REACH_RENDER_COMMAND_BLURRED_IMAGE)
    {
        if (command->icon_id == 0)
        {
            return REACH_OK;
        }

        (void)reach_d2d_draw_blurred_image(backend, command);
        return REACH_OK;
    }

    if (command->type == REACH_RENDER_COMMAND_ICON)
    {
        if (command->icon_id == 0)
        {
            return REACH_OK;
        }

        (void)reach_d2d_draw_icon(backend, command);
        return REACH_OK;
    }

    if (command->type == REACH_RENDER_COMMAND_VECTOR_ICON && command->icon_id != 0)
    {
        return reach_d2d_draw_vector_icon(backend, command);
    }

    if (command->type == REACH_RENDER_COMMAND_SHADOW)
    {
        return reach_d2d_draw_shadow(backend, command);
    }

    if (command->type == REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT)
    {
        return reach_d2d_draw_notched_rounded_rect(target, command);
    }

    if (command->type == REACH_RENDER_COMMAND_TRIANGLE)
    {
        return reach_d2d_draw_triangle(target, command);
    }

    if (command->type == REACH_RENDER_COMMAND_NOTCH_STROKE)
    {
        return reach_d2d_draw_notch_stroke(target, command);
    }

    if (command->type == REACH_RENDER_COMMAND_ICON_TINT)
    {
        if (command->icon_id == 0)
        {
            return REACH_OK;
        }

        (void)reach_d2d_draw_icon_tint(backend, command);
        return REACH_OK;
    }

    if (command->type == REACH_RENDER_COMMAND_RECT ||
        command->type == REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE)
    {
        return reach_d2d_draw_rect_or_rounded_rect(target, command);
    }

    if (command->type == REACH_RENDER_COMMAND_ARC_STROKE)
    {
        return reach_d2d_draw_arc_stroke(target, command);
    }

    if (command->type == REACH_RENDER_COMMAND_CLIPPED_ROUNDED_RECT)
    {
        return reach_d2d_draw_clipped_rounded_rect(target, command);
    }

    if (command->type == REACH_RENDER_COMMAND_TEXT)
    {
        return reach_d2d_draw_text(backend, command);
    }

    if (command->type == REACH_RENDER_COMMAND_TEXTBOX)
    {
        return reach_d2d_draw_textbox(backend, command);
    }

    return REACH_OK;
}

static int32_t reach_d2d_scissor_visible(const reach_render_command *command)
{
    return command->scissor_rect.width > 0.0f && command->scissor_rect.height > 0.0f;
}

reach_result reach_d2d_execute(reach_render_backend *backend,
                               const reach_render_command_buffer *commands)
{
    REACH_ASSERT(backend != nullptr);
    REACH_ASSERT(commands != nullptr);

    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (backend == nullptr || target == nullptr || commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const int32_t has_content_rect =
        commands->content_rect.width > 0.0f && commands->content_rect.height > 0.0f;
    if (has_content_rect)
    {
        (void)reach_wuc_apply_content_clip(backend, commands->content_rect);
        target->SetTransform(D2D1::Matrix3x2F::Translation(commands->content_rect.x,
                                                           commands->content_rect.y));
    }

    reach_result outcome = REACH_OK;
    for (size_t index = 0; index < commands->count && outcome == REACH_OK; ++index)
    {
        const reach_render_command *command = &commands->commands[index];
        if (!command->has_scissor)
        {
            outcome = reach_d2d_execute_command(backend, target, command);
            continue;
        }

        if (!reach_d2d_scissor_visible(command))
        {
            continue;
        }

        D2D1_RECT_F scissor = D2D1::RectF(
            command->scissor_rect.x, command->scissor_rect.y,
            command->scissor_rect.x + command->scissor_rect.width,
            command->scissor_rect.y + command->scissor_rect.height);
        target->PushAxisAlignedClip(scissor, D2D1_ANTIALIAS_MODE_ALIASED);
        outcome = reach_d2d_execute_command(backend, target, command);
        target->PopAxisAlignedClip();
    }

    if (has_content_rect)
    {
        target->SetTransform(D2D1::Matrix3x2F::Identity());
    }

    return outcome;
}
