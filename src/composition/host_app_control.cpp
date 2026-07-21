#include "host_internal.h"

reach_result reach_host_schedule_app_launch(reach_host *host,
                                            const reach_app_launch_request *request)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_result result = reach_app_control_schedule_launch(host->app_control, request);
    if (result == REACH_OK)
    {
        reach_host_request_update(host);
    }
    return result;
}

reach_result reach_host_schedule_reveal_path(reach_host *host, const uint16_t *path)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_result result = reach_app_control_schedule_reveal(host->app_control, path);
    if (result == REACH_OK)
    {
        reach_host_request_update(host);
    }
    return result;
}

void reach_host_stop_app_control(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }
    reach_app_control_stop(host->app_control);
    host->deferred_launch = {};
}

reach_result reach_host_schedule_window_control(reach_host *host,
                                                reach_window_control_action action,
                                                uintptr_t window_id)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_result result = reach_app_control_schedule_window(host->app_control, action, window_id);
    if (result == REACH_OK)
    {
        reach_host_request_update(host);
    }
    return result;
}

reach_result reach_host_schedule_minimize_windows(reach_host *host, const uintptr_t *window_ids,
                                                  size_t window_count)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_result result =
        reach_app_control_schedule_minimize(host->app_control, window_ids, window_count);
    if (result == REACH_OK)
    {
        reach_host_request_update(host);
    }
    return result;
}

reach_result reach_host_schedule_minimize_open_windows(reach_host *host)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (host->window_manager.ops.refresh != nullptr)
    {
        (void)host->window_manager.ops.refresh(host->window_manager.manager);
    }
    (void)reach_host_refresh_open_windows(host, nullptr);

    uintptr_t windows[REACH_MAX_PINNED_APPS] = {};
    size_t window_count = reach_window_tracking_collect_unminimized(host->window_tracking, windows,
                                                                    REACH_MAX_PINNED_APPS);
    return window_count > 0 ? reach_host_schedule_minimize_windows(host, windows, window_count)
                            : REACH_OK;
}

void reach_host_apply_window_control_result(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_result result = REACH_OK;
    if (!reach_app_control_take_window_completed(host->app_control, &result))
    {
        return;
    }

    if (host->window_manager.ops.refresh != nullptr)
    {
        (void)host->window_manager.ops.refresh(host->window_manager.manager);
        (void)reach_host_refresh_open_windows(host, nullptr);
    }

    if (result == REACH_OK)
    {
        host->dock.dirty_flags = 1;
    }
}

reach_result
reach_host_defer_app_launch_until_launcher_closed(reach_host *host,
                                                  const reach_app_launch_request *request)
{
    if (host == nullptr || request == nullptr || request->path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    host->deferred_launch.request = *request;
    host->deferred_launch.active = 1;
    reach_host_close_launcher_without_focus_restore(host);
    reach_host_request_update(host);
    return REACH_OK;
}

void reach_host_process_deferred_launcher_app_launch(reach_host *host)
{
    if (host == nullptr || !host->deferred_launch.active)
    {
        return;
    }
    if (reach_launcher_is_open(host->launcher_capsule) ||
        reach_host_surface_transition_visible(&host->launcher_transition) ||
        reach_host_surface_transition_active(host, &host->launcher_transition))
    {
        return;
    }

    reach_app_launch_request request = host->deferred_launch.request;
    host->deferred_launch = {};
    (void)reach_host_schedule_app_launch(host, &request);
}

static int32_t reach_host_app_launch_window_matches_app(const reach_window_snapshot *window,
                                                        const uint16_t *path,
                                                        const uint16_t *app_user_model_id)
{

    reach_pinned_app_model app = {};
    if (path != nullptr)
    {
        (void)reach_copy_utf16(app.path, 260, path);
    }
    if (app_user_model_id != nullptr)
    {
        (void)reach_copy_utf16(app.app_user_model_id, 260, app_user_model_id);
    }
    return reach_window_tracking_window_matches_app(&app, window);
}

static uintptr_t reach_host_find_open_app_window(reach_host *host, const uint16_t *path,
                                                 const uint16_t *app_user_model_id)
{
    if (host == nullptr)
    {
        return 0;
    }

    if (host->window_manager.ops.refresh != nullptr)
    {
        (void)host->window_manager.ops.refresh(host->window_manager.manager);
        (void)reach_host_refresh_open_windows(host, nullptr);
    }

    for (size_t index = 0; index < reach_host_open_window_count(host); ++index)
    {
        const reach_window_snapshot *window = &reach_host_open_windows(host)[index];
        if (window->id != 0 &&
            reach_host_app_launch_window_matches_app(window, path, app_user_model_id))
        {
            return window->id;
        }
    }

    return 0;
}

reach_result reach_host_focus_window(reach_host *host, uintptr_t window_id,
                                     int32_t minimize_if_foreground)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (window_id == 0)
    {
        return REACH_OK;
    }

    if (host->window_manager.ops.refresh != nullptr)
    {
        (void)host->window_manager.ops.refresh(host->window_manager.manager);
        (void)reach_host_refresh_open_windows(host, nullptr);
    }

    uintptr_t foreground = host->window_manager.ops.foreground != nullptr
                               ? host->window_manager.ops.foreground(host->window_manager.manager)
                               : 0;

    reach_result result = REACH_OK;
    if (minimize_if_foreground && foreground == window_id &&
        !reach_host_window_is_minimized(host, window_id))
    {
        result = reach_host_schedule_window_control(host, REACH_WINDOW_CONTROL_MINIMIZE, window_id);
    }
    else
    {
        result = reach_host_schedule_window_control(host, REACH_WINDOW_CONTROL_ACTIVATE, window_id);
    }

    host->dock.dirty_flags = 1;
    host->switcher.dirty_flags = 1;
    return result;
}

reach_result reach_host_launch_app(reach_host *host, const uint16_t *path,
                                   const uint16_t *arguments, int32_t force_new_instance,
                                   int32_t run_as_admin, int32_t defer_until_launcher_closed)
{
    if (host == nullptr || path == nullptr || path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_app_launch_request request = {};
    reach_copy_utf16(request.path, 260, path);

    if (arguments != nullptr)
    {
        reach_copy_utf16(request.arguments, 260, arguments);
    }

    request.force_new_instance = force_new_instance ? 1 : 0;
    request.run_as_admin = run_as_admin ? 1 : 0;

    return defer_until_launcher_closed
               ? reach_host_defer_app_launch_until_launcher_closed(host, &request)
               : reach_host_schedule_app_launch(host, &request);
}

reach_result reach_host_open_app(reach_host *host, const uint16_t *path, const uint16_t *arguments,
                                 const uint16_t *app_user_model_id, int32_t force_new_instance,
                                 int32_t defer_until_launcher_closed)
{
    if (host == nullptr || path == nullptr || path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (!force_new_instance)
    {
        uintptr_t window = reach_host_find_open_app_window(host, path, app_user_model_id);
        if (window != 0)
        {
            return reach_host_focus_window(host, window, 0);
        }
    }

    return reach_host_launch_app(host, path, arguments, force_new_instance, 0,
                                 defer_until_launcher_closed);
}

reach_result reach_host_open_pinned_app(reach_host *host, size_t pinned_index,
                                        int32_t force_new_instance,
                                        int32_t defer_until_launcher_closed)
{
    if (host == nullptr || pinned_index >= host->pinned_app_count)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_pinned_app_model *app = &host->pinned_apps[pinned_index];
    return reach_host_open_app(host, app->path, app->arguments, app->app_user_model_id,
                               force_new_instance, defer_until_launcher_closed);
}

reach_result reach_host_open_pinned_app_id(reach_host *host, uint32_t pin_id,
                                           int32_t force_new_instance,
                                           int32_t defer_until_launcher_closed)
{
    if (host == nullptr || pin_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < host->pinned_app_count; ++index)
    {
        if (host->pinned_apps[index].id == pin_id)
        {
            return reach_host_open_pinned_app(host, index, force_new_instance,
                                              defer_until_launcher_closed);
        }
    }

    return REACH_OK;
}

reach_result reach_host_schedule_open_terminal(reach_host *host)
{
    static const uint16_t terminal_path[] = {'w', 't', '.', 'e', 'x', 'e', 0};

    return reach_host_open_app(host, terminal_path, nullptr, nullptr, 1, 0);
}
