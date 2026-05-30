#include "shell_internal.h"

static size_t reach_shell_foreground_open_window_index(reach_shell *shell)
{
    if (shell == nullptr || shell->window_manager.ops.foreground == nullptr) {
        return 0;
    }

    uintptr_t foreground = shell->window_manager.ops.foreground(
        shell->window_manager.manager);

    for (size_t index = 0; index < shell->open_window_count; ++index) {
        if (shell->open_windows[index].id == foreground) {
            return index;
        }
    }

    return 0;
}

reach_result reach_shell_handle_switcher_event(
    reach_shell *shell,
    const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    if (event->type == REACH_UI_EVENT_ALT_TAB_BEGIN) {
        if (shell->window_manager.ops.refresh != nullptr) {
            (void)shell->window_manager.ops.refresh(shell->window_manager.manager);
            (void)reach_shell_refresh_open_windows(shell, nullptr);
        }

        shell->switcher_open = shell->open_window_count > 0 ? 1 : 0;
        shell->switcher_selected_index = reach_shell_foreground_open_window_index(shell);

        reach_shell_update_switcher_visible_start(shell);

        shell->switcher.dirty_flags = 1;
        return REACH_OK;
    }

    if (!shell->switcher_open) {
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_ALT_TAB_NEXT &&
        shell->open_window_count > 0) {
        shell->switcher_selected_index =
            (shell->switcher_selected_index + 1) % shell->open_window_count;

        reach_shell_update_switcher_visible_start(shell);

        shell->switcher.dirty_flags = 1;
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_ALT_TAB_PREVIOUS &&
        shell->open_window_count > 0) {
        shell->switcher_selected_index = shell->switcher_selected_index == 0
            ? shell->open_window_count - 1
            : shell->switcher_selected_index - 1;

        reach_shell_update_switcher_visible_start(shell);

        shell->switcher.dirty_flags = 1;
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_ALT_TAB_CANCEL) {
        shell->switcher_open = 0;
        shell->switcher.dirty_flags = 1;

        if (shell->switcher.window.ops.hide != nullptr) {
            (void)shell->switcher.window.ops.hide(shell->switcher.window.window);
        }

        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_ALT_TAB_COMMIT) {
        uintptr_t selected = shell->switcher_selected_index < shell->open_window_count
            ? shell->open_windows[shell->switcher_selected_index].id
            : 0;

        shell->switcher_open = 0;
        shell->switcher.dirty_flags = 1;

        if (shell->switcher.window.ops.hide != nullptr) {
            (void)shell->switcher.window.ops.hide(shell->switcher.window.window);
        }

        if (selected != 0 && shell->window_manager.ops.activate != nullptr) {
            return shell->window_manager.ops.activate(
                shell->window_manager.manager,
                selected);
        }

        return REACH_OK;
    }

    return REACH_OK;
}
