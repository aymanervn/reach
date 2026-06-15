#include "shell_internal.h"

static void reach_shell_release_render_icon_from_surface(reach_surface_runtime *surface,
                                                         uint64_t icon_id)
{
    if (surface == nullptr || icon_id == 0 || surface->renderer.ops.release_icon == nullptr)
    {
        return;
    }

    surface->renderer.ops.release_icon(surface->renderer.backend, icon_id);
}

void reach_shell_release_render_icon(reach_shell *shell, uint64_t icon_id)
{
    if (shell == nullptr || icon_id == 0)
    {
        return;
    }

    /*
        Conservative by design: a render icon id can be cached by whichever
        surface rendered it. This makes eviction a shell policy instead of
        spreading it across dock, launcher, tray, and lifecycle code.
    */
    reach_shell_release_render_icon_from_surface(&shell->launcher, icon_id);
    reach_shell_release_render_icon_from_surface(&shell->dock, icon_id);
    reach_shell_release_render_icon_from_surface(&shell->tray, icon_id);
    reach_shell_release_render_icon_from_surface(&shell->switcher, icon_id);
    reach_shell_release_render_icon_from_surface(&shell->context_menu, icon_id);
    reach_shell_release_render_icon_from_surface(&shell->quick_settings, icon_id);
}

void reach_shell_release_icon_handle(reach_shell *shell, reach_icon_handle *icon)
{
    if (icon == nullptr)
    {
        return;
    }

    if (shell == nullptr || icon->id == 0)
    {
        *icon = {};
        return;
    }

    reach_shell_release_render_icon(shell, icon->id);

    if (shell->icon_provider.ops.release != nullptr)
    {
        (void)shell->icon_provider.ops.release(shell->icon_provider.provider, *icon);
    }

    *icon = {};
}

reach_result reach_shell_load_icon_handle(reach_shell *shell, const uint16_t *path, int32_t size_px,
                                          reach_icon_handle *out_icon)
{
    if (out_icon != nullptr)
    {
        *out_icon = {};
    }

    if (shell == nullptr || path == nullptr || path[0] == 0 || out_icon == nullptr ||
        shell->icon_provider.ops.load == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_icon_request request = {};
    request.size_px = size_px;
    reach_copy_utf16(request.path, 260, path);

    return shell->icon_provider.ops.load(shell->icon_provider.provider, &request, out_icon);
}

static void reach_shell_query_open_window_icon(reach_shell *shell,
                                               const reach_shell_open_window_icon_job *job,
                                               reach_icon_handle *out_icon)
{
    if (out_icon != nullptr)
    {
        *out_icon = {};
    }
    if (shell == nullptr || job == nullptr || out_icon == nullptr)
    {
        return;
    }

    (void)reach_shell_load_icon_handle(shell, job->path, job->size_px, out_icon);
}

static void reach_shell_open_window_icon_thread_main(reach_shell *shell)
{
    for (;;)
    {
        reach_shell_open_window_icon_job job = {};
        reach_shell_pinned_icon_job pinned_job = {};
        int32_t load_pinned_icon = 0;
        {
            std::unique_lock<std::mutex> lock(shell->open_window_icons.mutex);
            shell->open_window_icons.cv.wait(lock,
                                             [shell]()
                                             {
                                                 return shell->open_window_icons.stop ||
                                                        shell->open_window_icons.job_count > 0 ||
                                                        shell->open_window_icons.pinned_job_count >
                                                            0;
                                             });

            if (shell->open_window_icons.stop)
            {
                return;
            }

            if (shell->open_window_icons.job_count > 0)
            {
                job = shell->open_window_icons.jobs[0];
                for (size_t index = 1; index < shell->open_window_icons.job_count; ++index)
                {
                    shell->open_window_icons.jobs[index - 1] = shell->open_window_icons.jobs[index];
                }
                shell->open_window_icons.job_count -= 1;
                shell->open_window_icons.in_flight = 1;
            }
            else
            {
                pinned_job = shell->open_window_icons.pinned_jobs[0];
                for (size_t index = 1; index < shell->open_window_icons.pinned_job_count; ++index)
                {
                    shell->open_window_icons.pinned_jobs[index - 1] =
                        shell->open_window_icons.pinned_jobs[index];
                }
                shell->open_window_icons.pinned_job_count -= 1;
                shell->open_window_icons.pinned_in_flight = 1;
                load_pinned_icon = 1;
            }
        }

        reach_icon_handle icon = {};
        if (load_pinned_icon)
        {
            (void)reach_shell_load_icon_handle(shell, pinned_job.path, pinned_job.size_px, &icon);
        }
        else
        {
            reach_shell_query_open_window_icon(shell, &job, &icon);
        }

        {
            std::lock_guard<std::mutex> lock(shell->open_window_icons.mutex);
            if (load_pinned_icon)
            {
                shell->open_window_icons.pinned_in_flight = 0;
                if (!shell->open_window_icons.stop &&
                    shell->open_window_icons.pinned_result_count < REACH_MAX_PINNED_APPS)
                {
                    reach_shell_pinned_icon_result *result =
                        &shell->open_window_icons
                             .pinned_results[shell->open_window_icons.pinned_result_count++];
                    result->generation = pinned_job.generation;
                    result->index = pinned_job.index;
                    reach_copy_utf16(result->path, 260, pinned_job.path);
                    result->initial = pinned_job.initial;
                    result->icon = icon;
                    icon = {};
                }
            }
            else
            {
                shell->open_window_icons.in_flight = 0;
                if (!shell->open_window_icons.stop &&
                    shell->open_window_icons.result_count < REACH_MAX_PINNED_APPS)
                {
                    reach_shell_open_window_icon_result *result =
                        &shell->open_window_icons.results[shell->open_window_icons.result_count++];
                    result->generation = job.generation;
                    result->window = job.window;
                    reach_copy_utf16(result->path, 260, job.path);
                    result->initial = job.initial;
                    result->icon = icon;
                    icon = {};
                }
            }
        }

        if (icon.id != 0)
        {
            reach_shell_release_icon_handle(shell, &icon);
        }
    }
}

static reach_result reach_shell_start_open_window_icon_worker(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (shell->open_window_icons.thread_started)
    {
        return REACH_OK;
    }

    shell->open_window_icons.stop = 0;
    try
    {
        shell->open_window_icons.thread =
            std::thread(reach_shell_open_window_icon_thread_main, shell);
    }
    catch (...)
    {
        return REACH_ERROR;
    }

    shell->open_window_icons.thread_started = 1;
    return REACH_OK;
}

void reach_shell_stop_open_window_icon_worker(reach_shell *shell)
{
    if (shell == nullptr || !shell->open_window_icons.thread_started)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(shell->open_window_icons.mutex);
        shell->open_window_icons.stop = 1;
        shell->open_window_icons.job_count = 0;
    }
    shell->open_window_icons.cv.notify_one();

    if (shell->open_window_icons.thread.joinable())
    {
        shell->open_window_icons.thread.join();
    }

    for (size_t index = 0; index < shell->open_window_icons.result_count; ++index)
    {
        reach_shell_release_icon_handle(shell, &shell->open_window_icons.results[index].icon);
    }
    for (size_t index = 0; index < shell->open_window_icons.pinned_result_count; ++index)
    {
        reach_shell_release_icon_handle(shell,
                                        &shell->open_window_icons.pinned_results[index].icon);
    }

    shell->open_window_icons.thread_started = 0;
    shell->open_window_icons.stop = 0;
    shell->open_window_icons.in_flight = 0;
    shell->open_window_icons.pinned_in_flight = 0;
    shell->open_window_icons.job_count = 0;
    shell->open_window_icons.result_count = 0;
    shell->open_window_icons.pinned_job_count = 0;
    shell->open_window_icons.pinned_result_count = 0;
}

static void reach_shell_schedule_open_window_icon_load(reach_shell *shell, size_t index)
{
    if (shell == nullptr || index >= shell->open_window_count || index >= REACH_MAX_PINNED_APPS ||
        (shell->open_windows[index].path[0] == 0 && shell->open_windows[index].icon_ref[0] == 0))
    {
        return;
    }

    const uint16_t *icon_path = shell->open_windows[index].icon_ref[0] != 0
                                    ? shell->open_windows[index].icon_ref
                                    : shell->open_windows[index].path;

    if (reach_shell_start_open_window_icon_worker(shell) != REACH_OK)
    {
        (void)reach_shell_load_icon_handle(shell, icon_path,
                                           reach_shell_dock_icon_size_px(shell),
                                           &shell->dock_icons.open_window_icons[index]);
        return;
    }

    reach_shell_open_window_icon_job job = {};
    job.generation = shell->open_window_icons.generation;
    job.window = shell->open_windows[index].id;
    job.initial = shell->dock_icons.open_window_initials[index];
    job.size_px = reach_shell_dock_icon_size_px(shell);
    reach_copy_utf16(job.path, 260, icon_path);

    {
        std::lock_guard<std::mutex> lock(shell->open_window_icons.mutex);
        if (shell->open_window_icons.job_count >= REACH_MAX_PINNED_APPS)
        {
            return;
        }
        shell->open_window_icons.jobs[shell->open_window_icons.job_count++] = job;
    }
    shell->open_window_icons.cv.notify_one();
}

void reach_shell_schedule_pinned_icon_loads(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    shell->open_window_icons.pinned_generation += 1;
    {
        std::lock_guard<std::mutex> lock(shell->open_window_icons.mutex);
        shell->open_window_icons.pinned_job_count = 0;
    }

    if (reach_shell_start_open_window_icon_worker(shell) != REACH_OK)
    {
        return;
    }

    uint32_t generation = shell->open_window_icons.pinned_generation;
    size_t count = shell->dock_icons.pinned_icon_count;
    if (count > REACH_MAX_PINNED_APPS)
    {
        count = REACH_MAX_PINNED_APPS;
    }

    {
        std::lock_guard<std::mutex> lock(shell->open_window_icons.mutex);
        for (size_t index = 0; index < count; ++index)
        {
            const reach_pinned_app_model *app = &shell->ui.pinned_apps[index];
            const uint16_t *icon_path = app->icon_ref[0] != 0 ? app->icon_ref : app->path;
            if (icon_path[0] == 0 ||
                shell->open_window_icons.pinned_job_count >= REACH_MAX_PINNED_APPS)
            {
                continue;
            }

            reach_shell_pinned_icon_job *job =
                &shell->open_window_icons.pinned_jobs[shell->open_window_icons.pinned_job_count++];
            job->generation = generation;
            job->index = index;
            job->initial = shell->dock_icons.pinned_icon_initials[index];
            job->size_px = reach_shell_dock_icon_size_px(shell);
            reach_copy_utf16(job->path, 260, icon_path);
        }
    }
    shell->open_window_icons.cv.notify_one();
}

int32_t reach_shell_open_window_icon_work_pending(const reach_shell *shell)
{
    if (shell == nullptr)
    {
        return 0;
    }

    reach_shell *mutable_shell = const_cast<reach_shell *>(shell);
    std::lock_guard<std::mutex> lock(mutable_shell->open_window_icons.mutex);
    return mutable_shell->open_window_icons.job_count > 0 ||
           mutable_shell->open_window_icons.result_count > 0 ||
           mutable_shell->open_window_icons.in_flight ||
           mutable_shell->open_window_icons.pinned_job_count > 0 ||
           mutable_shell->open_window_icons.pinned_result_count > 0 ||
           mutable_shell->open_window_icons.pinned_in_flight;
}

static int32_t reach_shell_pinned_icon_result_matches(const reach_shell *shell,
                                                      const reach_shell_pinned_icon_result *result)
{
    if (shell == nullptr || result == nullptr || result->index >= shell->ui.pinned_app_count ||
        result->index >= REACH_MAX_PINNED_APPS)
    {
        return 0;
    }

    const reach_pinned_app_model *app = &shell->ui.pinned_apps[result->index];
    const uint16_t *icon_path = app->icon_ref[0] != 0 ? app->icon_ref : app->path;
    size_t index = 0;
    while (icon_path[index] != 0 || result->path[index] != 0)
    {
        if (icon_path[index] != result->path[index])
        {
            return 0;
        }
        ++index;
    }

    return 1;
}

static void reach_shell_apply_pinned_icon_results(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    reach_shell_pinned_icon_result results[REACH_MAX_PINNED_APPS] = {};
    size_t result_count = 0;
    {
        std::lock_guard<std::mutex> lock(shell->open_window_icons.mutex);
        result_count = shell->open_window_icons.pinned_result_count;
        for (size_t index = 0; index < result_count; ++index)
        {
            results[index] = shell->open_window_icons.pinned_results[index];
            shell->open_window_icons.pinned_results[index].icon = {};
        }
        shell->open_window_icons.pinned_result_count = 0;
    }

    for (size_t result_index = 0; result_index < result_count; ++result_index)
    {
        reach_shell_pinned_icon_result *result = &results[result_index];
        int32_t used = 0;
        if (result->generation == shell->open_window_icons.pinned_generation &&
            result->icon.id != 0 && reach_shell_pinned_icon_result_matches(shell, result))
        {
            reach_shell_release_icon_handle(shell, &shell->dock_icons.pinned_icons[result->index]);
            shell->dock_icons.pinned_icons[result->index] = result->icon;
            shell->dock_icons.pinned_icon_pin_ids[result->index] =
                shell->ui.pinned_apps[result->index].id;
            shell->dock_icons.pinned_icon_initials[result->index] = result->initial;
            result->icon = {};
            shell->dock.dirty_flags = 1;
            used = 1;
        }

        if (!used && result->icon.id != 0)
        {
            reach_shell_release_icon_handle(shell, &result->icon);
        }
    }
}

static int32_t
reach_shell_open_window_icon_result_matches(const reach_window_snapshot *window,
                                            const reach_shell_open_window_icon_result *result)
{
    if (window == nullptr || result == nullptr || window->id != result->window)
    {
        return 0;
    }

    const uint16_t *icon_path = window->icon_ref[0] != 0 ? window->icon_ref : window->path;

    size_t index = 0;
    while (icon_path[index] != 0 || result->path[index] != 0)
    {
        if (icon_path[index] != result->path[index])
        {
            return 0;
        }
        ++index;
    }

    return 1;
}

void reach_shell_apply_open_window_icon_results(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    reach_shell_apply_pinned_icon_results(shell);

    reach_shell_open_window_icon_result results[REACH_MAX_PINNED_APPS] = {};
    size_t result_count = 0;
    {
        std::lock_guard<std::mutex> lock(shell->open_window_icons.mutex);
        result_count = shell->open_window_icons.result_count;
        for (size_t index = 0; index < result_count; ++index)
        {
            results[index] = shell->open_window_icons.results[index];
            shell->open_window_icons.results[index].icon = {};
        }
        shell->open_window_icons.result_count = 0;
    }

    for (size_t result_index = 0; result_index < result_count; ++result_index)
    {
        reach_shell_open_window_icon_result *result = &results[result_index];
        int32_t used = 0;
        if (result->generation == shell->open_window_icons.generation && result->icon.id != 0)
        {
            for (size_t window_index = 0;
                 window_index < shell->open_window_count && window_index < REACH_MAX_PINNED_APPS;
                 ++window_index)
            {
                if (shell->dock_icons.open_window_icons[window_index].id != 0 ||
                    !reach_shell_open_window_icon_result_matches(&shell->open_windows[window_index],
                                                                 result))
                {
                    continue;
                }

                shell->dock_icons.open_window_icons[window_index] = result->icon;
                shell->dock_icons.open_window_initials[window_index] = result->initial;
                result->icon = {};
                used = 1;
                shell->dock.dirty_flags = 1;
                shell->switcher.dirty_flags = 1;
                break;
            }
        }

        if (!used && result->icon.id != 0)
        {
            reach_shell_release_icon_handle(shell, &result->icon);
        }
    }
}

void reach_shell_release_dock_icons(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    shell->open_window_icons.pinned_generation += 1;
    {
        std::lock_guard<std::mutex> lock(shell->open_window_icons.mutex);
        shell->open_window_icons.pinned_job_count = 0;
    }

    size_t pinned_count = shell->dock_icons.pinned_icon_count;
    if (pinned_count > REACH_MAX_PINNED_APPS)
    {
        pinned_count = REACH_MAX_PINNED_APPS;
    }

    for (size_t index = 0; index < pinned_count; ++index)
    {
        reach_shell_release_icon_handle(shell, &shell->dock_icons.pinned_icons[index]);
        shell->dock_icons.pinned_icon_pin_ids[index] = 0;
        shell->dock_icons.pinned_icon_initials[index] = 0;
    }

    shell->dock_icons.pinned_icon_count = 0;

    size_t open_count = shell->open_window_count;
    if (open_count > REACH_MAX_PINNED_APPS)
    {
        open_count = REACH_MAX_PINNED_APPS;
    }

    for (size_t index = 0; index < open_count; ++index)
    {
        reach_shell_release_icon_handle(shell, &shell->dock_icons.open_window_icons[index]);
        shell->dock_icons.open_window_initials[index] = 0;
    }
    reach_dock_clear_all_icons(&shell->dock_icons, REACH_MAX_PINNED_APPS);
}

void reach_shell_release_open_window_icons(reach_shell *shell, size_t old_count)
{
    if (shell == nullptr)
    {
        return;
    }

    size_t count = old_count;
    if (count > REACH_MAX_PINNED_APPS)
    {
        count = REACH_MAX_PINNED_APPS;
    }

    for (size_t index = 0; index < count; ++index)
    {
        reach_shell_release_icon_handle(shell, &shell->dock_icons.open_window_icons[index]);
        shell->dock_icons.open_window_initials[index] = 0;
    }
}

static void reach_shell_load_open_window_icon(reach_shell *shell, size_t index)
{
    if (shell == nullptr || index >= shell->open_window_count || index >= REACH_MAX_PINNED_APPS)
    {
        return;
    }

    reach_shell_release_icon_handle(shell, &shell->dock_icons.open_window_icons[index]);

    shell->dock_icons.open_window_initials[index] =
        shell->open_windows[index].title[0] != 0 ? shell->open_windows[index].title[0] : '?';

    reach_shell_schedule_open_window_icon_load(shell, index);
}

static int32_t reach_shell_open_window_icon_match(const reach_window_snapshot *window,
                                                  uintptr_t old_window,
                                                  const uint16_t *old_icon_ref)
{
    if (window == nullptr || old_window == 0 || old_icon_ref == nullptr)
    {
        return 0;
    }

    if (window->id != old_window)
    {
        return 0;
    }

    const uint16_t *icon_ref = window->icon_ref[0] != 0 ? window->icon_ref : window->path;

    size_t index = 0;
    while (icon_ref[index] != 0 || old_icon_ref[index] != 0)
    {
        if (icon_ref[index] != old_icon_ref[index])
        {
            return 0;
        }
        ++index;
    }

    return 1;
}

void reach_shell_sync_open_window_icons(reach_shell *shell, const uintptr_t *old_windows,
                                        const uint16_t old_icon_refs[][260],
                                        const reach_icon_handle *old_icons,
                                        const uint16_t *old_initials, size_t old_count)
{
    if (shell == nullptr)
    {
        return;
    }

    reach_icon_handle next_icons[REACH_MAX_PINNED_APPS] = {};
    uint16_t next_initials[REACH_MAX_PINNED_APPS] = {};
    int32_t old_used[REACH_MAX_PINNED_APPS] = {};

    size_t new_count = shell->open_window_count;
    if (new_count > REACH_MAX_PINNED_APPS)
    {
        new_count = REACH_MAX_PINNED_APPS;
    }
    if (old_count > REACH_MAX_PINNED_APPS)
    {
        old_count = REACH_MAX_PINNED_APPS;
    }
    shell->open_window_icons.generation += 1;
    {
        std::lock_guard<std::mutex> lock(shell->open_window_icons.mutex);
        shell->open_window_icons.job_count = 0;
    }

    for (size_t new_index = 0; new_index < new_count; ++new_index)
    {
        for (size_t old_index = 0; old_index < old_count; ++old_index)
        {
            if (old_used[old_index])
            {
                continue;
            }
            if (!reach_shell_open_window_icon_match(&shell->open_windows[new_index],
                                                    old_windows[old_index],
                                                    old_icon_refs[old_index]))
            {
                continue;
            }

            next_icons[new_index] = old_icons[old_index];
            next_initials[new_index] = old_initials[old_index];
            old_used[old_index] = 1;
            break;
        }
    }

    for (size_t old_index = 0; old_index < old_count; ++old_index)
    {
        if (!old_used[old_index])
        {
            reach_icon_handle icon = old_icons[old_index];
            reach_shell_release_icon_handle(shell, &icon);
        }
    }

    for (size_t index = 0; index < REACH_MAX_PINNED_APPS; ++index)
    {
        shell->dock_icons.open_window_icons[index] = next_icons[index];
        shell->dock_icons.open_window_initials[index] = next_initials[index];
    }

    for (size_t index = 0; index < new_count; ++index)
    {
        if (shell->dock_icons.open_window_icons[index].id == 0)
        {
            reach_shell_load_open_window_icon(shell, index);
        }
    }
}

void reach_shell_load_open_window_icons(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    size_t count = shell->open_window_count;
    if (count > REACH_MAX_PINNED_APPS)
    {
        count = REACH_MAX_PINNED_APPS;
    }

    for (size_t index = 0; index < count; ++index)
    {
        reach_shell_load_open_window_icon(shell, index);
    }
}

void reach_shell_release_tray_render_icons(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    size_t count = shell->tray_state.model.item_count;
    if (count > REACH_MAX_TRAY_ITEMS)
    {
        count = REACH_MAX_TRAY_ITEMS;
    }

    for (size_t index = 0; index < count; ++index)
    {
        uint64_t icon_id = shell->tray_state.model.items[index].icon_id;
        if (icon_id != 0)
        {
            reach_shell_release_render_icon(shell, icon_id);
        }
    }
}

void reach_shell_release_quick_settings_audio_render_icons(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    size_t session_count = shell->quick_settings_audio_sessions.count;
    if (session_count > REACH_AUDIO_VOLUME_MAX_SESSIONS)
    {
        session_count = REACH_AUDIO_VOLUME_MAX_SESSIONS;
    }

    for (size_t index = 0; index < session_count; ++index)
    {
        uint64_t icon_id = shell->quick_settings_audio_sessions.sessions[index].icon_id;
        if (icon_id != 0)
        {
            reach_shell_release_render_icon(shell, icon_id);
        }
    }

    size_t device_count = shell->quick_settings_output_devices.count;
    if (device_count > REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES)
    {
        device_count = REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES;
    }

    for (size_t index = 0; index < device_count; ++index)
    {
        uint64_t icon_id = shell->quick_settings_output_devices.devices[index].icon_id;
        if (icon_id != 0)
        {
            reach_shell_release_render_icon(shell, icon_id);
        }
    }
}
