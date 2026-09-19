#include "host_internal.h"

static reach_result reach_host_schedule_app_launch_from(reach_host *host,
                                                        const reach_app_launch_request *request,
                                                        uint32_t pin_id, const uint16_t *app_name)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    uint64_t request_id = 0;
    reach_result result =
        reach_app_control_schedule_launch(host->app_control, request, &request_id);
    if (result == REACH_OK)
    {
        if (pin_id != 0)
        {
            reach_host_app_launch_context context = {};
            context.request_id = request_id;
            context.pin_id = pin_id;
            reach_copy_utf16(context.app_name, REACH_APPLICATION_TEXT_CAPACITY, app_name);
            host->app_launch_contexts.push_back(context);
        }
        reach_host_request_update(host);
    }
    return result;
}

reach_result reach_host_schedule_app_launch(reach_host *host,
                                            const reach_app_launch_request *request)
{
    return reach_host_schedule_app_launch_from(host, request, 0, nullptr);
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
    host->app_launch_contexts.clear();
    host->pending_unpins.clear();
}

void reach_host_apply_app_launch_results(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }
    reach_app_launch_completion completion = {};
    while (reach_app_control_take_launch_completion(host->app_control, &completion))
    {
        reach_host_app_launch_context context = {};
        int32_t has_context = 0;
        for (auto it = host->app_launch_contexts.begin(); it != host->app_launch_contexts.end();
             ++it)
        {
            if (it->request_id == completion.request_id)
            {
                context = *it;
                host->app_launch_contexts.erase(it);
                has_context = 1;
                break;
            }
        }
        if (!has_context || completion.result == REACH_OK ||
            completion.failure != REACH_APP_LAUNCH_FAILURE_NOT_FOUND)
        {
            continue;
        }
        reach_feature_notification notification = {};
        notification.kind = REACH_FEATURE_NOTIFICATION_APP_LAUNCH_FAILED;
        notification.app_launch_failure = completion.failure;
        reach_copy_utf16(notification.app_name, REACH_APPLICATION_TEXT_CAPACITY, context.app_name);
        reach_host_notify_registered_features(host, &notification);
        int32_t pending = 0;
        for (const reach_host_pending_unpin &unpin : host->pending_unpins)
        {
            if (unpin.pin_id == context.pin_id)
            {
                pending = 1;
                break;
            }
        }
        if (!pending)
        {
            host->pending_unpins.push_back({context.pin_id, 0.0});
        }
    }
}

void reach_host_tick_pending_unpins(reach_host *host, double delta_seconds)
{
    if (host == nullptr)
    {
        return;
    }
    if (delta_seconds < 0.0)
    {
        delta_seconds = 0.0;
    }
    for (auto it = host->pending_unpins.begin(); it != host->pending_unpins.end();)
    {
        it->elapsed_seconds += delta_seconds;
        if (it->elapsed_seconds < 0.5)
        {
            ++it;
            continue;
        }
        (void)reach_host_unpin_id(host, it->pin_id);
        it = host->pending_unpins.erase(it);
    }
    if (!host->pending_unpins.empty())
    {
        reach_host_request_update(host);
    }
}

reach_result reach_host_schedule_window_control(reach_host *host,
                                                reach_window_control_action action,
                                                uintptr_t window_id, uint64_t *out_request_id)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_result result =
        reach_app_control_schedule_window(host->app_control, action, window_id, out_request_id);
    if (result == REACH_OK)
    {
        reach_host_request_update(host);
    }
    return result;
}

reach_result reach_host_schedule_window_controls(reach_host *host,
                                                 reach_window_control_action action,
                                                 const uintptr_t *window_ids, size_t window_count,
                                                 uint64_t *out_request_id)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_result result = reach_app_control_schedule_windows(host->app_control, action, window_ids,
                                                             window_count, out_request_id);
    if (result == REACH_OK)
    {
        reach_host_request_update(host);
    }
    return result;
}

reach_result reach_host_schedule_minimize_open_windows(reach_host *host, uint64_t *out_request_id)
{
    if (out_request_id != nullptr)
    {
        *out_request_id = 0;
    }
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
    return window_count > 0
               ? reach_host_schedule_window_controls(host, REACH_WINDOW_CONTROL_MINIMIZE, windows,
                                                     window_count, out_request_id)
               : REACH_OK;
}

static void reach_host_resolve_close_handoff(reach_host *host, uint64_t request_id)
{
    if (request_id == 0)
    {
        return;
    }
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_feature_runtime *runtime = &host->feature_runtimes[index];
        if (runtime->surface == nullptr || !runtime->surface->close_handoff_pending ||
            runtime->surface->close_handoff_request_id != request_id)
        {
            continue;
        }
        runtime->surface->close_handoff_pending = 0;
        runtime->surface->close_handoff_request_id = 0;
        if (runtime->definition != nullptr && runtime->definition->capsule_ops != nullptr &&
            runtime->definition->capsule_ops->set_close_handoff_pending != nullptr)
        {
            reach_feature_tick_result tick = {};
            runtime->definition->capsule_ops->set_close_handoff_pending(runtime->capsule, 0, &tick);
            reach_host_apply_feature_tick_result(host, runtime, &tick);
        }
    }
}

void reach_host_apply_window_control_result(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_window_control_completion completion = {};
    while (reach_app_control_take_window_completion(host->app_control, &completion))
    {
        host->dirty.z_order = 1;
        reach_host_end_programmatic_window_manipulation(host);
        reach_host_refresh_window_world(host);
        reach_host_apply_foreground_change(host);
        reach_host_resolve_close_handoff(host, completion.request_id);

        if (completion.result == REACH_OK)
        {
            host->surfaces[REACH_SURFACE_ID_DOCK].dirty_flags = 1;
        }
    }
}

reach_result reach_host_defer_launch_until_surface_closed(reach_host *host, reach_surface_id source,
                                                          const reach_app_launch_request *request,
                                                          uint32_t pin_id, const uint16_t *app_name)
{
    if (host == nullptr || request == nullptr ||
        (request->path[0] == 0 && request->app_user_model_id[0] == 0) ||
        source >= REACH_HOST_SURFACE_COUNT)
    {
        return REACH_INVALID_ARGUMENT;
    }

    host->deferred_launch.request = *request;
    host->deferred_launch.surface = source;
    host->deferred_launch.pin_id = pin_id;
    reach_copy_utf16(host->deferred_launch.app_name, REACH_APPLICATION_TEXT_CAPACITY, app_name);
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
    if (reach_host_surface_is_open(source) || source->presentation_visible)
    {
        return;
    }

    reach_host_deferred_launch deferred = host->deferred_launch;
    host->deferred_launch = {};
    (void)reach_host_schedule_app_launch_from(host, &deferred.request, deferred.pin_id,
                                              deferred.app_name);
}

reach_result reach_host_pin_feature_target(reach_host *host, const reach_feature_target *target,
                                           uintptr_t window_id)
{
    if (host == nullptr || target == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_pinned_app_model app = {};
    const reach_window_snapshot *window =
        window_id != 0 ? reach_window_tracking_window_by_id(host->window_tracking, window_id)
                       : nullptr;
    if (window != nullptr && host->application_resolver.ops.resolve != nullptr)
    {
        reach_application_observation observation = {};
        observation.process_id = window->process_id;
        const uint16_t *runtime_path =
            reach_application_identity_primary_runtime_path(&window->identity);
        (void)reach_copy_utf16(observation.runtime_path, REACH_APPLICATION_TEXT_CAPACITY,
                               runtime_path);
        (void)reach_copy_utf16(observation.app_user_model_id, REACH_APPLICATION_TEXT_CAPACITY,
                               window->identity.app_user_model_id);
        (void)reach_copy_utf16(observation.icon_ref, REACH_APPLICATION_TEXT_CAPACITY,
                               window->icon_ref);
        if (host->application_resolver.ops.resolve(host->application_resolver.resolver,
                                                   &observation, &app.application) == REACH_OK)
        {
            return reach_host_pin_app(host, &app);
        }
    }
    app = {};
    if (target->path != nullptr)
    {
        app.application.launch.kind = target->launch_kind != REACH_APPLICATION_LAUNCH_NONE
                                          ? target->launch_kind
                                          : REACH_APPLICATION_LAUNCH_EXECUTABLE;
        (void)reach_copy_utf16(app.application.launch.path, 260, target->path);
        (void)reach_application_identity_add_runtime_path(&app.application.identity, target->path);
    }
    if (target->icon_ref != nullptr)
    {
        (void)reach_copy_utf16(app.application.icon_ref, 260, target->icon_ref);
    }
    if (target->app_user_model_id != nullptr)
    {
        (void)reach_copy_utf16(app.application.identity.app_user_model_id, 260,
                               target->app_user_model_id);
    }
    return app.application.launch.path[0] != 0 ? reach_host_pin_app(host, &app) : REACH_ERROR;
}

reach_result reach_host_open_feature_target(reach_host *host, reach_surface_id source,
                                            const reach_feature_target *target, uint32_t flags,
                                            uint32_t pin_id)
{
    if (host == nullptr || target == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const int32_t defer = (flags & REACH_FEATURE_ACTION_FLAG_DEFER_UNTIL_CLOSED) != 0;
    const int32_t new_instance = (flags & REACH_FEATURE_ACTION_FLAG_NEW_INSTANCE) != 0;
    const int32_t run_as_admin = (flags & REACH_FEATURE_ACTION_FLAG_RUN_AS_ADMIN) != 0;

    switch (target->kind)
    {
    case REACH_FEATURE_TARGET_APP:
        return (target->path != nullptr && target->path[0] != 0) ||
                       (target->app_user_model_id != nullptr && target->app_user_model_id[0] != 0)
                   ? reach_host_open_app(host, target->path, target->arguments,
                                         target->app_user_model_id, target->launch_kind,
                                         new_instance, run_as_admin, source, defer, pin_id,
                                         target->display_name)
                   : REACH_OK;

    case REACH_FEATURE_TARGET_PATH:
        return target->path != nullptr && target->path[0] != 0
                   ? reach_host_launch_app(host, target->path, target->arguments, nullptr,
                                           REACH_APPLICATION_LAUNCH_NONE, new_instance,
                                           run_as_admin, source, defer, pin_id,
                                           target->display_name)
                   : REACH_OK;

    case REACH_FEATURE_TARGET_TERMINAL_COMMAND:
        return reach_host_schedule_terminal_command(host, target->path);

    case REACH_FEATURE_TARGET_LOCATION:
        return reach_app_control_schedule_open_location(
            host->app_control, REACH_APP_CONTROL_LOCATION_PATH, target->path);

    case REACH_FEATURE_TARGET_SHELL_LOCATION:
        return reach_app_control_schedule_open_location(
            host->app_control, REACH_APP_CONTROL_LOCATION_SHELL, target->path);

    case REACH_FEATURE_TARGET_DEFAULT_LOCATION:
        return reach_app_control_schedule_open_location(
            host->app_control, REACH_APP_CONTROL_LOCATION_DEFAULT, nullptr);

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
        (void)reach_application_identity_add_runtime_path(&app.application.identity, path);
    }
    if (app_user_model_id != nullptr)
    {
        (void)reach_copy_utf16(app.application.identity.app_user_model_id, 260, app_user_model_id);
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

    reach_result result = REACH_OK;
    if (minimize_if_foreground &&
        reach_window_tracking_window_is_foreground(host->window_tracking, window_id) &&
        !reach_host_window_is_minimized(host, window_id))
    {
        result = reach_host_schedule_window_control(host, REACH_WINDOW_CONTROL_MINIMIZE, window_id,
                                                    nullptr);
    }
    else
    {
        result = reach_host_schedule_window_control(host, REACH_WINDOW_CONTROL_ACTIVATE, window_id,
                                                    nullptr);
    }

    host->surfaces[REACH_SURFACE_ID_DOCK].dirty_flags = 1;
    host->surfaces[REACH_SURFACE_ID_SWITCHER].dirty_flags = 1;
    return result;
}

reach_result reach_host_launch_app(reach_host *host, const uint16_t *path,
                                   const uint16_t *arguments, const uint16_t *app_user_model_id,
                                   reach_application_launch_kind launch_kind,
                                   int32_t force_new_instance, int32_t run_as_admin,
                                   reach_surface_id source, int32_t defer_until_closed,
                                   uint32_t pin_id, const uint16_t *app_name)
{
    if (host == nullptr || ((path == nullptr || path[0] == 0) &&
                            (app_user_model_id == nullptr || app_user_model_id[0] == 0)))
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_app_launch_request request = {};
    if (path != nullptr)
    {
        reach_copy_utf16(request.path, 260, path);
    }

    if (arguments != nullptr)
    {
        reach_copy_utf16(request.arguments, 260, arguments);
    }

    if (app_user_model_id != nullptr)
    {
        reach_copy_utf16(request.app_user_model_id, 260, app_user_model_id);
    }

    request.launch_kind = launch_kind;
    request.force_new_instance = force_new_instance ? 1 : 0;
    request.run_as_admin = run_as_admin ? 1 : 0;

    return defer_until_closed
               ? reach_host_defer_launch_until_surface_closed(host, source, &request, pin_id,
                                                              app_name)
               : reach_host_schedule_app_launch_from(host, &request, pin_id, app_name);
}

reach_result reach_host_open_app(reach_host *host, const uint16_t *path, const uint16_t *arguments,
                                 const uint16_t *app_user_model_id,
                                 reach_application_launch_kind launch_kind,
                                 int32_t force_new_instance, int32_t run_as_admin,
                                 reach_surface_id source, int32_t defer_until_closed,
                                 uint32_t pin_id, const uint16_t *app_name)
{
    if (host == nullptr || ((path == nullptr || path[0] == 0) &&
                            (app_user_model_id == nullptr || app_user_model_id[0] == 0)))
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

    return reach_host_launch_app(host, path, arguments, app_user_model_id, launch_kind,
                                 force_new_instance, run_as_admin, source, defer_until_closed,
                                 pin_id, app_name);
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
