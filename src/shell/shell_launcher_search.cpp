#include "shell_internal.h"

static void reach_shell_load_launcher_result_icons(reach_shell *shell)
{
    if (shell == nullptr || shell->icon_provider.ops.load == nullptr) {
        return;
    }

    for (size_t index = 0; index < shell->ui.launcher.result_count && index < REACH_SEARCH_MAX_RESULTS; ++index) {
        reach_icon_request request = {};
        request.size_px = 32;
        reach_copy_utf16(request.path, 260, shell->ui.launcher.results[index].path);

        reach_icon_handle icon = {};
        if (request.path[0] != 0 &&
            shell->icon_provider.ops.load(shell->icon_provider.provider, &request, &icon) == REACH_OK &&
            icon.id != 0) {
            shell->launcher_result_icons[index] = icon;
            (void)reach_ui_state_set_launcher_result_icon(
                &shell->ui,
                index,
                icon.id);
        }
    }
}

void reach_shell_release_launcher_result_icons(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }

    for (size_t index = 0; index < REACH_SEARCH_MAX_RESULTS; ++index) {
        if (shell->launcher_result_icons[index].id != 0 && shell->icon_provider.ops.release != nullptr) {
            (void)shell->icon_provider.ops.release(shell->icon_provider.provider, shell->launcher_result_icons[index]);
        }
        shell->launcher_result_icons[index] = {};
        (void)reach_ui_state_set_launcher_result_icon(&shell->ui, index, 0);
    }
}

static void reach_shell_query_launcher_search(
    reach_shell *shell,
    const uint16_t *query,
    reach_search_candidate *out_results,
    size_t *out_count)
{
    *out_count = 0;
    if (shell == nullptr ||
        query == nullptr ||
        query[0] == 0 ||
        out_results == nullptr ||
        shell->search_provider.ops.query == nullptr ||
        shell->search_provider.ops.result_count == nullptr ||
        shell->search_provider.ops.result_at == nullptr) {
        return;
    }

    if (shell->search_provider.ops.query(shell->search_provider.provider, query) != REACH_OK) {
        return;
    }

    size_t count = shell->search_provider.ops.result_count(shell->search_provider.provider);
    if (count > REACH_SEARCH_MAX_RESULTS) {
        count = REACH_SEARCH_MAX_RESULTS;
    }

    size_t write_index = 0;
    for (size_t index = 0; index < count; ++index) {
        reach_search_result result = {};
        if (shell->search_provider.ops.result_at(shell->search_provider.provider, index, &result) == REACH_OK) {
            reach_copy_utf16(out_results[write_index].name, REACH_SEARCH_RESULT_NAME_CAPACITY, result.title);
            reach_copy_utf16(out_results[write_index].path, REACH_SEARCH_RESULT_PATH_CAPACITY, result.path);
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
    for (;;) {
        uint32_t generation = 0;
        uint16_t query[REACH_MAX_SEARCH_CHARS + 1] = {};

        {
            std::unique_lock<std::mutex> lock(shell->launcher_search_mutex);
            shell->launcher_search_cv.wait(lock, [shell]() {
                return shell->launcher_search_stop || shell->launcher_search_pending;
            });

            if (shell->launcher_search_stop) {
                return;
            }

            generation = shell->launcher_search_pending_generation;
            reach_copy_utf16(query, REACH_MAX_SEARCH_CHARS + 1, shell->launcher_search_pending_query);
            shell->launcher_search_pending = 0;
            shell->launcher_search_in_flight = 1;
        }

        reach_search_candidate results[REACH_SEARCH_MAX_RESULTS] = {};
        size_t count = 0;
        reach_shell_query_launcher_search(shell, query, results, &count);

        void (*notify)(reach_shell *) = nullptr;
        {
            std::lock_guard<std::mutex> lock(shell->launcher_search_mutex);
            shell->launcher_search_in_flight = 0;
            if (!shell->launcher_search_stop) {
                shell->launcher_search_completed_generation = generation;
                shell->launcher_search_completed_count = count;
                for (size_t index = 0; index < REACH_SEARCH_MAX_RESULTS; ++index) {
                    shell->launcher_search_completed_results[index] = results[index];
                }
                shell->launcher_search_completed = 1;
                notify = shell->launcher_search_notify;
            }
        }

        if (notify != nullptr) {
            notify(shell);
        }
    }
}

static reach_result reach_shell_start_launcher_search_worker(reach_shell *shell)
{
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }
    if (shell->launcher_search_thread_started) {
        return REACH_OK;
    }

    shell->launcher_search_stop = 0;
    try {
        shell->launcher_search_thread = std::thread(reach_shell_launcher_search_thread_main, shell);
    } catch (...) {
        return REACH_ERROR;
    }

    shell->launcher_search_thread_started = 1;
    return REACH_OK;
}

void reach_shell_stop_launcher_search_worker(reach_shell *shell)
{
    if (shell == nullptr || !shell->launcher_search_thread_started) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(shell->launcher_search_mutex);
        shell->launcher_search_stop = 1;
        shell->launcher_search_pending = 0;
    }
    shell->launcher_search_cv.notify_one();

    if (shell->launcher_search_thread.joinable()) {
        shell->launcher_search_thread.join();
    }

    shell->launcher_search_thread_started = 0;
    shell->launcher_search_stop = 0;
    shell->launcher_search_in_flight = 0;
    shell->launcher_search_pending = 0;
    shell->launcher_search_completed = 0;
}

void reach_shell_cancel_launcher_search(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }

    ++shell->launcher_search_generation;
    {
        std::lock_guard<std::mutex> lock(shell->launcher_search_mutex);
        shell->launcher_search_pending = 0;
        shell->launcher_search_pending_query[0] = 0;
        shell->launcher_search_completed = 0;
    }
}

reach_result reach_shell_schedule_launcher_search(reach_shell *shell)
{
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    uint32_t generation = ++shell->launcher_search_generation;
    if (shell->ui.launcher.query_length == 0) {
        {
            std::lock_guard<std::mutex> lock(shell->launcher_search_mutex);
            shell->launcher_search_pending = 0;
            shell->launcher_search_pending_query[0] = 0;
            shell->launcher_search_completed = 0;
        }
        reach_shell_release_launcher_result_icons(shell);
        (void)reach_ui_state_clear_launcher_results(&shell->ui);
        shell->layout_dirty = 1;
        shell->launcher.dirty_flags = 1;
        return REACH_OK;
    }

    reach_result result = reach_shell_start_launcher_search_worker(shell);
    if (result != REACH_OK) {
        return result;
    }

    {
        std::lock_guard<std::mutex> lock(shell->launcher_search_mutex);
        shell->launcher_search_pending_generation = generation;
        reach_copy_utf16(shell->launcher_search_pending_query, REACH_MAX_SEARCH_CHARS + 1, shell->ui.launcher.query);
        shell->launcher_search_pending = 1;
    }
    shell->launcher_search_cv.notify_one();
    return REACH_OK;
}

void reach_shell_apply_launcher_search_results(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }

    uint32_t generation = 0;
    reach_search_candidate results[REACH_SEARCH_MAX_RESULTS] = {};
    size_t count = 0;
    {
        std::lock_guard<std::mutex> lock(shell->launcher_search_mutex);
        if (!shell->launcher_search_completed) {
            return;
        }
        generation = shell->launcher_search_completed_generation;
        count = shell->launcher_search_completed_count;
        for (size_t index = 0; index < REACH_SEARCH_MAX_RESULTS; ++index) {
            results[index] = shell->launcher_search_completed_results[index];
        }
        shell->launcher_search_completed = 0;
    }

    if (generation != shell->launcher_search_generation || !shell->ui.launcher.open) {
        return;
    }

    reach_shell_release_launcher_result_icons(shell);
    (void)reach_ui_state_set_launcher_results(&shell->ui, results, count);
    reach_shell_load_launcher_result_icons(shell);
    shell->layout_dirty = 1;
    shell->launcher.dirty_flags = 1;
}
