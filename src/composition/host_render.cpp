#include "host_internal.h"

#include <math.h>

reach_result reach_host_render_dock_surface(reach_host *host, const reach_dock_layout *layout)
{
    REACH_ASSERT(host != nullptr);
    REACH_ASSERT(layout != nullptr);
    if (host == nullptr || layout == nullptr || host->dock.renderer.ops.begin_frame == nullptr)
    {
        return REACH_OK;
    }

    uintptr_t focused_window =
        host->window_manager.ops.foreground != nullptr
            ? host->window_manager.ops.foreground(host->window_manager.manager)
            : 0;

    const reach_theme *theme = host->theme != nullptr ? host->theme : reach_theme_default();

    reach_render_command_buffer commands = {};
    reach_dock_render_context render_ctx = {};
    render_ctx.theme = theme;
    render_ctx.layout = layout;
    render_ctx.focused_window = focused_window;
    render_ctx.pinned_apps = host->pinned_apps;
    render_ctx.pinned_app_count = host->pinned_app_count;
    render_ctx.icon_size_px = reach_host_dock_icon_size_px(host);
    render_ctx.dpi_scale = reach_host_layout_dpi_scale(host);
    render_ctx.dock_gap = host->dock_config.gap;

    reach_result result =
        reach_dock_append_render_commands(host->dock_capsule, &render_ctx, &commands);
    if (result != REACH_OK)
    {
        return result;
    }

    if (host->dock.renderer.ops.begin_frame(host->dock.renderer.backend) != REACH_OK)
    {
        return REACH_ERROR;
    }

    /* A failed execute must still close the frame: an unbalanced BeginDraw
       wedges the target (WRONG_STATE on every later frame — a frozen surface). */
    result = host->dock.renderer.ops.execute(host->dock.renderer.backend, &commands);
    reach_result end_result = host->dock.renderer.ops.end_frame(host->dock.renderer.backend);
    return result != REACH_OK ? result : end_result;
}

reach_result reach_host_render_tray_surface(reach_host *host, reach_rect_f32 bounds)
{
    if (host == nullptr || host->tray.renderer.ops.begin_frame == nullptr)
    {
        return REACH_OK;
    }

    reach_render_command_buffer commands = {};
    reach_tray_render_context render_ctx = {};
    render_ctx.theme = host->theme != nullptr ? host->theme : reach_theme_default();
    render_ctx.bounds = bounds;
    render_ctx.dock_height = host->layout.dock.bounds.height;
    render_ctx.dpi_scale = reach_host_layout_dpi_scale(host);

    reach_result result =
        reach_tray_append_render_commands(host->tray_capsule, &render_ctx, &commands);
    if (result != REACH_OK)
    {
        return result;
    }

    if (host->tray.renderer.ops.begin_frame(host->tray.renderer.backend) != REACH_OK)
    {
        return REACH_ERROR;
    }

    (void)host->tray.renderer.ops.execute(host->tray.renderer.backend, &commands);
    return host->tray.renderer.ops.end_frame(host->tray.renderer.backend);
}

reach_result reach_host_render_quick_settings_surface(reach_host *host)
{
    if (host == nullptr || host->quick_settings.renderer.ops.begin_frame == nullptr)
    {
        return REACH_OK;
    }

    reach_render_command_buffer commands = {};
    reach_result result = reach_quick_settings_append_render_commands(
        host->quick_settings_capsule,
        host->theme != nullptr ? host->theme : reach_theme_default(),
        reach_host_layout_dpi_scale(host), &commands);
    if (result != REACH_OK)
    {
        return result;
    }

    return reach_host_render_popup_surface(host, &host->quick_settings,
                                            reach_quick_settings_state_ptr(host->quick_settings_capsule)->bounds,
                                            reach_quick_settings_state_ptr(host->quick_settings_capsule)->notch_anchor_x, &commands);
}

size_t reach_host_switcher_visible_count(const reach_host *host)
{
    if (host == nullptr)
    {
        return 0;
    }
    size_t window_count = reach_host_surface_transition_visible(&host->switcher_transition)
                              ? reach_switcher_state_ptr(host->switcher_capsule)->window_count
                              : reach_host_open_window_count(host);
    return reach_switcher_visible_count(window_count);
}

reach_result reach_host_render_switcher_surface(reach_host *host, reach_rect_f32 bounds)
{
    if (host == nullptr || host->switcher.renderer.ops.begin_frame == nullptr)
    {
        return REACH_OK;
    }

    /* The switcher capsule owns its render-input assembly (icons + labels). Composition
     * lends the borrowed window/icon data and owns the surface frame. */
    reach_switcher_render_context ctx = {};
    ctx.theme = host->theme != nullptr ? host->theme : reach_theme_default();
    ctx.bounds = bounds;
    ctx.dpi_scale = reach_host_layout_dpi_scale(host);
    ctx.icon_size_px = reach_host_dock_icon_size_px(host);

    reach_render_command_buffer commands = {};
    reach_result build_result =
        reach_switcher_append_render_commands(host->switcher_capsule, &ctx, &commands);
    if (build_result != REACH_OK)
    {
        return build_result;
    }

    if (host->switcher.renderer.ops.begin_frame(host->switcher.renderer.backend) != REACH_OK)
    {
        return REACH_ERROR;
    }

    (void)host->switcher.renderer.ops.execute(host->switcher.renderer.backend, &commands);
    return host->switcher.renderer.ops.end_frame(host->switcher.renderer.backend);
}

reach_result reach_host_render_launcher_surface(reach_host *host,
                                                 const reach_launcher_layout *layout)
{
    REACH_ASSERT(host != nullptr);
    REACH_ASSERT(layout != nullptr);
    if (host == nullptr || layout == nullptr ||
        host->launcher.renderer.ops.begin_frame == nullptr)
    {
        return REACH_OK;
    }

    reach_launcher_render_context render_ctx = {};
    render_ctx.theme = host->theme != nullptr ? host->theme : reach_theme_default();
    render_ctx.layout = layout;
    render_ctx.dpi_scale = reach_host_layout_dpi_scale(host);

    reach_render_command_buffer commands = {};
    reach_result build_result =
        reach_launcher_append_render_commands(host->launcher_capsule, &render_ctx, &commands);
    if (build_result != REACH_OK)
    {
        return build_result;
    }

    if (host->launcher.renderer.ops.begin_frame(host->launcher.renderer.backend) != REACH_OK)
    {
        return REACH_ERROR;
    }

    (void)host->launcher.renderer.ops.execute(host->launcher.renderer.backend, &commands);
    return host->launcher.renderer.ops.end_frame(host->launcher.renderer.backend);
}

reach_result reach_host_render_clipboard_surface(reach_host *host)
{
    if (host == nullptr || host->clipboard_surface.renderer.ops.begin_frame == nullptr)
    {
        return REACH_OK;
    }
    reach_render_command_buffer commands = {};
    reach_result result = reach_clipboard_append_render_commands(
        host->clipboard_capsule, host->theme != nullptr ? host->theme : reach_theme_default(),
        reach_host_layout_dpi_scale(host), &commands);
    if (result != REACH_OK)
    {
        return result;
    }
    result = host->clipboard_surface.renderer.ops.begin_frame(
        host->clipboard_surface.renderer.backend);
    if (result != REACH_OK)
    {
        return result;
    }
    /* Always close the frame (see the dock render note). */
    result = host->clipboard_surface.renderer.ops.execute(
        host->clipboard_surface.renderer.backend, &commands);
    reach_result end_result = host->clipboard_surface.renderer.ops.end_frame(
        host->clipboard_surface.renderer.backend);
    return result != REACH_OK ? result : end_result;
}

reach_result reach_host_render_context_menu_surface(reach_host *host)
{
    if (host == nullptr || host->context_menu.renderer.ops.begin_frame == nullptr)
    {
        return REACH_OK;
    }

    reach_dock_layout screen_dock = reach_dock_layout_to_screen(host->layout.dock);

    reach_context_menu_render_context render_ctx = {};
    render_ctx.theme = host->theme != nullptr ? host->theme : reach_theme_default();
    render_ctx.dock_layout = &screen_dock;
    render_ctx.has_layout = host->has_layout;
    render_ctx.dpi_scale = reach_host_layout_dpi_scale(host);

    reach_render_command_buffer commands = {};
    reach_result build_result = reach_context_menu_append_render_commands(
        host->context_menu_capsule, &render_ctx, &commands);
    if (build_result != REACH_OK)
    {
        return build_result;
    }

    if (host->context_menu.renderer.ops.begin_frame(host->context_menu.renderer.backend) !=
        REACH_OK)
    {
        return REACH_ERROR;
    }

    /* Always close the frame (see the dock render note). */
    reach_result result =
        host->context_menu.renderer.ops.execute(host->context_menu.renderer.backend, &commands);
    reach_result end_result =
        host->context_menu.renderer.ops.end_frame(host->context_menu.renderer.backend);
    return result != REACH_OK ? result : end_result;
}
