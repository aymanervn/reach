#include "shell_internal.h"

static int32_t reach_shell_detect_game_mode(const reach_shell *shell)
{
    if (shell == nullptr || shell->window_manager.manager == nullptr) {
        return 0;
    }

    return shell->window_manager.ops.foreground_is_exclusive_fullscreen != nullptr &&
        shell->window_manager.ops.foreground_is_exclusive_fullscreen(shell->window_manager.manager);
}

static void reach_shell_close_transient_ui_for_game_mode(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }

    reach_shell_set_tray_popup_open(shell, 0);
    reach_shell_set_quick_settings_open(shell, 0);
    reach_shell_close_context_menu(shell);
    reach_shell_cancel_launcher_search(shell);

    if (shell->ui.launcher.open) {
        (void)reach_ui_state_close_launcher(&shell->ui);
    }

    shell->dock_reveal_requested = 0;
    shell->dock_reveal_check_dirty = 0;

    shell->launcher.dirty_flags = 1;
    shell->tray.dirty_flags = 1;
    shell->context_menu.dirty_flags = 1;
    shell->quick_settings.dirty_flags = 1;
    shell->dock.dirty_flags = 1;
    shell->dirty.render = 1;
}

int32_t reach_shell_game_mode_enabled(const reach_shell *shell)
{
    return shell != nullptr &&
        reach_runtime_policy_game_mode_enabled(&shell->runtime_policy);
}

reach_result reach_shell_update_game_mode(reach_shell *shell)
{
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    int32_t next_active = reach_shell_detect_game_mode(shell);
    int32_t current_active =
        reach_runtime_policy_game_mode_enabled(&shell->runtime_policy);

    if (next_active == current_active) {
        return REACH_OK;
    }

    reach_runtime_policy_set_game_mode(&shell->runtime_policy, next_active);

    if (next_active) {
        reach_shell_close_transient_ui_for_game_mode(shell);
    } else {
        shell->dirty.layout = 1;
        shell->dirty.render = 1;
        shell->dock.dirty_flags = 1;
        shell->launcher.dirty_flags = 1;
        shell->tray.dirty_flags = 1;
        shell->switcher.dirty_flags = 1;
        shell->context_menu.dirty_flags = 1;
        shell->quick_settings.dirty_flags = 1;
    }

    return REACH_OK;
}
