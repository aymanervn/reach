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

reach_result reach_host_schedule_window_controls(reach_host *host,
                                                 reach_window_control_action action,
                                                 const uintptr_t *window_ids, size_t window_count)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_result result =
        reach_app_control_schedule_windows(host->app_control, action, window_ids, window_count);
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

    uintptr_t windows[REACH_MAX_OPEN_WINDOWS] = {};
    size_t window_count = reach_window_tracking_collect_unminimized(host->window_tracking, windows,
                                                                    REACH_MAX_OPEN_WINDOWS);
    return window_count > 0 ? reach_host_schedule_window_controls(
                                  host, REACH_WINDOW_CONTROL_MINIMIZE, windows, window_count)
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

    reach_host_end_programmatic_window_manipulation(host);
    reach_host_refresh_window_world(host);

    if (result == REACH_OK)
    {
        host->dock.dirty_flags = 1;
    }
}

reach_result reach_host_defer_launch_until_surface_closed(reach_host *host, reach_surface_id source,
                                                          const reach_app_launch_request *request)
{
    if (host == nullptr || request == nullptr || request->path[0] == 0 ||
        source >= REACH_HOST_SURFACE_COUNT)
    {
        return REACH_INVALID_ARGUMENT;
    }

    host->deferred_launch.request = *request;
    host->deferred_launch.surface = source;
    host->deferred_launch.active = 1;
    reach_host_close_registered_surface(host, source, REACH_SURFACE_CLOSE_SUPERSEDED);
    reach_host_request_update(host);
    return REACH_OK;
}

void reach_host_process_deferred_launch(reach_host *host)
{
    if (host == nullptr || !host->deferred_launch.active)
    {
        return;
    }
    const reach_feature_runtime *source = &host->feature_runtimes[host->deferred_launch.surface];
    if (reach_host_surface_is_open(source) ||
        reach_host_surface_transition_visible(source->transition) ||
        reach_host_surface_transition_active(host, source->transition))
    {
        return;
    }

    reach_app_launch_request request = host->deferred_launch.request;
    host->deferred_launch = {};
    (void)reach_host_schedule_app_launch(host, &request);
}

reach_result reach_host_open_default_location(reach_host *host)
{
    if (host == nullptr || host->explorer_service.service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (host->explorer_service.ops.open_default != nullptr)
    {
        return host->explorer_service.ops.open_default(host->explorer_service.service);
    }
    return REACH_OK;
}

reach_result reach_host_open_feature_target(reach_host *host, reach_surface_id source,
                                            const reach_feature_target *target, int32_t defer)
{
    if (host == nullptr || target == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    switch (target->kind)
    {
    case REACH_FEATURE_TARGET_APP:
        return target->path != nullptr && target->path[0] != 0
                   ? reach_host_open_app(host, target->path, target->arguments, nullptr, 0, source,
                                         defer)
                   : REACH_OK;

    case REACH_FEATURE_TARGET_PATH:
        return target->path != nullptr && target->path[0] != 0
                   ? reach_host_launch_app(host, target->path, target->arguments, 0, 0, source,
                                           defer)
                   : REACH_OK;

    case REACH_FEATURE_TARGET_TERMINAL_COMMAND:
        return reach_host_schedule_terminal_command(host, target->path);

    case REACH_FEATURE_TARGET_LOCATION:
        if (host->explorer_service.ops.path_exists != nullptr &&
            host->explorer_service.ops.path_exists(host->explorer_service.service, target->path) &&
            host->explorer_service.ops.open_path != nullptr)
        {
            return host->explorer_service.ops.open_path(host->explorer_service.service,
                                                        target->path);
        }
        return reach_host_open_default_location(host);

    case REACH_FEATURE_TARGET_SHELL_LOCATION:
        if (host->explorer_service.ops.open_shell_location != nullptr)
        {
            return host->explorer_service.ops.open_shell_location(host->explorer_service.service,
                                                                  target->path);
        }
        return reach_host_open_default_location(host);

    case REACH_FEATURE_TARGET_DEFAULT_LOCATION:
        return reach_host_open_default_location(host);

    case REACH_FEATURE_TARGET_NONE:
    default:
        return REACH_OK;
    }
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

    uintptr_t foreground = reach_host_foreground_window(host);

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
                                   int32_t run_as_admin, reach_surface_id source,
                                   int32_t defer_until_closed)
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

    return defer_until_closed ? reach_host_defer_launch_until_surface_closed(host, source, &request)
                              : reach_host_schedule_app_launch(host, &request);
}

reach_result reach_host_open_app(reach_host *host, const uint16_t *path, const uint16_t *arguments,
                                 const uint16_t *app_user_model_id, int32_t force_new_instance,
                                 reach_surface_id source, int32_t defer_until_closed)
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

    return reach_host_launch_app(host, path, arguments, force_new_instance, 0, source,
                                 defer_until_closed);
}

reach_result reach_host_open_pinned_app(reach_host *host, size_t pinned_index,
                                        int32_t force_new_instance, reach_surface_id source,
                                        int32_t defer_until_closed)
{
    if (host == nullptr || pinned_index >= host->pinned_app_count)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_pinned_app_model *app = &host->pinned_apps[pinned_index];
    return reach_host_open_app(host, app->path, app->arguments, app->app_user_model_id,
                               force_new_instance, source, defer_until_closed);
}

reach_result reach_host_open_pinned_app_id(reach_host *host, uint32_t pin_id,
                                           int32_t force_new_instance, reach_surface_id source,
                                           int32_t defer_until_closed)
{
    if (host == nullptr || pin_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < host->pinned_app_count; ++index)
    {
        if (host->pinned_apps[index].id == pin_id)
        {
            return reach_host_open_pinned_app(host, index, force_new_instance, source,
                                              defer_until_closed);
        }
    }

    return REACH_OK;
}

reach_result reach_host_schedule_open_terminal(reach_host *host)
{
    return reach_host_schedule_terminal_command(host, (const uint16_t *)L"");
}

reach_result reach_host_schedule_terminal_command(reach_host *host, const uint16_t *command)
{
    if (host == nullptr || command == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_terminal_launch_request request = {};
    reach_copy_utf16(request.command, REACH_TERMINAL_COMMAND_CAPACITY, command);
    reach_result result = reach_app_control_schedule_terminal_launch(host->app_control, &request);
    if (result == REACH_OK)
    {
        reach_host_request_update(host);
    }
    return result;
}
