#include "shell_internal.h"

static reach_rect_f32 reach_shell_settings_default_bounds(reach_shell *shell)
{
    reach_rect_f32 monitor = {};
    const reach_monitor_info *primary_monitor =
        shell != nullptr && shell->monitors.list != nullptr && shell->monitors.ops.primary != nullptr
            ? shell->monitors.ops.primary(shell->monitors.list)
            : nullptr;
    if (primary_monitor != nullptr)
    {
        monitor.x = (float)primary_monitor->bounds.left;
        monitor.y = (float)primary_monitor->bounds.top;
        monitor.width = (float)(primary_monitor->bounds.right - primary_monitor->bounds.left);
        monitor.height = (float)(primary_monitor->bounds.bottom - primary_monitor->bounds.top);
    }
    else
    {
        monitor = {0.0f, 0.0f, 1280.0f, 720.0f};
    }

    float scale = reach_shell_layout_dpi_scale(shell);
    float width = 780.0f * scale;
    float height = 520.0f * scale;
    if (width > monitor.width - 48.0f * scale)
    {
        width = monitor.width - 48.0f * scale;
    }
    if (height > monitor.height - 48.0f * scale)
    {
        height = monitor.height - 48.0f * scale;
    }
    if (width < 520.0f * scale)
    {
        width = 520.0f * scale;
    }
    if (height < 360.0f * scale)
    {
        height = 360.0f * scale;
    }
    return {monitor.x + (monitor.width - width) * 0.5f,
            monitor.y + (monitor.height - height) * 0.5f, width, height};
}

static reach_rect_f32 reach_shell_settings_maximized_bounds(reach_shell *shell)
{
    reach_rect_f32 monitor = {};
    const reach_monitor_info *primary_monitor =
        shell != nullptr && shell->monitors.list != nullptr && shell->monitors.ops.primary != nullptr
            ? shell->monitors.ops.primary(shell->monitors.list)
            : nullptr;
    if (primary_monitor != nullptr)
    {
        monitor.x = (float)primary_monitor->work_area.left;
        monitor.y = (float)primary_monitor->work_area.top;
        monitor.width = (float)(primary_monitor->work_area.right - primary_monitor->work_area.left);
        monitor.height =
            (float)(primary_monitor->work_area.bottom - primary_monitor->work_area.top);
    }
    else
    {
        monitor = reach_shell_settings_default_bounds(shell);
    }
    return monitor;
}

void reach_shell_refresh_settings_layout(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    reach_rect_f32 local = {};
    local.width = shell->settings_bounds.width;
    local.height = shell->settings_bounds.height;
    shell->settings_layout =
        reach_settings_layout_for_bounds(local, shell->theme, reach_shell_layout_dpi_scale(shell));
}

void reach_shell_open_settings(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    reach_shell_set_quick_settings_open(shell, 0);
    reach_shell_set_tray_popup_open(shell, 0);
    reach_shell_close_context_menu(shell);

    if (!shell->settings_open)
    {
        if (shell->settings_restored_bounds.width <= 0.0f ||
            shell->settings_restored_bounds.height <= 0.0f)
        {
            shell->settings_restored_bounds = reach_shell_settings_default_bounds(shell);
        }
        shell->settings_bounds = shell->settings_maximized
                                     ? reach_shell_settings_maximized_bounds(shell)
                                     : shell->settings_restored_bounds;
        shell->settings_open = 1;
    }

    reach_shell_refresh_settings_layout(shell);
    shell->settings.dirty_flags = 1;
    shell->dirty.render = 1;
    if (shell->settings.window.ops.show != nullptr)
    {
        (void)shell->settings.window.ops.show(shell->settings.window.window);
    }
    reach_shell_request_update(shell);
}

void reach_shell_close_settings(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }
    shell->settings_open = 0;
    shell->settings.dirty_flags = 1;
    if (shell->settings.window.ops.hide != nullptr)
    {
        (void)shell->settings.window.ops.hide(shell->settings.window.window);
    }
    reach_shell_request_update(shell);
}

void reach_shell_toggle_settings_maximized(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }
    if (!shell->settings_maximized)
    {
        if (shell->settings_bounds.width > 0.0f && shell->settings_bounds.height > 0.0f)
        {
            shell->settings_restored_bounds = shell->settings_bounds;
        }
        shell->settings_maximized = 1;
        shell->settings_bounds = reach_shell_settings_maximized_bounds(shell);
    }
    else
    {
        shell->settings_maximized = 0;
        shell->settings_bounds = shell->settings_restored_bounds.width > 0.0f
                                     ? shell->settings_restored_bounds
                                     : reach_shell_settings_default_bounds(shell);
    }
    reach_shell_refresh_settings_layout(shell);
    shell->settings.dirty_flags = 1;
    shell->dirty.render = 1;
    reach_shell_request_update(shell);
}

reach_result reach_shell_handle_settings_pointer_up(reach_shell *shell,
                                                    const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->settings_open)
    {
        return REACH_OK;
    }

    float local_x = (float)event->x - shell->settings_bounds.x;
    float local_y = (float)event->y - shell->settings_bounds.y;
    reach_settings_hit_result hit = reach_settings_hit_test(&shell->settings_layout, local_x, local_y);
    if (hit.type == REACH_SETTINGS_HIT_CLOSE)
    {
        reach_shell_close_settings(shell);
        return REACH_OK;
    }
    if (hit.type == REACH_SETTINGS_HIT_MAXIMIZE)
    {
        reach_shell_toggle_settings_maximized(shell);
        return REACH_OK;
    }
    if (hit.type == REACH_SETTINGS_HIT_NAV_ITEM)
    {
        reach_settings_model_select_page(&shell->settings_model, hit.page);
        shell->settings.dirty_flags = 1;
        shell->dirty.render = 1;
        reach_shell_request_update(shell);
        return REACH_OK;
    }
    return REACH_OK;
}
