#include "shell_internal.h"

#include <memory>

static void update_progress(void *user, reach_windows_update_progress progress)
{
    reach_shell *shell = static_cast<reach_shell *>(user);
    if (shell == nullptr)
        return;
    shell->windows_update_worker.progress_state = (int32_t)progress;
    if (shell->windows_update_worker.notify != nullptr)
        shell->windows_update_worker.notify(shell);
}

static void worker_main(reach_shell *shell)
{
    reach_shell_windows_update_worker_state *worker = &shell->windows_update_worker;
    for (;;)
    {
        reach_shell_windows_update_work_type work = REACH_SHELL_WINDOWS_UPDATE_WORK_NONE;
        reach_windows_update_identity selected[REACH_WINDOWS_UPDATE_MAX_UPDATES] = {};
        size_t selected_count = 0;
        {
            std::unique_lock<std::mutex> lock(worker->mutex);
            worker->cv.wait(lock, [worker] { return worker->stop || worker->pending; });
            if (worker->stop)
                break;
            work = worker->pending_work;
            selected_count = worker->selected_count;
            for (size_t index = 0; index < selected_count; ++index)
                selected[index] = worker->selected[index];
            worker->pending = 0;
            worker->in_flight = 1;
        }

        std::unique_ptr<reach_windows_update_list> scan_result(new (std::nothrow)
                                                                   reach_windows_update_list());
        std::unique_ptr<reach_windows_update_operation_result> operation_result(
            new (std::nothrow) reach_windows_update_operation_result());
        int32_t scan_hresult = 0;
        reach_result result = REACH_ERROR;
        if (scan_result == nullptr || operation_result == nullptr)
            result = REACH_ERROR;
        else if (work == REACH_SHELL_WINDOWS_UPDATE_WORK_SCAN &&
                 shell->windows_update.scan != nullptr)
            result = shell->windows_update.scan(shell->windows_update.userdata, scan_result.get(),
                                                &scan_hresult);
        else if (work == REACH_SHELL_WINDOWS_UPDATE_WORK_INSTALL &&
                 shell->windows_update.install != nullptr)
            result = shell->windows_update.install(shell->windows_update.userdata, selected,
                                                   selected_count, update_progress, shell,
                                                   operation_result.get());
        else if (work == REACH_SHELL_WINDOWS_UPDATE_WORK_VERIFY &&
                 shell->windows_update.verify != nullptr)
            result = shell->windows_update.verify(shell->windows_update.userdata, selected,
                                                  selected_count, operation_result.get());

        {
            std::lock_guard<std::mutex> lock(worker->mutex);
            if (scan_result != nullptr)
                worker->scan_result = *scan_result;
            else
                worker->scan_result = {};
            if (operation_result != nullptr)
                worker->operation_result = *operation_result;
            else
                worker->operation_result = {};
            worker->scan_hresult = scan_hresult;
            worker->work_result = result;
            worker->completed_work = work;
            worker->completed = 1;
            worker->in_flight = 0;
        }
        if (worker->notify != nullptr)
            worker->notify(shell);
    }
}

static reach_result ensure_worker(reach_shell *shell)
{
    reach_shell_windows_update_worker_state *worker = &shell->windows_update_worker;
    if (worker->thread_started)
        return REACH_OK;
    try
    {
        worker->stop = 0;
        worker->thread = std::thread(worker_main, shell);
        worker->thread_started = 1;
        return REACH_OK;
    }
    catch (...)
    {
        return REACH_ERROR;
    }
}

void reach_shell_schedule_windows_update_scan(reach_shell *shell)
{
    if (shell == nullptr || shell->windows_update.scan == nullptr ||
        reach_settings_model_update_busy(&shell->settings_model) ||
        ensure_worker(shell) != REACH_OK)
        return;
    reach_settings_model_begin_update_scan(&shell->settings_model);
    {
        std::lock_guard<std::mutex> lock(shell->windows_update_worker.mutex);
        shell->windows_update_worker.pending_work = REACH_SHELL_WINDOWS_UPDATE_WORK_SCAN;
        shell->windows_update_worker.pending = 1;
    }
    shell->settings.dirty_flags = 1;
    shell->windows_update_worker.cv.notify_one();
    reach_shell_request_update(shell);
}

void reach_shell_schedule_windows_update_install(reach_shell *shell)
{
    if (shell == nullptr || shell->windows_update.install == nullptr ||
        reach_settings_model_update_busy(&shell->settings_model) ||
        ensure_worker(shell) != REACH_OK)
        return;
    reach_shell_windows_update_worker_state *worker = &shell->windows_update_worker;
    reach_windows_update_identity selected[REACH_WINDOWS_UPDATE_MAX_UPDATES] = {};
    size_t count = 0;
    for (size_t index = 0; index < shell->settings_model.update_list.count; ++index)
        if (shell->settings_model.update_list.updates[index].selected &&
            (shell->settings_model.update_list.updates[index].state ==
                 REACH_WINDOWS_UPDATE_SELECTED ||
             shell->settings_model.update_list.updates[index].state ==
                 REACH_WINDOWS_UPDATE_FAILED) &&
            count < REACH_WINDOWS_UPDATE_MAX_UPDATES)
            selected[count++] = shell->settings_model.update_list.updates[index].identity;
    if (count == 0)
        return;
    reach_settings_model_begin_update_install(&shell->settings_model);
    {
        std::lock_guard<std::mutex> lock(worker->mutex);
        worker->selected_count = count;
        for (size_t index = 0; index < count; ++index)
            worker->selected[index] = selected[index];
        worker->pending_work = REACH_SHELL_WINDOWS_UPDATE_WORK_INSTALL;
        worker->pending = 1;
    }
    shell->settings.dirty_flags = 1;
    worker->cv.notify_one();
    reach_shell_request_update(shell);
}

void reach_shell_schedule_windows_update_resume_verification(reach_shell *shell)
{
    if (shell == nullptr || shell->windows_update.load_pending_verification == nullptr ||
        shell->windows_update.verify == nullptr)
        return;
    reach_windows_update_identity pending[REACH_WINDOWS_UPDATE_MAX_UPDATES] = {};
    size_t count = 0;
    if (shell->windows_update.load_pending_verification(shell->windows_update.userdata, pending,
                                                        REACH_WINDOWS_UPDATE_MAX_UPDATES,
                                                        &count) != REACH_OK ||
        count == 0 || ensure_worker(shell) != REACH_OK)
        return;
    reach_shell_windows_update_worker_state *worker = &shell->windows_update_worker;
    {
        std::lock_guard<std::mutex> lock(worker->mutex);
        worker->selected_count = count;
        for (size_t index = 0; index < count; ++index)
            worker->selected[index] = pending[index];
        worker->pending_work = REACH_SHELL_WINDOWS_UPDATE_WORK_VERIFY;
        worker->pending = 1;
    }
    shell->settings_model.update_page_state = REACH_SETTINGS_UPDATE_VERIFYING;
    reach_copy_utf16(shell->settings_model.update_status, REACH_WINDOWS_UPDATE_TEXT_CAPACITY,
                     (const uint16_t *)u"Verifying installed updates...");
    worker->cv.notify_one();
    reach_shell_request_update(shell);
}

void reach_shell_apply_windows_update_progress(reach_shell *shell)
{
    if (shell == nullptr)
        return;
    int32_t encoded = shell->windows_update_worker.progress_state.exchange(0);
    if (encoded == 0)
        return;
    reach_windows_update_progress progress = (reach_windows_update_progress)encoded;
    if (progress == REACH_WINDOWS_UPDATE_PROGRESS_DOWNLOADING)
    {
        shell->settings_model.update_page_state = REACH_SETTINGS_UPDATE_DOWNLOADING;
        reach_copy_utf16(shell->settings_model.update_status, REACH_WINDOWS_UPDATE_TEXT_CAPACITY,
                         (const uint16_t *)u"Downloading selected updates...");
    }
    else if (progress == REACH_WINDOWS_UPDATE_PROGRESS_INSTALLING)
    {
        shell->settings_model.update_page_state = REACH_SETTINGS_UPDATE_INSTALLING;
        reach_copy_utf16(shell->settings_model.update_status, REACH_WINDOWS_UPDATE_TEXT_CAPACITY,
                         (const uint16_t *)u"Installing selected updates...");
    }
    else if (progress == REACH_WINDOWS_UPDATE_PROGRESS_VERIFYING)
    {
        shell->settings_model.update_page_state = REACH_SETTINGS_UPDATE_VERIFYING;
        reach_copy_utf16(shell->settings_model.update_status, REACH_WINDOWS_UPDATE_TEXT_CAPACITY,
                         (const uint16_t *)u"Verifying installation state...");
    }
    if (progress != REACH_WINDOWS_UPDATE_PROGRESS_VERIFYING)
        for (size_t index = 0; index < shell->settings_model.update_list.count; ++index)
            if (shell->settings_model.update_list.updates[index].selected)
                shell->settings_model.update_list.updates[index].state =
                    progress == REACH_WINDOWS_UPDATE_PROGRESS_DOWNLOADING
                        ? REACH_WINDOWS_UPDATE_DOWNLOADING
                        : REACH_WINDOWS_UPDATE_INSTALLING;
    shell->settings.dirty_flags = 1;
    shell->dirty.render = 1;
}

void reach_shell_apply_windows_update_result(reach_shell *shell)
{
    if (shell == nullptr)
        return;
    reach_shell_windows_update_worker_state *worker = &shell->windows_update_worker;
    {
        std::lock_guard<std::mutex> lock(worker->mutex);
        if (!worker->completed)
            return;
    }
    reach_shell_windows_update_work_type work = REACH_SHELL_WINDOWS_UPDATE_WORK_NONE;
    std::unique_ptr<reach_windows_update_list> scan(new (std::nothrow) reach_windows_update_list());
    std::unique_ptr<reach_windows_update_operation_result> operation(
        new (std::nothrow) reach_windows_update_operation_result());
    if (scan == nullptr || operation == nullptr)
        return;
    int32_t scan_hresult = 0;
    reach_result result = REACH_ERROR;
    {
        std::lock_guard<std::mutex> lock(worker->mutex);
        if (!worker->completed)
            return;
        work = worker->completed_work;
        *scan = worker->scan_result;
        *operation = worker->operation_result;
        scan_hresult = worker->scan_hresult;
        result = worker->work_result;
        worker->completed = 0;
    }
    if (work != REACH_SHELL_WINDOWS_UPDATE_WORK_SCAN)
    {
        for (size_t result_index = 0; result_index < operation->per_update_result_count;
             ++result_index)
            for (size_t update_index = 0; update_index < shell->settings_model.update_list.count;
                 ++update_index)
            {
                const reach_windows_update_identity *left =
                    &operation->per_update_results[result_index].identity;
                const reach_windows_update_identity *right =
                    &shell->settings_model.update_list.updates[update_index].identity;
                int32_t equal = left->revision_number == right->revision_number;
                for (size_t character = 0; equal && character < REACH_WINDOWS_UPDATE_ID_CAPACITY;
                     ++character)
                {
                    if (left->update_id[character] != right->update_id[character])
                        equal = 0;
                    if (left->update_id[character] == 0)
                        break;
                }
                if (equal)
                    reach_copy_utf16(
                        operation->per_update_results[result_index].selected_reason,
                        REACH_WINDOWS_UPDATE_TEXT_CAPACITY,
                        shell->settings_model.update_list.updates[update_index].selected_reason);
            }
    }
    if (result != REACH_OK && operation->failure_class == REACH_WINDOWS_UPDATE_FAILURE_NONE)
    {
        operation->failure_class = work == REACH_SHELL_WINDOWS_UPDATE_WORK_VERIFY
                                       ? REACH_WINDOWS_UPDATE_VERIFICATION_FAILED
                                       : REACH_WINDOWS_UPDATE_INSTALL_FAILED;
        if (operation->overall_install_hresult == 0)
            operation->overall_install_hresult = -1;
    }
    if (work == REACH_SHELL_WINDOWS_UPDATE_WORK_SCAN)
        reach_settings_model_apply_update_scan(
            &shell->settings_model, result == REACH_OK ? scan.get() : nullptr,
            result == REACH_OK ? scan_hresult : (scan_hresult != 0 ? scan_hresult : -1));
    else
        reach_settings_model_apply_update_operation(&shell->settings_model, operation.get());
    shell->settings.dirty_flags = 1;
    shell->dirty.render = 1;
    reach_shell_request_update(shell);
}

void reach_shell_stop_windows_update_worker(reach_shell *shell)
{
    if (shell == nullptr)
        return;
    reach_shell_windows_update_worker_state *worker = &shell->windows_update_worker;
    if (!worker->thread_started)
        return;
    {
        std::lock_guard<std::mutex> lock(worker->mutex);
        worker->stop = 1;
        worker->pending = 0;
    }
    worker->cv.notify_one();
    worker->thread.join();
    worker->thread_started = 0;
}

int32_t reach_shell_windows_update_work_pending(const reach_shell *shell)
{
    if (shell == nullptr)
        return 0;
    reach_shell_windows_update_worker_state *worker =
        const_cast<reach_shell_windows_update_worker_state *>(&shell->windows_update_worker);
    std::lock_guard<std::mutex> lock(worker->mutex);
    return worker->pending || worker->in_flight || worker->completed;
}
