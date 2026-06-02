#include "shell_internal.h"

static size_t reach_shell_switcher_foreground_index(const reach_shell *shell)
{
    if (shell == nullptr || shell->foreground_window == 0)
    {
        return REACH_MAX_PINNED_APPS;
    }

    for (size_t index = 0; index < shell->open_window_count; ++index)
    {
        if (shell->open_windows[index].id == shell->foreground_window)
        {
            return index;
        }
    }

    return REACH_MAX_PINNED_APPS;
}

reach_result reach_shell_handle_switcher_event(reach_shell *shell, const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (event->type == REACH_UI_EVENT_ALT_TAB_BEGIN)
    {
        reach_shell_close_transient_surfaces(shell);

        if (shell->window_manager.ops.refresh != nullptr)
        {
            (void)shell->window_manager.ops.refresh(shell->window_manager.manager);
            (void)reach_shell_refresh_open_windows(shell, nullptr);
            if (shell->window_manager.ops.foreground != nullptr)
            {
                shell->foreground_window =
                    shell->window_manager.ops.foreground(shell->window_manager.manager);
            }
        }

        shell->switcher_state.open = shell->open_window_count > 0 ? 1 : 0;
        size_t foreground_index = reach_shell_switcher_foreground_index(shell);
        shell->switcher_state.selected_index =
            foreground_index < shell->open_window_count ? foreground_index : 0;
        shell->switcher_state.visible_start = 0;

        reach_shell_update_switcher_visible_start(shell);

        shell->switcher.dirty_flags = 1;
        return REACH_OK;
    }

    if (!shell->switcher_state.open)
    {
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_ALT_TAB_NEXT && shell->open_window_count > 0)
    {
        shell->switcher_state.selected_index =
            (shell->switcher_state.selected_index + 1) % shell->open_window_count;

        reach_shell_update_switcher_visible_start(shell);

        shell->switcher.dirty_flags = 1;
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_ALT_TAB_PREVIOUS && shell->open_window_count > 0)
    {
        shell->switcher_state.selected_index = shell->switcher_state.selected_index == 0
                                                   ? shell->open_window_count - 1
                                                   : shell->switcher_state.selected_index - 1;

        reach_shell_update_switcher_visible_start(shell);

        shell->switcher.dirty_flags = 1;
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_ALT_TAB_CANCEL)
    {
        shell->switcher_state.open = 0;
        shell->switcher.dirty_flags = 1;

        if (shell->switcher.window.ops.hide != nullptr)
        {
            (void)shell->switcher.window.ops.hide(shell->switcher.window.window);
        }

        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_ALT_TAB_COMMIT)
    {
        uintptr_t selected = shell->switcher_state.selected_index < shell->open_window_count
                                 ? shell->open_windows[shell->switcher_state.selected_index].id
                                 : 0;

        shell->switcher_state.open = 0;
        shell->switcher.dirty_flags = 1;

        if (shell->switcher.window.ops.hide != nullptr)
        {
            (void)shell->switcher.window.ops.hide(shell->switcher.window.window);
        }

        if (selected != 0 && shell->window_manager.ops.activate != nullptr)
        {
            (void)shell->window_manager.ops.activate(shell->window_manager.manager, selected);
        }

        return REACH_OK;
    }

    return REACH_OK;
}
