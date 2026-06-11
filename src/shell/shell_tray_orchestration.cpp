#include "shell_internal.h"

void reach_shell_set_tray_popup_open(reach_shell *shell, int32_t open)
{
    if (shell == nullptr)
    {
        return;
    }

    int32_t next_open = open ? 1 : 0;
    if (shell->tray_state.popup_open == next_open)
    {
        return;
    }

    int32_t was_open = shell->tray_state.popup_open;
    shell->tray_state.popup_open = next_open;
    if (shell->tray_state.popup_open)
    {
        reach_shell_set_quick_settings_open(shell, 0);
        reach_shell_close_context_menu(shell);
        (void)reach_shell_refresh_tray_items(shell);
    }
    else if (was_open)
    {
        reach_shell_schedule_dock_reveal_recheck(shell);
    }
    reach_shell_sync_popup_mouse_hook(shell);
    shell->dock.dirty_flags = 1;
    shell->tray.dirty_flags = 1;
}

void reach_shell_toggle_tray_popup(reach_shell *shell)
{
    if (shell != nullptr)
    {
        reach_shell_set_tray_popup_open(shell, !shell->tray_state.popup_open);
    }
}

reach_result reach_shell_refresh_tray_items(reach_shell *shell)
{
    return shell != nullptr
               ? reach_tray_model_refresh(&shell->tray_state.model, &shell->tray_provider)
               : REACH_OK;
}

void reach_shell_compute_tray_popup_layout(reach_shell *shell, const reach_dock_layout *dock_layout,
                                           reach_rect_f32 *out_bounds, reach_rect_f32 *out_slots)
{
    if (shell == nullptr || dock_layout == nullptr || out_bounds == nullptr)
    {
        return;
    }

    (void)out_slots;
    const reach_theme *theme = shell->theme != nullptr ? shell->theme : reach_theme_default();
    reach_tray_compute_popup_layout(&shell->tray_state.model, theme, dock_layout,
                                    reach_shell_layout_dpi_scale(shell), out_bounds);
}

reach_result reach_shell_execute_tray_action(reach_shell *shell, reach_tray_feature_action action)
{
    if (shell == nullptr || action.type != REACH_TRAY_FEATURE_ACTION_ACTIVATE)
    {
        return REACH_OK;
    }
    if (shell->tray_provider.ops.activate == nullptr)
    {
        return REACH_OK;
    }

    reach_result result = shell->tray_provider.ops.activate(shell->tray_provider.provider,
                                                            action.item_id, action.provider_action);
    reach_shell_release_tray_item(shell);
    return result;
}
