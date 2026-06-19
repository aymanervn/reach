#include "shell_internal.h"

reach_result reach_shell_render_popup_surface(reach_shell *shell, reach_surface_runtime *surface,
                                              reach_rect_f32 bounds, float notch_anchor_x,
                                              const reach_render_command_buffer *content_commands)
{
    if (shell == nullptr || surface == nullptr || surface->renderer.ops.begin_frame == nullptr)
    {
        return REACH_OK;
    }

    reach_render_command_buffer commands = {};

    reach_popup_background_input popup = {};
    popup.theme = shell->theme != nullptr ? shell->theme : reach_theme_default();
    popup.bounds = bounds;
    popup.notch_center_x = notch_anchor_x - bounds.x;
    popup.dpi_scale = reach_shell_layout_dpi_scale(shell);

    reach_result result = reach_popup_push_background(&popup, &commands);
    if (result != REACH_OK)
    {
        return result;
    }

    if (content_commands != nullptr)
    {
        for (size_t index = 0; index < content_commands->count; ++index)
        {
            result =
                reach_render_command_buffer_push(&commands, &content_commands->commands[index]);
            if (result != REACH_OK)
            {
                return result;
            }
        }
    }

    result = surface->renderer.ops.begin_frame(surface->renderer.backend);
    if (result != REACH_OK)
    {
        return result;
    }

    result = surface->renderer.ops.execute(surface->renderer.backend, &commands);
    if (result != REACH_OK)
    {
        return result;
    }

    return surface->renderer.ops.end_frame(surface->renderer.backend);
}
