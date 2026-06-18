#include "shell_internal.h"

static reach_rect_f32 reach_shell_settings_default_bounds(reach_shell *shell)
{
    reach_rect_f32 monitor = {};
    const reach_monitor_info *primary_monitor =
        shell != nullptr && shell->monitors.list != nullptr &&
                shell->monitors.ops.primary != nullptr
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

int32_t reach_shell_settings_window_minimized(const reach_shell *shell)
{
    if (shell == nullptr || !shell->settings_open)
    {
        return 0;
    }
    return shell->settings.window.ops.is_minimized != nullptr &&
           shell->settings.window.ops.is_minimized(shell->settings.window.window);
}

int32_t reach_shell_window_is_settings_window(const reach_shell *shell, uintptr_t window_id)
{
    if (shell == nullptr || window_id == 0 || shell->settings.window.ops.native_id == nullptr)
    {
        return 0;
    }

    return shell->settings.window.ops.native_id(shell->settings.window.window) == window_id;
}

int32_t reach_shell_foreground_is_settings_window(const reach_shell *shell, uintptr_t window_id)
{
    if (shell == nullptr || window_id == 0)
    {
        return 0;
    }
    // to be implmented, currently might require double clicking on icon if focus is lost but not
    // minimized
    return 1;
}

static void reach_shell_prepare_settings_window(reach_shell *shell)
{
    if (shell == nullptr || shell->settings.window.ops.set_bounds == nullptr)
    {
        return;
    }

    int32_t settings_window_changed = 0;
    if (reach_shell_apply_window_state(
            &shell->settings.window, shell->settings_bounds, 1.0f, &shell->settings.last_bounds,
            &shell->settings.last_opacity, &shell->settings.bounds_valid,
            &shell->settings.opacity_valid, &settings_window_changed) != REACH_OK)
    {
        return;
    }

    if (settings_window_changed && shell->settings.window.ops.apply_rounded_corners != nullptr)
    {
        (void)shell->settings.window.ops.apply_rounded_corners(
            shell->settings.window.window, 18.0f * reach_shell_layout_dpi_scale(shell));
    }

    if (shell->settings.renderer.ops.begin_frame != nullptr)
    {
        (void)reach_shell_render_settings_surface(shell);
    }
}

static void reach_shell_ensure_settings_open(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    if (!shell->settings_open)
    {
        if (shell->settings_restored_bounds.width <= 0.0f ||
            shell->settings_restored_bounds.height <= 0.0f)
        {
            shell->settings_restored_bounds = reach_shell_settings_default_bounds(shell);
        }
        shell->settings_bounds = shell->settings_restored_bounds;
        shell->settings_open = 1;
    }
    reach_shell_refresh_settings_layout(shell);
    shell->settings.dirty_flags = 1;
    shell->dirty.render = 1;
}

reach_result reach_shell_execute_settings_window_control(reach_shell *shell,
                                                         reach_shell_window_control_action action)
{
    if (shell == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (action == REACH_SHELL_WINDOW_CONTROL_CLOSE)
    {
        reach_shell_close_settings(shell);
        return REACH_OK;
    }
    if (action == REACH_SHELL_WINDOW_CONTROL_MINIMIZE)
    {
        reach_shell_minimize_settings(shell);
        return REACH_OK;
    }
    if (action == REACH_SHELL_WINDOW_CONTROL_ACTIVATE)
    {
        reach_shell_ensure_settings_open(shell);
        reach_shell_prepare_settings_window(shell);
        if (shell->settings.window.ops.raise != nullptr)
        {
            (void)shell->settings.window.ops.raise(shell->settings.window.window);
        }
        else if (shell->settings.window.ops.show != nullptr)
        {
            (void)shell->settings.window.ops.show(shell->settings.window.window);
        }
        reach_shell_request_update(shell);
        return REACH_OK;
    }

    return REACH_INVALID_ARGUMENT;
}

void reach_shell_sync_settings_bounds_from_window(reach_shell *shell)
{
    if (shell == nullptr || !shell->settings_open || reach_shell_settings_window_minimized(shell) ||
        shell->settings.window.ops.get_bounds == nullptr)
    {
        return;
    }

    reach_rect_f32 actual = {};
    if (shell->settings.window.ops.get_bounds(shell->settings.window.window, &actual) != REACH_OK ||
        actual.width <= 0.0f || actual.height <= 0.0f)
    {
        return;
    }

    if (!reach_shell_rect_equal(shell->settings_bounds, actual))
    {
        shell->settings_bounds = actual;
        shell->settings_restored_bounds = actual;
        reach_shell_refresh_settings_layout(shell);
        shell->settings.dirty_flags = 1;
    }
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

    (void)reach_shell_execute_settings_window_control(shell, REACH_SHELL_WINDOW_CONTROL_ACTIVATE);
}

void reach_shell_close_settings(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }
    reach_shell_sync_settings_bounds_from_window(shell);
    shell->settings_open = 0;
    shell->settings.dirty_flags = 1;
    if (shell->settings.window.ops.hide != nullptr)
    {
        (void)shell->settings.window.ops.hide(shell->settings.window.window);
    }
    reach_shell_request_update(shell);
}

void reach_shell_minimize_settings(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    reach_shell_sync_settings_bounds_from_window(shell);
    if (shell->settings.window.ops.minimize != nullptr)
    {
        (void)shell->settings.window.ops.minimize(shell->settings.window.window);
    }
    reach_shell_request_update(shell);
}

reach_result reach_shell_handle_settings_pointer_up(reach_shell *shell, const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->settings_open)
    {
        return REACH_OK;
    }

    reach_shell_sync_settings_bounds_from_window(shell);

    float local_x = (float)event->x - shell->settings_bounds.x;
    float local_y = (float)event->y - shell->settings_bounds.y;
    reach_settings_hit_result hit =
        reach_settings_hit_test(&shell->settings_layout, local_x, local_y);
    if (hit.type == REACH_SETTINGS_HIT_CLOSE)
    {
        reach_shell_close_settings(shell);
        return REACH_OK;
    }
    if (hit.type == REACH_SETTINGS_HIT_MINIMIZE)
    {
        reach_shell_minimize_settings(shell);
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
    if (shell->settings_model.selected_page != REACH_SETTINGS_PAGE_UPDATE)
        return REACH_OK;
    if (hit.type == REACH_SETTINGS_HIT_UPDATE_SEARCH)
    {
        reach_shell_schedule_windows_update_scan(shell);
        return REACH_OK;
    }
    if (hit.type == REACH_SETTINGS_HIT_UPDATE_INSTALL)
    {
        reach_shell_schedule_windows_update_install(shell);
        return REACH_OK;
    }
    if (hit.type == REACH_SETTINGS_HIT_UPDATE_CHECKBOX)
    {
        reach_settings_model_toggle_update(
            &shell->settings_model, shell->settings_model.update_scroll_offset + hit.update_index);
        shell->settings.dirty_flags = 1;
        shell->dirty.render = 1;
        reach_shell_request_update(shell);
    }
    return REACH_OK;
}

reach_result reach_shell_handle_settings_pointer_wheel(reach_shell *shell,
                                                        const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->settings_open ||
        shell->settings_model.selected_page != REACH_SETTINGS_PAGE_UPDATE ||
        event->wheel_delta == 0) return REACH_OK;
    if ((float)event->x < shell->settings_bounds.x ||
        (float)event->x > shell->settings_bounds.x + shell->settings_bounds.width ||
        (float)event->y < shell->settings_bounds.y ||
        (float)event->y > shell->settings_bounds.y + shell->settings_bounds.height) return REACH_OK;
    size_t previous = shell->settings_model.update_scroll_offset;
    reach_settings_model_scroll_updates(&shell->settings_model,
                                        event->wheel_delta > 0 ? -1 : 1,
                                        shell->settings_layout.update_row_count);
    if (previous != shell->settings_model.update_scroll_offset)
    {
        shell->settings.dirty_flags = 1;
        shell->dirty.render = 1;
        reach_shell_request_update(shell);
    }
    return REACH_OK;
}
