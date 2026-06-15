#include "shell_internal.h"

static int32_t reach_utf16_starts_with_ascii_case_insensitive(const uint16_t *text,
                                                              const char *prefix)
{
    if (text == nullptr || prefix == nullptr)
    {
        return 0;
    }

    size_t index = 0;
    while (prefix[index] != 0)
    {
        uint16_t current = text[index];
        char expected = prefix[index];

        if (current >= 'A' && current <= 'Z')
        {
            current = (uint16_t)(current - 'A' + 'a');
        }

        if (expected >= 'A' && expected <= 'Z')
        {
            expected = (char)(expected - 'A' + 'a');
        }

        if (current != (uint16_t)expected)
        {
            return 0;
        }

        ++index;
    }

    return 1;
}

static void reach_shell_copy_parent_path(uint16_t *dst, size_t dst_count, const uint16_t *path)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }

    dst[0] = 0;
    if (path == nullptr || path[0] == 0)
    {
        return;
    }

    size_t length = 0;
    size_t last_separator = 0;
    int32_t has_separator = 0;
    while (path[length] != 0)
    {
        if (path[length] == '\\' || path[length] == '/')
        {
            last_separator = length;
            has_separator = 1;
        }
        ++length;
    }

    if (!has_separator)
    {
        reach_copy_utf16(dst, dst_count, path);
        return;
    }

    size_t copy_length = last_separator;
    if (last_separator == 2 && path[1] == ':')
    {
        copy_length = 3;
    }
    if (copy_length + 1 > dst_count)
    {
        copy_length = dst_count - 1;
    }

    for (size_t index = 0; index < copy_length; ++index)
    {
        dst[index] = path[index];
    }
    dst[copy_length] = 0;
}

void reach_shell_raise_launcher(reach_shell *shell)
{
    if (shell == nullptr || shell->launcher.window.ops.raise == nullptr)
    {
        return;
    }

    (void)shell->launcher.window.ops.raise(shell->launcher.window.window);
}

void reach_shell_notify_launcher_search_ready(reach_shell *shell)
{
    if (shell == nullptr || shell->launcher.window.ops.post_event == nullptr)
    {
        return;
    }

    (void)shell->launcher.window.ops.post_event(shell->launcher.window.window,
                                                REACH_UI_EVENT_LAUNCHER_SEARCH_READY);
}

void reach_shell_remember_launcher_restore_window(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    shell->launcher_restore_window = 0;
    shell->launcher_restore_window_valid = 0;
    if (shell->window_manager.ops.foreground == nullptr)
    {
        return;
    }

    uintptr_t foreground = shell->window_manager.ops.foreground(shell->window_manager.manager);
    if (foreground != 0)
    {
        shell->launcher_restore_window = foreground;
        shell->launcher_restore_window_valid = 1;
    }
}

void reach_shell_clear_launcher_restore_window(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    shell->launcher_restore_window = 0;
    shell->launcher_restore_window_valid = 0;
}

void reach_shell_defer_launcher_close_until_foreground_change(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    reach_shell_clear_launcher_restore_window(shell);
    shell->launcher_close_after_foreground_change = 1;
    reach_shell_request_update(shell);
}

static int32_t reach_shell_launcher_can_restore_focus_to(reach_shell *shell, uintptr_t window)
{
    if (shell == nullptr || window == 0 || shell->window_manager.ops.activate == nullptr)
    {
        return 0;
    }

    return !reach_shell_window_is_minimized(shell, window);
}

void reach_shell_restore_launcher_focus(reach_shell *shell)
{
    if (shell == nullptr || !shell->launcher_restore_window_valid)
    {
        return;
    }

    uintptr_t window = shell->launcher_restore_window;
    reach_shell_clear_launcher_restore_window(shell);
    if (reach_shell_launcher_can_restore_focus_to(shell, window))
    {
        (void)shell->window_manager.ops.activate(shell->window_manager.manager, window);
    }
}

static void reach_shell_close_launcher_impl(reach_shell *shell, int32_t restore_focus)
{
    if (shell == nullptr || !shell->ui.launcher.open)
    {
        return;
    }

    reach_shell_cancel_launcher_search(shell);
    reach_shell_release_launcher_result_icons(shell);
    (void)reach_ui_state_close_launcher(&shell->ui);
    shell->pressed_launcher_hit_type = REACH_LAUNCHER_HIT_NONE;
    shell->pressed_launcher_index = REACH_MAX_PINNED_APPS;
    shell->launcher_close_after_foreground_change = 0;
    shell->dirty.layout = 1;
    shell->launcher.dirty_flags = 1;
    reach_shell_sync_popup_mouse_hook(shell);
    if (shell->launcher.window.ops.hide != nullptr)
    {
        (void)shell->launcher.window.ops.hide(shell->launcher.window.window);
    }
    if (restore_focus)
    {
        reach_shell_restore_launcher_focus(shell);
    }
    else
    {
        reach_shell_clear_launcher_restore_window(shell);
    }
}

void reach_shell_close_launcher(reach_shell *shell)
{
    reach_shell_close_launcher_impl(shell, 1);
}

void reach_shell_close_launcher_without_focus_restore(reach_shell *shell)
{
    reach_shell_close_launcher_impl(shell, 0);
}

reach_result reach_shell_open_launcher_result(reach_shell *shell)
{
    REACH_ASSERT(shell != nullptr);
    if (shell == nullptr || shell->explorer_service.service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (shell->ui.launcher.result_count > 0 &&
        shell->ui.launcher.selected_result_index < shell->ui.launcher.result_count)
    {
        const reach_search_candidate *result =
            &shell->ui.launcher.results[shell->ui.launcher.selected_result_index];
        if (result->path[0] == 0)
        {
            return REACH_OK;
        }
        if (result->kind == REACH_SEARCH_RESULT_APP && shell->app_launcher.ops.launch != nullptr)
        {
            reach_app_launch_request request = {};
            reach_copy_utf16(request.path, 260, result->path);
            return shell->app_launcher.ops.launch(shell->app_launcher.launcher, &request);
        }
        if (shell->explorer_service.ops.open_path != nullptr)
        {
            return shell->explorer_service.ops.open_path(shell->explorer_service.service,
                                                         result->path);
        }
        return REACH_OK;
    }

    const uint16_t *query = shell->ui.launcher.query;
    if (query[0] == 0)
    {
        if (shell->explorer_service.ops.open_default != nullptr)
        {
            return shell->explorer_service.ops.open_default(shell->explorer_service.service);
        }
        return REACH_OK;
    }

    if (reach_utf16_starts_with_ascii_case_insensitive(query, "shell:") &&
        shell->explorer_service.ops.open_shell_location != nullptr)
    {
        return shell->explorer_service.ops.open_shell_location(shell->explorer_service.service,
                                                               query);
    }

    if (shell->explorer_service.ops.path_exists != nullptr &&
        shell->explorer_service.ops.path_exists(shell->explorer_service.service, query) &&
        shell->explorer_service.ops.open_path != nullptr)
    {
        return shell->explorer_service.ops.open_path(shell->explorer_service.service, query);
    }

    if (shell->explorer_service.ops.open_default != nullptr)
    {
        return shell->explorer_service.ops.open_default(shell->explorer_service.service);
    }
    return REACH_OK;
}

reach_result reach_shell_open_launcher_result_path(reach_shell *shell, size_t result_index)
{
    REACH_ASSERT(shell != nullptr);
    if (shell == nullptr || shell->explorer_service.service == nullptr ||
        shell->explorer_service.ops.open_path == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (result_index >= shell->ui.launcher.result_count)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_search_candidate *result = &shell->ui.launcher.results[result_index];
    if (result->kind != REACH_SEARCH_RESULT_APP || result->path[0] == 0)
    {
        return REACH_OK;
    }

    uint16_t parent_path[260] = {};
    reach_shell_copy_parent_path(parent_path, 260, result->path);
    if (parent_path[0] == 0)
    {
        return REACH_OK;
    }

    return shell->explorer_service.ops.open_path(shell->explorer_service.service, parent_path);
}
