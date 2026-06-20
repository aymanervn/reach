#include "shell_internal.h"

static int32_t reach_shell_detect_game_mode(const reach_shell *shell)
{
    if (shell != nullptr && shell->window_manager.ops.game_mode_active != nullptr)
    {
        return shell->window_manager.ops.game_mode_active(shell->window_manager.manager);
    }
    return 0;
}

static void reach_shell_close_transient_ui_for_game_mode(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    reach_ui_event cancel = {};
    cancel.type = REACH_UI_EVENT_POINTER_CANCEL;
    (void)reach_shell_handle_event(shell, &cancel);
    reach_shell_close_transient_surfaces(shell, 1);
    reach_shell_set_clipboard_open(shell, 0);

    shell->switcher_state.open = 0;
    shell->switcher_state.selected_index = 0;
    shell->switcher_state.visible_start = 0;
    reach_animation_manager_init(&shell->animations, shell->animation_tracks,
                                 REACH_SHELL_ANIMATION_COUNT);
    reach_shell_surface_transitions_init(shell);
    reach_shell_clear_dock_item_x_animations(shell);
    shell->dock_animation.initialized = 0;
    shell->dock_width.initialized = 0;
    shell->dock_reveal.reveal_session_active = 0;
    shell->dock_reveal.pointer_sequence_active = 0;
    shell->dock_reveal.edge_visible = 0;
    shell->dock_reveal.edge_bounds_valid = 0;
    if (shell->dock_reveal_edge.ops.hide != nullptr)
    {
        (void)shell->dock_reveal_edge.ops.hide(shell->dock_reveal_edge.edge);
    }

    shell->launcher.dirty_flags = 1;
    shell->tray.dirty_flags = 1;
    shell->switcher.dirty_flags = 1;
    shell->context_menu.dirty_flags = 1;
    shell->quick_settings.dirty_flags = 1;
    shell->clipboard_surface.dirty_flags = 1;
    shell->dock.dirty_flags = 1;
    shell->dirty.render = 1;
}

int32_t reach_shell_game_mode_enabled(const reach_shell *shell)
{
    return shell != nullptr && reach_runtime_policy_game_mode_enabled(&shell->runtime_policy);
}

reach_result reach_shell_update_game_mode(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    int32_t next_active = reach_shell_detect_game_mode(shell);
    int32_t current_active = reach_runtime_policy_game_mode_enabled(&shell->runtime_policy);

    if (next_active == current_active)
    {
        return REACH_OK;
    }

    reach_runtime_policy_set_game_mode(&shell->runtime_policy, next_active);

    if (next_active)
    {
        reach_shell_close_transient_ui_for_game_mode(shell);
    }
    else
    {
        shell->dirty.layout = 1;
        shell->dirty.render = 1;
        shell->dock.dirty_flags = 1;
        shell->launcher.dirty_flags = 1;
        shell->tray.dirty_flags = 1;
        shell->switcher.dirty_flags = 1;
        shell->context_menu.dirty_flags = 1;
        shell->quick_settings.dirty_flags = 1;
        shell->clipboard_surface.dirty_flags = 1;
    }
    reach_shell_sync_pointer_move_subscriptions(shell);

    return REACH_OK;
}
