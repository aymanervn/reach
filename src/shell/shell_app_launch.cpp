#include "shell_internal.h"

static void reach_shell_app_launch_thread_main(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    for (;;)
    {
        reach_app_launch_request request = {};

        {
            std::unique_lock<std::mutex> lock(shell->app_launch.mutex);
            shell->app_launch.cv.wait(
                lock, [shell]() { return shell->app_launch.stop || shell->app_launch.pending; });

            if (shell->app_launch.stop)
            {
                return;
            }

            request = shell->app_launch.pending_request;
            shell->app_launch.pending = 0;
        }

        if (shell->app_launcher.ops.launch != nullptr)
        {
            (void)shell->app_launcher.ops.launch(shell->app_launcher.launcher, &request);
        }
    }
}

static reach_result reach_shell_start_app_launch_worker(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (shell->app_launch.thread_started)
    {
        return REACH_OK;
    }

    shell->app_launch.stop = 0;
    try
    {
        shell->app_launch.thread = std::thread(reach_shell_app_launch_thread_main, shell);
    }
    catch (...)
    {
        return REACH_ERROR;
    }

    shell->app_launch.thread_started = 1;
    return REACH_OK;
}

reach_result reach_shell_schedule_app_launch(reach_shell *shell,
                                             const reach_app_launch_request *request)
{
    if (shell == nullptr || request == nullptr || request->path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (shell->app_launcher.ops.launch == nullptr)
    {
        return REACH_ERROR;
    }

    reach_result result = reach_shell_start_app_launch_worker(shell);
    if (result != REACH_OK)
    {
        return result;
    }

    {
        std::lock_guard<std::mutex> lock(shell->app_launch.mutex);
        shell->app_launch.pending_request = *request;
        shell->app_launch.pending = 1;
    }

    shell->app_launch.cv.notify_one();
    reach_shell_request_update(shell);
    return REACH_OK;
}

void reach_shell_stop_app_launch_worker(reach_shell *shell)
{
    if (shell == nullptr || !shell->app_launch.thread_started)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(shell->app_launch.mutex);
        shell->app_launch.stop = 1;
        shell->app_launch.pending = 0;
        shell->app_launch.launch_after_launcher_close = 0;
    }
    shell->app_launch.cv.notify_one();

    if (shell->app_launch.thread.joinable())
    {
        shell->app_launch.thread.join();
    }

    shell->app_launch.thread_started = 0;
    shell->app_launch.stop = 0;
    shell->app_launch.pending = 0;
    shell->app_launch.launch_after_launcher_close = 0;
}

reach_result
reach_shell_defer_app_launch_until_launcher_closed(reach_shell *shell,
                                                   const reach_app_launch_request *request)
{
    if (shell == nullptr || request == nullptr || request->path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    shell->app_launch.deferred_launcher_request = *request;
    shell->app_launch.launch_after_launcher_close = 1;
    reach_shell_close_launcher_without_focus_restore(shell);
    reach_shell_request_update(shell);
    return REACH_OK;
}

void reach_shell_process_deferred_launcher_app_launch(reach_shell *shell)
{
    if (shell == nullptr || !shell->app_launch.launch_after_launcher_close)
    {
        return;
    }
    if (shell->ui.launcher.open ||
        reach_shell_surface_transition_visible(&shell->launcher_transition) ||
        reach_shell_surface_transition_active(shell, &shell->launcher_transition))
    {
        return;
    }

    reach_app_launch_request request = shell->app_launch.deferred_launcher_request;
    shell->app_launch.launch_after_launcher_close = 0;
    shell->app_launch.deferred_launcher_request = {};
    (void)reach_shell_schedule_app_launch(shell, &request);
}

static int32_t reach_shell_app_launch_text_equal(const uint16_t *a, const uint16_t *b)
{
    if (a == nullptr || b == nullptr)
    {
        return 0;
    }

    size_t index = 0;
    while (a[index] != 0 || b[index] != 0)
    {
        if (a[index] != b[index])
        {
            return 0;
        }
        ++index;
    }

    return 1;
}

static int32_t reach_shell_app_launch_nonempty_text_equal(const uint16_t *a,
                                                          const uint16_t *b)
{
    return a != nullptr && b != nullptr && a[0] != 0 && b[0] != 0 &&
           reach_shell_app_launch_text_equal(a, b);
}

static int32_t reach_shell_app_launch_window_matches_app(
    const reach_window_snapshot *window, const uint16_t *path,
    const uint16_t *app_user_model_id)
{
    if (window == nullptr)
    {
        return 0;
    }

    if (reach_shell_app_launch_nonempty_text_equal(app_user_model_id,
                                                   window->app_user_model_id))
    {
        return 1;
    }

    return reach_shell_app_launch_nonempty_text_equal(path, window->path);
}

static uintptr_t reach_shell_find_open_app_window(reach_shell *shell, const uint16_t *path,
                                                  const uint16_t *app_user_model_id)
{
    if (shell == nullptr)
    {
        return 0;
    }

    if (shell->window_manager.ops.refresh != nullptr)
    {
        (void)shell->window_manager.ops.refresh(shell->window_manager.manager);
        (void)reach_shell_refresh_open_windows(shell, nullptr);
    }

    for (size_t index = 0; index < shell->open_window_count; ++index)
    {
        const reach_window_snapshot *window = &shell->open_windows[index];
        if (window->id != 0 &&
            reach_shell_app_launch_window_matches_app(window, path, app_user_model_id))
        {
            return window->id;
        }
    }

    return 0;
}

reach_result reach_shell_focus_window(reach_shell *shell, uintptr_t window_id,
                                      int32_t minimize_if_foreground)
{
    if (shell == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (window_id == 0)
    {
        return REACH_OK;
    }

    if (shell->window_manager.ops.refresh != nullptr)
    {
        (void)shell->window_manager.ops.refresh(shell->window_manager.manager);
        (void)reach_shell_refresh_open_windows(shell, nullptr);
    }

    uintptr_t foreground =
        shell->window_manager.ops.foreground != nullptr
            ? shell->window_manager.ops.foreground(shell->window_manager.manager)
            : 0;

    reach_result result = REACH_OK;
    if (minimize_if_foreground && foreground == window_id &&
        !reach_shell_window_is_minimized(shell, window_id))
    {
        result = reach_shell_schedule_window_control(shell, REACH_SHELL_WINDOW_CONTROL_MINIMIZE,
                                                     window_id);
    }
    else
    {
        result = reach_shell_schedule_window_control(shell, REACH_SHELL_WINDOW_CONTROL_ACTIVATE,
                                                     window_id);
    }

    shell->dock.dirty_flags = 1;
    shell->switcher.dirty_flags = 1;
    return result;
}

reach_result reach_shell_launch_app(reach_shell *shell, const uint16_t *path,
                                    const uint16_t *arguments,
                                    int32_t force_new_instance,
                                    int32_t run_as_admin,
                                    int32_t defer_until_launcher_closed)
{
    if (shell == nullptr || path == nullptr || path[0] == 0)
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
               ? reach_shell_defer_app_launch_until_launcher_closed(shell, &request)
               : reach_shell_schedule_app_launch(shell, &request);
}

reach_result reach_shell_open_app(reach_shell *shell, const uint16_t *path,
                                  const uint16_t *arguments,
                                  const uint16_t *app_user_model_id,
                                  int32_t force_new_instance,
                                  int32_t defer_until_launcher_closed)
{
    if (shell == nullptr || path == nullptr || path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (!force_new_instance)
    {
        uintptr_t window = reach_shell_find_open_app_window(shell, path, app_user_model_id);
        if (window != 0)
        {
            return reach_shell_focus_window(shell, window, 0);
        }
    }

    return reach_shell_launch_app(shell, path, arguments, force_new_instance, 0,
                                  defer_until_launcher_closed);
}

reach_result reach_shell_open_pinned_app(reach_shell *shell, size_t pinned_index,
                                         int32_t force_new_instance,
                                         int32_t defer_until_launcher_closed)
{
    if (shell == nullptr || pinned_index >= shell->ui.pinned_app_count)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_pinned_app_model *app = &shell->ui.pinned_apps[pinned_index];
    return reach_shell_open_app(shell, app->path, app->arguments, app->app_user_model_id,
                                force_new_instance, defer_until_launcher_closed);
}

reach_result reach_shell_open_pinned_app_id(reach_shell *shell, uint32_t pin_id,
                                            int32_t force_new_instance,
                                            int32_t defer_until_launcher_closed)
{
    if (shell == nullptr || pin_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < shell->ui.pinned_app_count; ++index)
    {
        if (shell->ui.pinned_apps[index].id == pin_id)
        {
            return reach_shell_open_pinned_app(shell, index, force_new_instance,
                                               defer_until_launcher_closed);
        }
    }

    return REACH_OK;
}

reach_result reach_shell_schedule_open_terminal(reach_shell *shell)
{
    static const uint16_t terminal_path[] = {
        'w', 't', '.', 'e', 'x', 'e', 0
    };

    return reach_shell_open_app(shell, terminal_path, nullptr, nullptr, 1, 0);
}
