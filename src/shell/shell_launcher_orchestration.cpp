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

void reach_shell_close_launcher(reach_shell *shell)
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
    shell->dirty.layout = 1;
    shell->launcher.dirty_flags = 1;
    reach_shell_sync_popup_mouse_hook(shell);
    if (shell->launcher.window.ops.hide != nullptr)
    {
        (void)shell->launcher.window.ops.hide(shell->launcher.window.window);
    }
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
