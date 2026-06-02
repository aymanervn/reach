#include "shell_internal.h"

static void reach_shell_clear_launcher_result_icon_jobs(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(shell->launcher_search.mutex);
    shell->launcher_search.icon_job_count = 0;
}

static void reach_shell_launcher_result_icon_thread_main(reach_shell *shell)
{
    for (;;)
    {
        reach_shell_launcher_result_icon_job job = {};
        {
            std::unique_lock<std::mutex> lock(shell->launcher_search.mutex);
            shell->launcher_search.icon_cv.wait(
                lock,
                [shell]()
                {
                    return shell->launcher_search.stop || shell->launcher_search.icon_job_count > 0;
                });

            if (shell->launcher_search.stop)
            {
                return;
            }

            job = shell->launcher_search.icon_jobs[0];
            for (size_t index = 1; index < shell->launcher_search.icon_job_count; ++index)
            {
                shell->launcher_search.icon_jobs[index - 1] =
                    shell->launcher_search.icon_jobs[index];
            }
            shell->launcher_search.icon_job_count -= 1;
            shell->launcher_search.icon_in_flight = 1;
        }

        reach_icon_handle icon = {};
        (void)reach_shell_load_icon_handle(shell, job.path, job.size_px, &icon);

        void (*notify)(reach_shell *) = nullptr;
        {
            std::lock_guard<std::mutex> lock(shell->launcher_search.mutex);
            shell->launcher_search.icon_in_flight = 0;
            if (!shell->launcher_search.stop &&
                shell->launcher_search.icon_result_count < REACH_SEARCH_MAX_RESULTS)
            {
                auto *result = &shell->launcher_search
                                    .icon_results[shell->launcher_search.icon_result_count++];
                result->generation = job.generation;
                result->index = job.index;
                reach_copy_utf16(result->path, 260, job.path);
                result->icon = icon;
                icon = {};
                notify = shell->launcher_search.notify;
            }
        }

        if (notify != nullptr)
        {
            notify(shell);
        }

        if (icon.id != 0)
        {
            reach_shell_release_icon_handle(shell, &icon);
        }
    }
}

static reach_result reach_shell_start_launcher_result_icon_worker(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (shell->launcher_search.icon_thread_started)
    {
        return REACH_OK;
    }

    shell->launcher_search.stop = 0;
    try
    {
        shell->launcher_search.icon_thread =
            std::thread(reach_shell_launcher_result_icon_thread_main, shell);
    }
    catch (...)
    {
        return REACH_ERROR;
    }

    shell->launcher_search.icon_thread_started = 1;
    return REACH_OK;
}

void reach_shell_stop_launcher_result_icon_worker(reach_shell *shell)
{
    if (shell == nullptr || !shell->launcher_search.icon_thread_started)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(shell->launcher_search.mutex);
        shell->launcher_search.stop = 1;
        shell->launcher_search.icon_job_count = 0;
    }
    shell->launcher_search.icon_cv.notify_one();

    if (shell->launcher_search.icon_thread.joinable())
    {
        shell->launcher_search.icon_thread.join();
    }

    for (size_t index = 0; index < shell->launcher_search.icon_result_count; ++index)
    {
        reach_shell_release_icon_handle(shell, &shell->launcher_search.icon_results[index].icon);
    }

    shell->launcher_search.icon_thread_started = 0;
    shell->launcher_search.stop = 0;
    shell->launcher_search.icon_in_flight = 0;
    shell->launcher_search.icon_job_count = 0;
    shell->launcher_search.icon_result_count = 0;
}

static void reach_shell_schedule_launcher_result_icons(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    if (reach_shell_start_launcher_result_icon_worker(shell) != REACH_OK)
    {
        return;
    }

    uint32_t generation = ++shell->launcher_search.icon_generation;
    reach_shell_clear_launcher_result_icon_jobs(shell);

    for (size_t index = 0;
         index < shell->ui.launcher.result_count && index < REACH_SEARCH_MAX_RESULTS; ++index)
    {
        const uint16_t *path = shell->ui.launcher.results[index].path;
        if (path[0] == 0)
        {
            continue;
        }

        std::lock_guard<std::mutex> lock(shell->launcher_search.mutex);
        if (shell->launcher_search.icon_job_count >= REACH_SEARCH_MAX_RESULTS)
        {
            break;
        }

        auto *job = &shell->launcher_search.icon_jobs[shell->launcher_search.icon_job_count++];
        job->generation = generation;
        job->index = index;
        job->size_px = 32;
        reach_copy_utf16(job->path, 260, path);
    }

    shell->launcher_search.icon_cv.notify_one();
}

static int32_t reach_shell_launcher_icon_path_matches(const reach_search_candidate *candidate,
                                                      const uint16_t *path)
{
    if (candidate == nullptr || path == nullptr)
    {
        return 0;
    }

    size_t index = 0;
    while (candidate->path[index] != 0 || path[index] != 0)
    {
        if (candidate->path[index] != path[index])
        {
            return 0;
        }
        ++index;
    }

    return 1;
}

static void reach_shell_replace_launcher_search_results(reach_shell *shell,
                                                        const reach_search_candidate *results,
                                                        size_t count)
{
    if (shell == nullptr)
    {
        return;
    }

    if (count > REACH_SEARCH_MAX_RESULTS)
    {
        count = REACH_SEARCH_MAX_RESULTS;
    }

    reach_icon_handle old_icons[REACH_SEARCH_MAX_RESULTS] = {};
    reach_search_candidate old_results[REACH_SEARCH_MAX_RESULTS] = {};
    size_t old_count = shell->ui.launcher.result_count;
    if (old_count > REACH_SEARCH_MAX_RESULTS)
    {
        old_count = REACH_SEARCH_MAX_RESULTS;
    }

    for (size_t index = 0; index < old_count; ++index)
    {
        old_icons[index] = shell->launcher_search.result_icons[index];
        old_results[index] = shell->ui.launcher.results[index];
        shell->launcher_search.result_icons[index] = {};
    }

    (void)reach_ui_state_set_launcher_results(&shell->ui, results, count);

    int32_t old_used[REACH_SEARCH_MAX_RESULTS] = {};
    for (size_t new_index = 0; new_index < count; ++new_index)
    {
        for (size_t old_index = 0; old_index < old_count; ++old_index)
        {
            if (old_used[old_index] || old_icons[old_index].id == 0 ||
                !reach_shell_launcher_icon_path_matches(&old_results[old_index],
                                                        results[new_index].path))
            {
                continue;
            }

            shell->launcher_search.result_icons[new_index] = old_icons[old_index];
            (void)reach_ui_state_set_launcher_result_icon(&shell->ui, new_index,
                                                          old_icons[old_index].id);
            old_icons[old_index] = {};
            old_used[old_index] = 1;
            break;
        }
    }

    for (size_t index = 0; index < old_count; ++index)
    {
        reach_shell_release_icon_handle(shell, &old_icons[index]);
    }
}

void reach_shell_apply_launcher_result_icon_results(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    reach_shell_launcher_result_icon_result results[REACH_SEARCH_MAX_RESULTS] = {};
    size_t result_count = 0;
    {
        std::lock_guard<std::mutex> lock(shell->launcher_search.mutex);
        result_count = shell->launcher_search.icon_result_count;
        for (size_t index = 0; index < result_count; ++index)
        {
            results[index] = shell->launcher_search.icon_results[index];
            shell->launcher_search.icon_results[index].icon = {};
        }
        shell->launcher_search.icon_result_count = 0;
    }

    for (size_t result_index = 0; result_index < result_count; ++result_index)
    {
        auto *result = &results[result_index];
        int32_t used = 0;

        if (result->generation == shell->launcher_search.icon_generation &&
            result->index < shell->ui.launcher.result_count && result->icon.id != 0 &&
            reach_shell_launcher_icon_path_matches(&shell->ui.launcher.results[result->index],
                                                   result->path))
        {
            reach_shell_release_icon_handle(shell,
                                            &shell->launcher_search.result_icons[result->index]);
            shell->launcher_search.result_icons[result->index] = result->icon;
            result->icon = {};
            (void)reach_ui_state_set_launcher_result_icon(
                &shell->ui, result->index, shell->launcher_search.result_icons[result->index].id);
            shell->launcher.dirty_flags = 1;
            used = 1;
        }

        if (!used && result->icon.id != 0)
        {
            reach_shell_release_icon_handle(shell, &result->icon);
        }
    }
}

int32_t reach_shell_launcher_result_icon_work_pending(const reach_shell *shell)
{
    if (shell == nullptr)
    {
        return 0;
    }

    reach_shell *mutable_shell = const_cast<reach_shell *>(shell);
    std::lock_guard<std::mutex> lock(mutable_shell->launcher_search.mutex);
    return mutable_shell->launcher_search.icon_job_count > 0 ||
           mutable_shell->launcher_search.icon_result_count > 0 ||
           mutable_shell->launcher_search.icon_in_flight;
}

void reach_shell_release_launcher_result_icons(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    ++shell->launcher_search.icon_generation;
    reach_shell_clear_launcher_result_icon_jobs(shell);

    for (size_t index = 0; index < REACH_SEARCH_MAX_RESULTS; ++index)
    {
        reach_shell_release_icon_handle(shell, &shell->launcher_search.result_icons[index]);

        (void)reach_ui_state_set_launcher_result_icon(&shell->ui, index, 0);
    }
}

static void reach_shell_query_launcher_search(reach_shell *shell, const uint16_t *query,
                                              reach_search_candidate *out_results,
                                              size_t *out_count)
{
    *out_count = 0;
    if (shell == nullptr || query == nullptr || query[0] == 0 || out_results == nullptr ||
        shell->search_provider.ops.query == nullptr ||
        shell->search_provider.ops.result_count == nullptr ||
        shell->search_provider.ops.result_at == nullptr)
    {
        return;
    }

    if (shell->search_provider.ops.query(shell->search_provider.provider, query) != REACH_OK)
    {
        return;
    }

    size_t count = shell->search_provider.ops.result_count(shell->search_provider.provider);
    if (count > REACH_SEARCH_MAX_RESULTS)
    {
        count = REACH_SEARCH_MAX_RESULTS;
    }

    size_t write_index = 0;
    for (size_t index = 0; index < count; ++index)
    {
        reach_search_result result = {};
        if (shell->search_provider.ops.result_at(shell->search_provider.provider, index, &result) ==
            REACH_OK)
        {
            reach_copy_utf16(out_results[write_index].name, REACH_SEARCH_RESULT_NAME_CAPACITY,
                             result.title);
            reach_copy_utf16(out_results[write_index].path, REACH_SEARCH_RESULT_PATH_CAPACITY,
                             result.path);
            out_results[write_index].kind = result.kind;
            out_results[write_index].is_directory = result.is_directory;
            out_results[write_index].score = result.score;
            ++write_index;
        }
    }
    *out_count = write_index;
}

static void reach_shell_launcher_search_thread_main(reach_shell *shell)
{
    for (;;)
    {
        uint32_t generation = 0;
        uint16_t query[REACH_MAX_SEARCH_CHARS + 1] = {};

        {
            std::unique_lock<std::mutex> lock(shell->launcher_search.mutex);
            shell->launcher_search.cv.wait(
                lock, [shell]()
                { return shell->launcher_search.stop || shell->launcher_search.pending; });

            if (shell->launcher_search.stop)
            {
                return;
            }

            generation = shell->launcher_search.pending_generation;
            reach_copy_utf16(query, REACH_MAX_SEARCH_CHARS + 1,
                             shell->launcher_search.pending_query);
            shell->launcher_search.pending = 0;
            shell->launcher_search.in_flight = 1;
        }

        reach_search_candidate results[REACH_SEARCH_MAX_RESULTS] = {};
        size_t count = 0;
        reach_shell_query_launcher_search(shell, query, results, &count);

        void (*notify)(reach_shell *) = nullptr;
        {
            std::lock_guard<std::mutex> lock(shell->launcher_search.mutex);
            shell->launcher_search.in_flight = 0;
            if (!shell->launcher_search.stop)
            {
                shell->launcher_search.completed_generation = generation;
                shell->launcher_search.completed_count = count;
                for (size_t index = 0; index < REACH_SEARCH_MAX_RESULTS; ++index)
                {
                    shell->launcher_search.completed_results[index] = results[index];
                }
                shell->launcher_search.completed = 1;
                notify = shell->launcher_search.notify;
            }
        }

        if (notify != nullptr)
        {
            notify(shell);
        }
    }
}

static reach_result reach_shell_start_launcher_search_worker(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (shell->launcher_search.thread_started)
    {
        return REACH_OK;
    }

    shell->launcher_search.stop = 0;
    try
    {
        shell->launcher_search.thread = std::thread(reach_shell_launcher_search_thread_main, shell);
    }
    catch (...)
    {
        return REACH_ERROR;
    }

    shell->launcher_search.thread_started = 1;
    return REACH_OK;
}

void reach_shell_stop_launcher_search_worker(reach_shell *shell)
{
    if (shell == nullptr || !shell->launcher_search.thread_started)
    {
        return;
    }

    reach_shell_stop_launcher_result_icon_worker(shell);

    {
        std::lock_guard<std::mutex> lock(shell->launcher_search.mutex);
        shell->launcher_search.stop = 1;
        shell->launcher_search.pending = 0;
    }
    shell->launcher_search.cv.notify_one();

    if (shell->launcher_search.thread.joinable())
    {
        shell->launcher_search.thread.join();
    }

    shell->launcher_search.thread_started = 0;
    shell->launcher_search.stop = 0;
    shell->launcher_search.in_flight = 0;
    shell->launcher_search.pending = 0;
    shell->launcher_search.completed = 0;
}

void reach_shell_cancel_launcher_search(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    ++shell->launcher_search.generation;
    {
        std::lock_guard<std::mutex> lock(shell->launcher_search.mutex);
        shell->launcher_search.pending = 0;
        shell->launcher_search.pending_query[0] = 0;
        shell->launcher_search.completed = 0;
        shell->launcher_search.icon_job_count = 0;
    }
}

reach_result reach_shell_schedule_launcher_search(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    uint32_t generation = ++shell->launcher_search.generation;
    if (shell->ui.launcher.query_length == 0)
    {
        {
            std::lock_guard<std::mutex> lock(shell->launcher_search.mutex);
            shell->launcher_search.pending = 0;
            shell->launcher_search.pending_query[0] = 0;
            shell->launcher_search.completed = 0;
        }
        reach_shell_release_launcher_result_icons(shell);
        (void)reach_ui_state_clear_launcher_results(&shell->ui);
        shell->dirty.layout = 1;
        shell->launcher.dirty_flags = 1;
        return REACH_OK;
    }

    reach_result result = reach_shell_start_launcher_search_worker(shell);
    if (result != REACH_OK)
    {
        return result;
    }

    {
        std::lock_guard<std::mutex> lock(shell->launcher_search.mutex);
        shell->launcher_search.pending_generation = generation;
        reach_copy_utf16(shell->launcher_search.pending_query, REACH_MAX_SEARCH_CHARS + 1,
                         shell->ui.launcher.query);
        shell->launcher_search.pending = 1;
    }
    shell->launcher_search.cv.notify_one();
    return REACH_OK;
}

void reach_shell_apply_launcher_search_results(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    uint32_t generation = 0;
    reach_search_candidate results[REACH_SEARCH_MAX_RESULTS] = {};
    size_t count = 0;
    {
        std::lock_guard<std::mutex> lock(shell->launcher_search.mutex);
        if (!shell->launcher_search.completed)
        {
            return;
        }
        generation = shell->launcher_search.completed_generation;
        count = shell->launcher_search.completed_count;
        for (size_t index = 0; index < REACH_SEARCH_MAX_RESULTS; ++index)
        {
            results[index] = shell->launcher_search.completed_results[index];
        }
        shell->launcher_search.completed = 0;
    }

    if (generation != shell->launcher_search.generation || !shell->ui.launcher.open)
    {
        return;
    }

    ++shell->launcher_search.icon_generation;
    reach_shell_clear_launcher_result_icon_jobs(shell);
    reach_shell_replace_launcher_search_results(shell, results, count);
    reach_shell_schedule_launcher_result_icons(shell);
    shell->dirty.layout = 1;
    shell->launcher.dirty_flags = 1;
}
