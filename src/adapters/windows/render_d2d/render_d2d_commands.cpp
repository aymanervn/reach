#include "render_d2d_internal.h"

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

    for (size_t index = 0; index < commands->count; ++index)
    {
        const reach_render_command *command = &commands->commands[index];
        if (command->type == REACH_RENDER_COMMAND_BLURRED_IMAGE)
        {
            if (command->icon_id == 0)
            {
                continue;
            }

            reach_result result = reach_d2d_draw_blurred_image(backend, command);
            if (result != REACH_OK)
            {
                continue;
            }

            continue;
        }

        if (command->type == REACH_RENDER_COMMAND_ICON)
        {
            if (command->icon_id == 0)
            {
                continue;
            }

            reach_result result = reach_d2d_draw_icon(backend, command);
            if (result != REACH_OK)
            {
                continue;
            }

            continue;
        }

        if (command->type == REACH_RENDER_COMMAND_VECTOR_ICON && command->icon_id != 0)
        {
            reach_result result = reach_d2d_draw_vector_icon(backend, command);
            if (result != REACH_OK)
            {
                return result;
            }
            continue;
        }

        if (command->type == REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT)
        {
            reach_result result = reach_d2d_draw_notched_rounded_rect(target, command);
            if (result != REACH_OK)
            {
                return result;
            }
            continue;
        }

        if (command->type == REACH_RENDER_COMMAND_TRIANGLE)
        {
            reach_result result = reach_d2d_draw_triangle(target, command);
            if (result != REACH_OK)
            {
                return result;
            }
            continue;
        }

        if (command->type == REACH_RENDER_COMMAND_NOTCH_STROKE)
        {
            reach_result result = reach_d2d_draw_notch_stroke(target, command);
            if (result != REACH_OK)
            {
                return result;
            }
            continue;
        }

        if (command->type == REACH_RENDER_COMMAND_ICON_TINT)
        {
            if (command->icon_id == 0)
            {
                continue;
            }

            reach_result result = reach_d2d_draw_icon_tint(backend, command);
            if (result != REACH_OK)
            {
                continue;
            }

            continue;
        }

        if (command->type == REACH_RENDER_COMMAND_RECT ||
            command->type == REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE)
        {
            reach_result result = reach_d2d_draw_rect_or_rounded_rect(target, command);
            if (result != REACH_OK)
            {
                return result;
            }
            continue;
        }

        if (command->type == REACH_RENDER_COMMAND_CLIPPED_ROUNDED_RECT)
        {
            reach_result result = reach_d2d_draw_clipped_rounded_rect(target, command);
            if (result != REACH_OK)
            {
                return result;
            }
            continue;
        }

        if (command->type == REACH_RENDER_COMMAND_TEXT)
        {
            reach_result result = reach_d2d_draw_text(backend, command);
            if (result != REACH_OK)
            {
                return result;
            }
            continue;
        }

        if (command->type == REACH_RENDER_COMMAND_TEXTBOX)
        {
            reach_result result = reach_d2d_draw_textbox(backend, command);
            if (result != REACH_OK)
            {
                return result;
            }
            continue;
        }

        if (command->type == REACH_RENDER_COMMAND_BLUR_BACKGROUND)
        {
            // Blur is handled through the platform surface/composition path,
            // not the core render command execution path.
            continue;
        }
    }

    return REACH_OK;
}
