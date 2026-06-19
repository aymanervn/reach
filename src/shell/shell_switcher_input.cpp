#include "shell_internal.h"

static int32_t reach_shell_switcher_open_window_index(const reach_shell *shell, uintptr_t window_id,
                                                      size_t *out_index)
{
    if (shell == nullptr || window_id == 0)
    {
        return 0;
    }

    for (size_t index = 0; index < shell->open_window_count; ++index)
    {
        if (shell->open_windows[index].id == window_id)
        {
            if (out_index != nullptr)
            {
                *out_index = index;
            }
            return 1;
        }
    }
    return 0;
}

static int32_t reach_shell_switcher_contains(const reach_shell_switcher_state *state,
                                             uintptr_t window_id)
{
    if (state == nullptr || window_id == 0)
    {
        return 0;
    }

    for (size_t index = 0; index < state->window_count; ++index)
    {
        if (state->windows[index] == window_id)
        {
            return 1;
        }
    }
    return 0;
}

static void reach_shell_switcher_append_window(reach_shell *shell, uintptr_t window_id)
{
    if (shell == nullptr || window_id == 0 ||
        shell->switcher_state.window_count >= REACH_MAX_PINNED_APPS ||
        reach_shell_switcher_contains(&shell->switcher_state, window_id) ||
        !reach_shell_switcher_open_window_index(shell, window_id, nullptr))
    {
        return;
    }

    shell->switcher_state.windows[shell->switcher_state.window_count++] = window_id;
}

static void reach_shell_rebuild_switcher_windows(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    shell->switcher_state.window_count = 0;
    for (size_t index = 0; index < REACH_MAX_PINNED_APPS; ++index)
    {
        shell->switcher_state.windows[index] = 0;
    }

    reach_shell_switcher_append_window(shell, shell->foreground_window);
    for (size_t index = 0; index < shell->focus_history_count; ++index)
    {
        reach_shell_switcher_append_window(shell, shell->focus_history[index]);
    }
    for (size_t index = 0; index < shell->open_window_count; ++index)
    {
        reach_shell_switcher_append_window(shell, shell->open_windows[index].id);
    }
}

static int32_t reach_shell_switcher_window_index(const reach_shell_switcher_state *state,
                                                 uintptr_t window_id, size_t *out_index)
{
    if (state == nullptr || window_id == 0)
    {
        return 0;
    }

    for (size_t index = 0; index < state->window_count; ++index)
    {
        if (state->windows[index] == window_id)
        {
            if (out_index != nullptr)
            {
                *out_index = index;
            }
            return 1;
        }
    }
    return 0;
}

void reach_shell_refresh_switcher_windows(reach_shell *shell)
{
    if (shell == nullptr || !shell->switcher_state.open)
    {
        return;
    }

    uintptr_t selected_window = 0;
    size_t old_selected = shell->switcher_state.selected_index;
    if (old_selected < shell->switcher_state.window_count)
    {
        selected_window = shell->switcher_state.windows[old_selected];
    }

    reach_shell_rebuild_switcher_windows(shell);
    if (shell->switcher_state.window_count == 0)
    {
        shell->switcher_state.open = 0;
        shell->switcher_state.selected_index = 0;
        shell->switcher_state.visible_start = 0;
        reach_animation_manager_reset(&shell->animations, REACH_SHELL_ANIMATION_SWITCHER_WIDTH);
        shell->switcher.dirty_flags = 1;
        return;
    }

    size_t selected_index = 0;
    if (reach_shell_switcher_window_index(&shell->switcher_state, selected_window, &selected_index))
    {
        shell->switcher_state.selected_index = selected_index;
    }
    else if (old_selected < shell->switcher_state.window_count)
    {
        shell->switcher_state.selected_index = old_selected;
    }
    else
    {
        shell->switcher_state.selected_index = shell->switcher_state.window_count - 1;
    }

    reach_shell_update_switcher_visible_start(shell);
    shell->switcher.dirty_flags = 1;
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
                reach_shell_note_foreground_window(
                    shell, shell->window_manager.ops.foreground(shell->window_manager.manager));
            }
        }

        reach_shell_rebuild_switcher_windows(shell);
        shell->switcher_state.open = shell->switcher_state.window_count > 0 ? 1 : 0;
        shell->switcher_state.selected_index = shell->switcher_state.window_count > 1 ? 1 : 0;
        shell->switcher_state.visible_start = 0;
        reach_animation_manager_reset(&shell->animations, REACH_SHELL_ANIMATION_SWITCHER_WIDTH);

        reach_shell_update_switcher_visible_start(shell);

        shell->switcher.dirty_flags = 1;
        return REACH_OK;
    }

    if (!shell->switcher_state.open)
    {
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_ALT_TAB_NEXT && shell->switcher_state.window_count > 0)
    {
        shell->switcher_state.selected_index =
            (shell->switcher_state.selected_index + 1) % shell->switcher_state.window_count;

        reach_shell_update_switcher_visible_start(shell);

        shell->switcher.dirty_flags = 1;
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_ALT_TAB_PREVIOUS && shell->switcher_state.window_count > 0)
    {
        shell->switcher_state.selected_index = shell->switcher_state.selected_index == 0
                                                   ? shell->switcher_state.window_count - 1
                                                   : shell->switcher_state.selected_index - 1;

        reach_shell_update_switcher_visible_start(shell);

        shell->switcher.dirty_flags = 1;
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_ALT_TAB_CANCEL)
    {
        shell->switcher_state.open = 0;
        reach_animation_manager_reset(&shell->animations, REACH_SHELL_ANIMATION_SWITCHER_WIDTH);
        shell->switcher.dirty_flags = 1;

        if (shell->switcher.window.ops.hide != nullptr)
        {
            (void)shell->switcher.window.ops.hide(shell->switcher.window.window);
        }

        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_ALT_TAB_COMMIT)
    {
        uintptr_t window = 0;
        if (shell->switcher_state.selected_index < shell->switcher_state.window_count)
        {
            window = shell->switcher_state.windows[shell->switcher_state.selected_index];
        }

        shell->switcher_state.open = 0;
        reach_animation_manager_reset(&shell->animations, REACH_SHELL_ANIMATION_SWITCHER_WIDTH);
        shell->switcher.dirty_flags = 1;

        if (shell->switcher.window.ops.hide != nullptr)
        {
            (void)shell->switcher.window.ops.hide(shell->switcher.window.window);
        }

        if (window != 0)
        {
            (void)reach_shell_schedule_window_control(shell, REACH_SHELL_WINDOW_CONTROL_ACTIVATE,
                                                      window);
        }

        return REACH_OK;
    }

    return REACH_OK;
}
