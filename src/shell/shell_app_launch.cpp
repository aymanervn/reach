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
