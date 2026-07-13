#include "host_internal.h"

reach_result reach_host_render_popup_surface(reach_host *host, reach_surface_runtime *surface,
                                             reach_rect_f32 bounds, float notch_anchor_x,
                                             const reach_render_command_buffer *content_commands)
{
    if (host == nullptr || surface == nullptr || surface->renderer.ops.begin_frame == nullptr)
    {
        return REACH_OK;
    }

    reach_render_command_buffer commands = {};

    reach_popup_background_input popup = {};
    popup.theme = host->theme != nullptr ? host->theme : reach_theme_default();
    popup.bounds = bounds;
    popup.notch_center_x = notch_anchor_x - bounds.x;
    popup.dpi_scale = reach_host_layout_dpi_scale(host);

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
    reach_result end_result = surface->renderer.ops.end_frame(surface->renderer.backend);
    return result != REACH_OK ? result : end_result;
}
