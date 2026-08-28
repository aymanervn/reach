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

    reach_launcher_cancel_search(
        reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER));
    reach_launcher_clear_query(
        reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER));
    (void)reach_launcher_clear_results(
        reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER));
    reach_launcher_reset_text_edit(
        reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER));
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

    reach_launcher_remember_restore_window(
        reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER),
        reach_host_foreground_window(host));
}

void reach_host_toggle_launcher(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }
    if (!reach_launcher_is_open(
            reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER)))
    {
        host->launcher_restore_pending = 0;
        reach_host_remember_launcher_restore_window(host);
        reach_host_surface_opening(host, REACH_SURFACE_ID_LAUNCHER, REACH_SURFACE_ORIGIN_NONE);
    }
    (void)reach_launcher_toggle(
        reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER));
}

void reach_host_clear_launcher_restore_window(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_launcher_clear_restore_window(
        reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER));
}

static int32_t reach_host_launcher_can_restore_focus_to(reach_host *host, uintptr_t window)
{
    if (host == nullptr || window == 0 || host->window_manager.ops.activate == nullptr)
    {
        return 0;
    }

    return !reach_host_window_is_minimized(host, window);
}

void reach_host_request_launcher_focus_restore(reach_host *host)
{
    if (host != nullptr)
    {
        host->launcher_restore_pending = 1;
    }
}

void reach_host_flush_launcher_focus_restore(reach_host *host)
{
    if (host == nullptr || !host->launcher_restore_pending)
    {
        return;
    }

    host->launcher_restore_pending = 0;
    reach_host_restore_launcher_focus(host);
}

void reach_host_restore_launcher_focus(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    uintptr_t window = reach_launcher_take_restore_window(
        reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER));
    if (window != 0 && reach_host_launcher_can_restore_focus_to(host, window))
    {
        (void)reach_host_schedule_window_control(host, REACH_WINDOW_CONTROL_ACTIVATE, window);
    }
}

static void reach_host_close_launcher_impl(reach_host *host, int32_t restore_focus)
{
    if (host == nullptr || !reach_launcher_is_open(reach_host_feature_capsule<reach_launcher>(
                               host, REACH_SURFACE_ID_LAUNCHER)))
    {
        return;
    }

    (void)reach_launcher_close(
        reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER));
    host->dirty.layout = 1;
    host->launcher.dirty_flags = 1;
    reach_host_surface_transition_set(host, &host->launcher_transition, 0);
    reach_host_sync_popup_mouse_hook(host);
    if (restore_focus)
    {
        reach_host_request_launcher_focus_restore(host);
    }
    else
    {
        host->launcher_restore_pending = 0;
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
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (reach_launcher_result_count(
            reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER)) > 0 &&
        reach_launcher_selected_result_index(
            reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER)) <
            reach_launcher_result_count(
                reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER)))
    {
        const reach_launcher_result *result = reach_launcher_result_at(
            reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER),
            reach_launcher_selected_result_index(
                reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER)));
        if (result->action == REACH_LAUNCHER_RESULT_RUN_TERMINAL_COMMAND)
        {
            return reach_host_schedule_terminal_command(host, result->payload.terminal_command);
        }

        const reach_search_candidate *search = &result->payload.search;
        if (search->path[0] == 0)
        {
            return REACH_OK;
        }
        const uint16_t *arguments = search->arguments[0] != 0 ? search->arguments : nullptr;
        if (search->kind == REACH_SEARCH_RESULT_APP)
        {
            return reach_host_open_app(
                host, search->path, arguments, nullptr, 0,
                reach_launcher_is_open(
                    reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER)));
        }

        return reach_host_launch_app(
            host, search->path, arguments, 0, 0,
            reach_launcher_is_open(
                reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER)));
    }

    if (host->explorer_service.service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const uint16_t *query = reach_launcher_query_text(
        reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER));
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
    if (host == nullptr || !reach_app_control_reveal_available(host->app_control))
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (result_index >= reach_launcher_result_count(reach_host_feature_capsule<reach_launcher>(
                            host, REACH_SURFACE_ID_LAUNCHER)))
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_launcher_result *result = reach_launcher_result_at(
        reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER), result_index);
    if (result->action != REACH_LAUNCHER_RESULT_OPEN_SEARCH)
    {
        return REACH_OK;
    }

    const reach_search_candidate *search = &result->payload.search;
    if (search->kind != REACH_SEARCH_RESULT_APP || search->path[0] == 0)
    {
        return REACH_OK;
    }

    return reach_host_schedule_reveal_path(host, search->path);
}
