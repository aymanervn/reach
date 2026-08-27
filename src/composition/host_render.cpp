#include "host_internal.h"

#include <math.h>

reach_result reach_host_render_stage_surface(reach_host *host, reach_rect_f32 bounds)
{
    if (host == nullptr || host->stage.renderer.ops.begin_frame == nullptr)
    {
        return REACH_OK;
    }

    reach_stage_render_context ctx = {};
    ctx.theme = host->theme != nullptr ? host->theme : reach_theme_default();
    ctx.bounds = bounds;
    ctx.dpi_scale = reach_host_layout_dpi_scale(host);

    reach_render_command_buffer *commands = &host->render_commands;
    reach_render_command_buffer_clear(commands);
    reach_result build_result =
        reach_stage_append_render_commands(host->stage_capsule, &ctx, commands);
    if (build_result != REACH_OK)
    {
        return build_result;
    }

    if (host->stage.renderer.ops.begin_frame(host->stage.renderer.backend) != REACH_OK)
    {
        return REACH_ERROR;
    }

    (void)host->stage.renderer.ops.execute(host->stage.renderer.backend, commands);
    return host->stage.renderer.ops.end_frame(host->stage.renderer.backend);
}

reach_result reach_host_render_launcher_surface(reach_host *host,
                                                const reach_launcher_layout *layout,
                                                const reach_host_surface_transition_frame *frame)
{
    REACH_ASSERT(host != nullptr);
    REACH_ASSERT(layout != nullptr);
    REACH_ASSERT(frame != nullptr);
    if (host == nullptr || layout == nullptr || frame == nullptr ||
        host->launcher.renderer.ops.begin_frame == nullptr)
    {
        return REACH_OK;
    }

    reach_launcher_render_context render_ctx = {};
    render_ctx.theme = host->theme != nullptr ? host->theme : reach_theme_default();
    render_ctx.layout = layout;
    render_ctx.dpi_scale = reach_host_layout_dpi_scale(host);

    reach_render_command_buffer *commands = &host->render_commands;
    reach_render_command_buffer_clear(commands);
    reach_result build_result =
        reach_launcher_append_render_commands(host->launcher_capsule, &render_ctx, commands);
    if (build_result != REACH_OK)
    {
        return build_result;
    }
    reach_render_command_buffer_set_content_transform(commands, frame->content_rect,
                                                      frame->render_transform);

    if (host->launcher.renderer.ops.begin_frame(host->launcher.renderer.backend) != REACH_OK)
    {
        return REACH_ERROR;
    }

    (void)host->launcher.renderer.ops.execute(host->launcher.renderer.backend, commands);
    return host->launcher.renderer.ops.end_frame(host->launcher.renderer.backend);
}

reach_result reach_host_render_context_menu_surface(reach_host *host)
{
    if (host == nullptr || host->context_menu.renderer.ops.begin_frame == nullptr)
    {
        return REACH_OK;
    }

    reach_context_menu_render_context render_ctx = {};
    render_ctx.theme = host->theme != nullptr ? host->theme : reach_theme_default();
    render_ctx.dpi_scale = reach_host_layout_dpi_scale(host);

    reach_render_command_buffer *commands = &host->render_commands;
    reach_render_command_buffer_clear(commands);
    reach_result build_result = reach_context_menu_append_render_commands(
        host->context_menu_capsule, &render_ctx, commands);
    if (build_result != REACH_OK)
    {
        return build_result;
    }
    reach_host_stamp_surface_content(host, REACH_SURFACE_ID_CONTEXT_MENU, commands);

    if (host->context_menu.renderer.ops.begin_frame(host->context_menu.renderer.backend) !=
        REACH_OK)
    {
        return REACH_ERROR;
    }

    reach_result result =
        host->context_menu.renderer.ops.execute(host->context_menu.renderer.backend, commands);
    reach_result end_result =
        host->context_menu.renderer.ops.end_frame(host->context_menu.renderer.backend);
    return result != REACH_OK ? result : end_result;
}
