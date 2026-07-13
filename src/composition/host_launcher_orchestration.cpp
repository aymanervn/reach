#include "host_internal.h"

#include <math.h>

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

void reach_host_cleanup_closed_launcher(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_launcher_cancel_search(host->launcher_capsule);
    reach_launcher_clear_query(host->launcher_capsule);
    (void)reach_launcher_clear_results(host->launcher_capsule);
    reach_launcher_clear_pressed(host->launcher_capsule);
    reach_launcher_reset_text_edit(host->launcher_capsule);
}

void reach_host_notify_launcher_search_ready(reach_host *host)
{
    if (host == nullptr || host->launcher.window.ops.post_event == nullptr)
    {
        return;
    }

    (void)host->launcher.window.ops.post_event(host->launcher.window.window,
                                                REACH_UI_EVENT_LAUNCHER_SEARCH_READY);
}

void reach_host_remember_launcher_restore_window(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    uintptr_t foreground =
        host->window_manager.ops.foreground != nullptr
            ? host->window_manager.ops.foreground(host->window_manager.manager)
            : 0;
    reach_launcher_remember_restore_window(host->launcher_capsule, foreground);
}

void reach_host_toggle_launcher(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }
    if (!reach_launcher_is_open(host->launcher_capsule))
    {
        reach_host_remember_launcher_restore_window(host);
    }
    (void)reach_launcher_toggle(host->launcher_capsule);
}

void reach_host_clear_launcher_restore_window(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_launcher_clear_restore_window(host->launcher_capsule);
}

static int32_t reach_host_launcher_can_restore_focus_to(reach_host *host, uintptr_t window)
{
    if (host == nullptr || window == 0 || host->window_manager.ops.activate == nullptr)
    {
        return 0;
    }

    return !reach_host_window_is_minimized(host, window);
}

void reach_host_restore_launcher_focus(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    uintptr_t window = reach_launcher_take_restore_window(host->launcher_capsule);
    if (window != 0 && reach_host_launcher_can_restore_focus_to(host, window))
    {
        (void)host->window_manager.ops.activate(host->window_manager.manager, window);
    }
}

static void reach_host_close_launcher_impl(reach_host *host, int32_t restore_focus)
{
    if (host == nullptr || !reach_launcher_is_open(host->launcher_capsule))
    {
        return;
    }

    (void)reach_launcher_close(host->launcher_capsule);
    host->dirty.layout = 1;
    host->launcher.dirty_flags = 1;
    reach_host_surface_transition_set(host, &host->launcher_transition, 0);
    reach_host_sync_popup_mouse_hook(host);
    if (restore_focus)
    {
        reach_host_restore_launcher_focus(host);
    }
    else
    {
        reach_host_clear_launcher_restore_window(host);
    }
}

void reach_host_close_launcher(reach_host *host)
{
    reach_host_close_launcher_impl(host, 1);
}

void reach_host_close_launcher_without_focus_restore(reach_host *host)
{
    reach_host_close_launcher_impl(host, 0);
}

reach_result reach_host_open_launcher_result(reach_host *host)
{
    REACH_ASSERT(host != nullptr);
    if (host == nullptr || host->explorer_service.service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (reach_launcher_result_count(host->launcher_capsule) > 0 &&
        reach_launcher_selected_result_index(host->launcher_capsule) < reach_launcher_result_count(host->launcher_capsule))
    {
        const reach_search_candidate *result =
            reach_launcher_result_at(host->launcher_capsule, reach_launcher_selected_result_index(host->launcher_capsule));
        if (result->path[0] == 0)
        {
            return REACH_OK;
        }
        if (result->kind == REACH_SEARCH_RESULT_APP)
        {
            return reach_host_open_app(host, result->path, nullptr, nullptr, 0,
                                        reach_launcher_is_open(host->launcher_capsule));
        }
        /* Folder/file results open through the app-launch service too
           (ShellExecute on the path) — never a synchronous main-thread open. */
        return reach_host_launch_app(host, result->path, nullptr, 0, 0,
                                     reach_launcher_is_open(host->launcher_capsule));
    }

    const uint16_t *query = reach_launcher_query_text(host->launcher_capsule);
    if (query[0] == 0)
    {
        if (host->explorer_service.ops.open_default != nullptr)
        {
            return host->explorer_service.ops.open_default(host->explorer_service.service);
        }
        return REACH_OK;
    }

    if (reach_utf16_starts_with_ascii_case_insensitive(query, "shell:") &&
        host->explorer_service.ops.open_shell_location != nullptr)
    {
        return host->explorer_service.ops.open_shell_location(host->explorer_service.service,
                                                               query);
    }

    if (host->explorer_service.ops.path_exists != nullptr &&
        host->explorer_service.ops.path_exists(host->explorer_service.service, query) &&
        host->explorer_service.ops.open_path != nullptr)
    {
        return host->explorer_service.ops.open_path(host->explorer_service.service, query);
    }

    if (host->explorer_service.ops.open_default != nullptr)
    {
        return host->explorer_service.ops.open_default(host->explorer_service.service);
    }
    return REACH_OK;
}

reach_result reach_host_reveal_launcher_result(reach_host *host, size_t result_index)
{
    REACH_ASSERT(host != nullptr);
    if (host == nullptr || host->explorer_service.service == nullptr ||
        host->explorer_service.ops.reveal_path == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (result_index >= reach_launcher_result_count(host->launcher_capsule))
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_search_candidate *result = reach_launcher_result_at(host->launcher_capsule, result_index);
    if (result->kind != REACH_SEARCH_RESULT_APP || result->path[0] == 0)
    {
        return REACH_OK;
    }

    return host->explorer_service.ops.reveal_path(host->explorer_service.service, result->path);
}
