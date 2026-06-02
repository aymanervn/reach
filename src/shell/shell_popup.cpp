#include "shell_internal.h"

static int32_t reach_shell_rect_contains_point(reach_rect_f32 rect, reach_point_i32 point)
{
    return (float)point.x >= rect.x && (float)point.x <= rect.x + rect.width &&
           (float)point.y >= rect.y && (float)point.y <= rect.y + rect.height;
}
static void reach_shell_handle_global_mouse_down(reach_shell *shell, reach_point_i32 point)
{
    if (shell == nullptr)
    {
        return;
    }

    int32_t on_tray = shell->tray_state.popup_open && (float)point.x >= shell->tray.last_bounds.x &&
                      (float)point.x <= shell->tray.last_bounds.x + shell->tray.last_bounds.width &&
                      (float)point.y >= shell->tray.last_bounds.y &&
                      (float)point.y <= shell->tray.last_bounds.y + shell->tray.last_bounds.height;
    int32_t on_context = shell->context_menu_state.open &&
                         (float)point.x >= shell->context_menu_state.bounds.x &&
                         (float)point.x <= shell->context_menu_state.bounds.x +
                                               shell->context_menu_state.bounds.width &&
                         (float)point.y >= shell->context_menu_state.bounds.y &&
                         (float)point.y <= shell->context_menu_state.bounds.y +
                                               shell->context_menu_state.bounds.height;
    int32_t on_quick_settings =
        shell->quick_settings_open && (float)point.x >= shell->quick_settings_bounds.x &&
        (float)point.x <= shell->quick_settings_bounds.x + shell->quick_settings_bounds.width &&
        (float)point.y >= shell->quick_settings_bounds.y &&
        (float)point.y <= shell->quick_settings_bounds.y + shell->quick_settings_bounds.height;
    int32_t on_launcher =
        shell->ui.launcher.open && (float)point.x >= shell->layout.launcher.bounds.x &&
        (float)point.x <= shell->layout.launcher.bounds.x + shell->layout.launcher.bounds.width &&
        (float)point.y >= shell->layout.launcher.bounds.y &&
        (float)point.y <= shell->layout.launcher.bounds.y + shell->layout.launcher.bounds.height;
    reach_dock_hit_result dock_hit =
        shell->has_layout ? reach_dock_hit_test(&shell->layout.dock, point.x, point.y)
                          : reach_dock_hit_result{};
    int32_t on_tray_button =
        shell->tray_state.popup_open && dock_hit.type == REACH_DOCK_HIT_TRAY_BUTTON;
    int32_t on_quick_settings_button =
        shell->quick_settings_open && dock_hit.type == REACH_DOCK_HIT_QUICK_SETTINGS_BUTTON;
    int32_t on_power_button = shell->context_menu_state.open &&
                              shell->context_menu_state.power_open &&
                              dock_hit.type == REACH_DOCK_HIT_POWER_BUTTON;

    if (shell->tray_state.popup_open && !on_tray && !on_tray_button)
    {
        reach_shell_set_tray_popup_open(shell, 0);
    }
    if (shell->context_menu_state.open && !on_context && !on_power_button)
    {
        reach_shell_close_context_menu(shell);
        reach_shell_clear_sticky_dock_feedback(shell);
    }
    if (shell->quick_settings_open && !on_quick_settings && !on_quick_settings_button)
    {
        reach_shell_set_quick_settings_open(shell, 0);
        reach_shell_clear_sticky_dock_feedback(shell);
    }
    if (shell->ui.launcher.open && !on_launcher)
    {
        reach_shell_close_launcher(shell);
    }
}

static void reach_shell_on_popup_mouse_down(void *user, int32_t x, int32_t y)
{
    reach_shell *shell = static_cast<reach_shell *>(user);
    if (shell == nullptr)
    {
        return;
    }
    reach_point_i32 point = {x, y};
    reach_shell_handle_global_mouse_down(shell, point);
}

void reach_shell_sync_popup_mouse_hook(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    if (shell->popup_capture.sync_mouse_hook != nullptr)
    {
        int32_t should_hook = shell->tray_state.popup_open || shell->context_menu_state.open ||
                              shell->quick_settings_open || shell->ui.launcher.open;
        (void)shell->popup_capture.sync_mouse_hook(shell->popup_capture.userdata, should_hook,
                                                   reach_shell_on_popup_mouse_down, shell);
    }
}

void reach_shell_close_transient_surfaces(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    reach_shell_close_launcher(shell);
    reach_shell_set_tray_popup_open(shell, 0);
    reach_shell_set_quick_settings_open(shell, 0);
    reach_shell_close_context_menu(shell);
    reach_shell_clear_sticky_dock_feedback(shell);
}
