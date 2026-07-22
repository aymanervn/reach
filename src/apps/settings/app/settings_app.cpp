#include "settings_app.h"

#include "reach/core/theme.h"
#include "reach/apps/settings/settings.h"
#include "reach/platform/windows_adapters.h"

#include <windows.h>
#include <shlwapi.h>
#include <shellapi.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <new>
#include <string.h>
#include <thread>

enum reach_settings_update_work_type
{
    REACH_SETTINGS_UPDATE_WORK_NONE = 0,
    REACH_SETTINGS_UPDATE_WORK_SCAN,
    REACH_SETTINGS_UPDATE_WORK_INSTALL,
    REACH_SETTINGS_UPDATE_WORK_VERIFY
};

enum reach_settings_reach_work_type
{
    REACH_SETTINGS_REACH_WORK_NONE = 0,
    REACH_SETTINGS_REACH_WORK_CHECK,
    REACH_SETTINGS_REACH_WORK_DOWNLOAD
};

struct reach_settings_reach_worker
{
    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;
    int32_t thread_started;
    int32_t stop;
    int32_t pending;
    int32_t in_flight;
    int32_t completed;
    reach_settings_reach_work_type pending_work;
    reach_settings_reach_work_type completed_work;
    uint16_t url[REACH_APP_UPDATE_URL_CAPACITY];
    uint16_t dest[260];
    reach_app_update_info info;
    reach_result work_result;
    std::atomic<uint64_t> received;
    std::atomic<uint64_t> total;
};

struct reach_settings_update_worker
{
    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;
    int32_t thread_started;
    int32_t stop;
    int32_t pending;
    int32_t in_flight;
    int32_t completed;
    reach_settings_update_work_type pending_work;
    reach_settings_update_work_type completed_work;
    reach_windows_update_identity selected[REACH_WINDOWS_UPDATE_MAX_UPDATES];
    size_t selected_count;
    reach_windows_update_list scan_result;
    reach_windows_update_operation_result operation_result;
    int32_t scan_hresult;
    reach_result work_result;
    std::atomic<int32_t> progress_state;
};

struct reach_settings_app
{
    reach_platform_window_port window;
    reach_render_backend_port renderer;
    reach_monitor_port monitors;
    reach_power_session_port power_session;
    reach_windows_update_port windows_update;
    reach_app_update_port app_update;
    reach_config_store_port config_store;
    reach_user_account_port user_account;
    reach_settings_model model;
    reach_settings_layout layout;
    reach_render_command_buffer render_commands;
    reach_rect_f32 bounds;
    const reach_theme *theme;
    reach_settings_update_worker update_worker;
    reach_settings_reach_worker reach_worker;
    uint16_t app_update_zip[260];
    reach_scrollbar_drag update_scrollbar_drag;
    int32_t running;
    int32_t dirty;
};

static float reach_settings_monitor_scale(const reach_monitor_info *monitor)
{
    if (monitor == nullptr)
    {
        return 1.0f;
    }
    int32_t dpi = monitor->dpi_y > 0 ? monitor->dpi_y : monitor->dpi_x;
    return dpi > 0 ? (float)dpi / 96.0f : 1.0f;
}

static float reach_settings_intersection_area(reach_rect_f32 bounds,
                                              const reach_monitor_info *monitor)
{
    if (monitor == nullptr)
    {
        return 0.0f;
    }
    float left = bounds.x > (float)monitor->bounds.left ? bounds.x : (float)monitor->bounds.left;
    float top = bounds.y > (float)monitor->bounds.top ? bounds.y : (float)monitor->bounds.top;
    float right = bounds.x + bounds.width < (float)monitor->bounds.right
                      ? bounds.x + bounds.width
                      : (float)monitor->bounds.right;
    float bottom = bounds.y + bounds.height < (float)monitor->bounds.bottom
                       ? bounds.y + bounds.height
                       : (float)monitor->bounds.bottom;
    float width = right - left;
    float height = bottom - top;
    return width > 0.0f && height > 0.0f ? width * height : 0.0f;
}

static float reach_settings_app_scale(const reach_settings_app *app)
{
    if (app == nullptr || app->monitors.list == nullptr || app->monitors.ops.count == nullptr ||
        app->monitors.ops.get == nullptr)
    {
        return 1.0f;
    }

    const reach_monitor_info *best = nullptr;
    float best_area = 0.0f;
    size_t count = app->monitors.ops.count(app->monitors.list);
    for (size_t index = 0; index < count; ++index)
    {
        const reach_monitor_info *monitor = app->monitors.ops.get(app->monitors.list, index);
        float area = reach_settings_intersection_area(app->bounds, monitor);
        if (area > best_area)
        {
            best = monitor;
            best_area = area;
        }
    }
    if (best == nullptr && app->monitors.ops.primary != nullptr)
    {
        best = app->monitors.ops.primary(app->monitors.list);
    }
    return reach_settings_monitor_scale(best);
}

static reach_rect_f32 reach_settings_default_bounds(const reach_settings_app *app)
{
    const reach_monitor_info *monitor =
        app != nullptr && app->monitors.list != nullptr && app->monitors.ops.primary != nullptr
            ? app->monitors.ops.primary(app->monitors.list)
            : nullptr;
    reach_rect_f32 monitor_bounds = {0.0f, 0.0f, 1280.0f, 720.0f};
    if (monitor != nullptr)
    {
        monitor_bounds.x = (float)monitor->bounds.left;
        monitor_bounds.y = (float)monitor->bounds.top;
        monitor_bounds.width = (float)(monitor->bounds.right - monitor->bounds.left);
        monitor_bounds.height = (float)(monitor->bounds.bottom - monitor->bounds.top);
    }

    float scale = reach_settings_monitor_scale(monitor);
    float available_width = monitor_bounds.width - 48.0f * scale;
    float available_height = monitor_bounds.height - 48.0f * scale;
    if (available_width < 1.0f)
    {
        available_width = monitor_bounds.width;
    }
    if (available_height < 1.0f)
    {
        available_height = monitor_bounds.height;
    }

    float width = 780.0f * scale;
    float height = 520.0f * scale;
    float minimum_width = 520.0f * scale;
    float minimum_height = 360.0f * scale;
    if (width > available_width)
    {
        width = available_width;
    }
    if (height > available_height)
    {
        height = available_height;
    }
    if (width < minimum_width && minimum_width <= available_width)
    {
        width = minimum_width;
    }
    if (height < minimum_height && minimum_height <= available_height)
    {
        height = minimum_height;
    }

    return {monitor_bounds.x + (monitor_bounds.width - width) * 0.5f,
            monitor_bounds.y + (monitor_bounds.height - height) * 0.5f, width, height};
}

static void reach_settings_refresh_bounds(reach_settings_app *app)
{
    if (app == nullptr || app->window.ops.get_bounds == nullptr ||
        app->window.ops.is_minimized == nullptr || app->window.ops.is_minimized(app->window.window))
    {
        return;
    }
    reach_rect_f32 bounds = {};
    if (app->window.ops.get_bounds(app->window.window, &bounds) == REACH_OK &&
        bounds.width > 0.0f && bounds.height > 0.0f)
    {
        app->bounds = bounds;
    }
}

static void reach_settings_refresh_layout(reach_settings_app *app)
{
    if (app == nullptr)
    {
        return;
    }
    reach_rect_f32 local = {0.0f, 0.0f, app->bounds.width, app->bounds.height};
    app->layout = reach_settings_layout_for_bounds(local, app->theme, reach_settings_app_scale(app),
                                                   &app->model);
}

static reach_result reach_settings_apply_window_style(reach_settings_app *app)
{
    if (app == nullptr || app->window.ops.apply_rounded_corners == nullptr)
    {
        return REACH_OK;
    }
    return app->window.ops.apply_rounded_corners(app->window.window,
                                                 18.0f * reach_settings_app_scale(app));
}

static reach_result reach_settings_render(reach_settings_app *app)
{
    if (app == nullptr || app->renderer.ops.begin_frame == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_settings_render_input input = {};
    input.theme = app->theme;
    input.model = &app->model;
    input.layout = &app->layout;
    input.dpi_scale = reach_settings_app_scale(app);
    input.text_alignment_leading = REACH_TEXT_ALIGNMENT_LEADING;

    reach_render_command_buffer_clear(&app->render_commands);
    reach_result result = reach_settings_build_render_commands(&input, &app->render_commands);
    if (result != REACH_OK)
    {
        return result;
    }
    if (app->renderer.ops.begin_frame(app->renderer.backend) != REACH_OK)
    {
        return REACH_ERROR;
    }
    result = app->renderer.ops.execute(app->renderer.backend, &app->render_commands);
    if (result != REACH_OK)
    {
        return result;
    }
    return app->renderer.ops.end_frame(app->renderer.backend);
}

static void reach_settings_load_power_config(reach_settings_app *app)
{
    if (app == nullptr || app->config_store.ops.load == nullptr)
    {
        return;
    }
    std::unique_ptr<reach_config_snapshot> snapshot(new (std::nothrow) reach_config_snapshot());
    if (snapshot == nullptr ||
        app->config_store.ops.load(app->config_store.store, snapshot.get()) != REACH_OK)
    {
        return;
    }
    reach_settings_model_set_power_minutes(&app->model, REACH_SETTINGS_POWER_TIMER_SLEEP,
                                           snapshot->power_sleep_minutes);
    reach_settings_model_set_power_minutes(&app->model, REACH_SETTINGS_POWER_TIMER_LOCK,
                                           snapshot->power_lock_minutes);
    reach_settings_model_set_power_minutes(&app->model, REACH_SETTINGS_POWER_TIMER_SHUTDOWN,
                                           snapshot->power_shutdown_minutes);
    reach_settings_model_set_power_minutes(&app->model, REACH_SETTINGS_POWER_TIMER_RESTART,
                                           snapshot->power_restart_minutes);
    reach_settings_model_set_power_wait_apps(&app->model, REACH_SETTINGS_POWER_TIMER_SLEEP,
                                             snapshot->power_sleep_wait_apps);
    reach_settings_model_set_power_wait_apps(&app->model, REACH_SETTINGS_POWER_TIMER_SHUTDOWN,
                                             snapshot->power_shutdown_wait_apps);
    reach_settings_model_set_power_wait_apps(&app->model, REACH_SETTINGS_POWER_TIMER_RESTART,
                                             snapshot->power_restart_wait_apps);
    reach_settings_model_power_mark_applied(&app->model);
}

static void reach_settings_save_power_config(reach_settings_app *app)
{
    if (app == nullptr || app->config_store.ops.load == nullptr ||
        app->config_store.ops.save == nullptr)
    {
        return;
    }
    std::unique_ptr<reach_config_snapshot> snapshot(new (std::nothrow) reach_config_snapshot());
    if (snapshot == nullptr ||
        app->config_store.ops.load(app->config_store.store, snapshot.get()) != REACH_OK)
    {
        return;
    }
    snapshot->power_sleep_minutes =
        reach_settings_model_power_minutes(&app->model, REACH_SETTINGS_POWER_TIMER_SLEEP);
    snapshot->power_lock_minutes =
        reach_settings_model_power_minutes(&app->model, REACH_SETTINGS_POWER_TIMER_LOCK);
    snapshot->power_shutdown_minutes =
        reach_settings_model_power_minutes(&app->model, REACH_SETTINGS_POWER_TIMER_SHUTDOWN);
    snapshot->power_restart_minutes =
        reach_settings_model_power_minutes(&app->model, REACH_SETTINGS_POWER_TIMER_RESTART);
    snapshot->power_sleep_wait_apps =
        reach_settings_model_power_wait_apps(&app->model, REACH_SETTINGS_POWER_TIMER_SLEEP);
    snapshot->power_shutdown_wait_apps =
        reach_settings_model_power_wait_apps(&app->model, REACH_SETTINGS_POWER_TIMER_SHUTDOWN);
    snapshot->power_restart_wait_apps =
        reach_settings_model_power_wait_apps(&app->model, REACH_SETTINGS_POWER_TIMER_RESTART);
    if (app->config_store.ops.save(app->config_store.store, snapshot.get()) == REACH_OK)
    {
        (void)reach_windows_notify_config_changed();
    }
}

static void reach_settings_update_progress(void *user, reach_windows_update_progress progress)
{
    reach_settings_app *app = static_cast<reach_settings_app *>(user);
    if (app != nullptr)
    {
        app->update_worker.progress_state = (int32_t)progress;
    }
}

static void reach_settings_update_worker_main(reach_settings_app *app)
{
    reach_settings_update_worker *worker = &app->update_worker;
    for (;;)
    {
        reach_settings_update_work_type work = REACH_SETTINGS_UPDATE_WORK_NONE;
        std::unique_ptr<reach_windows_update_identity[]> selected(
            new (std::nothrow) reach_windows_update_identity[REACH_WINDOWS_UPDATE_MAX_UPDATES]());
        size_t selected_count = 0;

        {
            std::unique_lock<std::mutex> lock(worker->mutex);
            worker->cv.wait(lock, [worker] { return worker->stop || worker->pending; });
            if (worker->stop)
            {
                return;
            }

            work = worker->pending_work;

            selected_count = worker->selected_count;
            if (selected_count > REACH_WINDOWS_UPDATE_MAX_UPDATES)
            {
                selected_count = REACH_WINDOWS_UPDATE_MAX_UPDATES;
            }

            if (selected == nullptr)
            {
                selected_count = 0;
            }

            for (size_t index = 0; index < selected_count; ++index)
            {
                selected[index] = worker->selected[index];
            }

            worker->pending = 0;
            worker->in_flight = 1;
        }

        std::unique_ptr<reach_windows_update_list> scan(new (std::nothrow)
                                                            reach_windows_update_list());
        std::unique_ptr<reach_windows_update_operation_result> operation(
            new (std::nothrow) reach_windows_update_operation_result());
        int32_t scan_hresult = 0;
        reach_result result = REACH_ERROR;
        if (scan != nullptr && operation != nullptr)
        {
            if (work == REACH_SETTINGS_UPDATE_WORK_SCAN && app->windows_update.scan != nullptr)
            {
                result = app->windows_update.scan(app->windows_update.userdata, scan.get(),
                                                  &scan_hresult);
            }
            else if (work == REACH_SETTINGS_UPDATE_WORK_INSTALL &&
                     app->windows_update.install != nullptr)
            {
                result = app->windows_update.install(app->windows_update.userdata, selected.get(),
                                                     selected_count, reach_settings_update_progress,
                                                     app, operation.get());
            }
            else if (work == REACH_SETTINGS_UPDATE_WORK_VERIFY &&
                     app->windows_update.verify != nullptr)
            {
                result = app->windows_update.verify(app->windows_update.userdata, selected.get(),
                                                    selected_count, operation.get());
            }
        }

        {
            std::lock_guard<std::mutex> lock(worker->mutex);
            if (scan != nullptr)
            {
                worker->scan_result = *scan;
            }
            else
            {
                memset(&worker->scan_result, 0, sizeof(worker->scan_result));
            }
            if (operation != nullptr)
            {
                worker->operation_result = *operation;
            }
            else
            {
                memset(&worker->operation_result, 0, sizeof(worker->operation_result));
            }
            worker->scan_hresult = scan_hresult;
            worker->work_result = result;
            worker->completed_work = work;
            worker->completed = 1;
            worker->in_flight = 0;
        }
    }
}

static reach_result reach_settings_ensure_worker(reach_settings_app *app)
{
    if (app->update_worker.thread_started)
    {
        return REACH_OK;
    }
    try
    {
        app->update_worker.stop = 0;
        app->update_worker.thread = std::thread(reach_settings_update_worker_main, app);
        app->update_worker.thread_started = 1;
        return REACH_OK;
    }
    catch (...)
    {
        return REACH_ERROR;
    }
}

static void reach_settings_schedule_scan(reach_settings_app *app)
{
    if (app == nullptr || app->windows_update.scan == nullptr ||
        reach_settings_model_update_busy(&app->model) ||
        reach_settings_ensure_worker(app) != REACH_OK)
    {
        return;
    }
    reach_settings_model_begin_update_scan(&app->model);
    {
        std::lock_guard<std::mutex> lock(app->update_worker.mutex);
        app->update_worker.selected_count = 0;
        app->update_worker.pending_work = REACH_SETTINGS_UPDATE_WORK_SCAN;
        app->update_worker.pending = 1;
    }
    app->update_worker.cv.notify_one();
    app->dirty = 1;
}

static void reach_settings_schedule_install(reach_settings_app *app)
{
    if (app == nullptr || app->windows_update.install == nullptr ||
        reach_settings_model_update_busy(&app->model) ||
        reach_settings_ensure_worker(app) != REACH_OK)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(app->update_worker.mutex);
        app->update_worker.selected_count = 0;
        for (size_t index = 0; index < app->model.update_list.count &&
                               app->update_worker.selected_count < REACH_WINDOWS_UPDATE_MAX_UPDATES;
             ++index)
        {
            if (app->model.update_list.updates[index].selected &&
                app->model.update_list.updates[index].state == REACH_WINDOWS_UPDATE_SELECTED)
            {
                app->update_worker.selected[app->update_worker.selected_count++] =
                    app->model.update_list.updates[index].identity;
            }
        }
        if (app->update_worker.selected_count == 0)
        {
            return;
        }
        app->update_worker.pending_work = REACH_SETTINGS_UPDATE_WORK_INSTALL;
        app->update_worker.pending = 1;
    }
    reach_settings_model_begin_update_install(&app->model);
    app->update_worker.cv.notify_one();
    app->dirty = 1;
}

static void reach_settings_schedule_verification(reach_settings_app *app)
{
    if (app == nullptr || app->windows_update.load_pending_verification == nullptr ||
        app->windows_update.verify == nullptr)
    {
        return;
    }
    std::unique_ptr<reach_windows_update_identity[]> pending(
        new (std::nothrow) reach_windows_update_identity[REACH_WINDOWS_UPDATE_MAX_UPDATES]());
    if (pending == nullptr)
    {
        return;
    }
    size_t count = 0;
    if (app->windows_update.load_pending_verification(app->windows_update.userdata, pending.get(),
                                                      REACH_WINDOWS_UPDATE_MAX_UPDATES,
                                                      &count) != REACH_OK ||
        count == 0 || reach_settings_ensure_worker(app) != REACH_OK)
    {
        return;
    }

    if (count > REACH_WINDOWS_UPDATE_MAX_UPDATES)
    {
        count = REACH_WINDOWS_UPDATE_MAX_UPDATES;
    }

    {
        std::lock_guard<std::mutex> lock(app->update_worker.mutex);
        app->update_worker.selected_count = count;
        for (size_t index = 0; index < count; ++index)
        {
            app->update_worker.selected[index] = pending[index];
        }
        app->update_worker.pending_work = REACH_SETTINGS_UPDATE_WORK_VERIFY;
        app->update_worker.pending = 1;
    }
    app->model.update_page_state = REACH_SETTINGS_UPDATE_VERIFYING;
    app->update_worker.cv.notify_one();
    app->dirty = 1;
}

static void reach_settings_apply_progress(reach_settings_app *app)
{
    int32_t encoded = app->update_worker.progress_state.exchange(0);
    if (encoded == 0)
    {
        return;
    }
    reach_windows_update_progress progress = (reach_windows_update_progress)encoded;
    if (progress == REACH_WINDOWS_UPDATE_PROGRESS_DOWNLOADING)
    {
        app->model.update_page_state = REACH_SETTINGS_UPDATE_DOWNLOADING;
    }
    else if (progress == REACH_WINDOWS_UPDATE_PROGRESS_INSTALLING)
    {
        app->model.update_page_state = REACH_SETTINGS_UPDATE_INSTALLING;
    }
    else if (progress == REACH_WINDOWS_UPDATE_PROGRESS_VERIFYING)
    {
        app->model.update_page_state = REACH_SETTINGS_UPDATE_VERIFYING;
    }
    if (progress != REACH_WINDOWS_UPDATE_PROGRESS_VERIFYING)
    {
        for (size_t index = 0; index < app->model.update_list.count; ++index)
        {
            if (app->model.update_list.updates[index].selected)
            {
                app->model.update_list.updates[index].state =
                    progress == REACH_WINDOWS_UPDATE_PROGRESS_DOWNLOADING
                        ? REACH_WINDOWS_UPDATE_DOWNLOADING
                        : REACH_WINDOWS_UPDATE_INSTALLING;
            }
        }
    }
    app->dirty = 1;
}

static void reach_settings_apply_result(reach_settings_app *app)
{
    reach_settings_update_worker *worker = &app->update_worker;
    reach_settings_update_work_type work = REACH_SETTINGS_UPDATE_WORK_NONE;
    std::unique_ptr<reach_windows_update_list> scan(new (std::nothrow) reach_windows_update_list());
    std::unique_ptr<reach_windows_update_operation_result> operation(
        new (std::nothrow) reach_windows_update_operation_result());
    if (scan == nullptr || operation == nullptr)
    {
        return;
    }
    int32_t scan_hresult = 0;
    reach_result result = REACH_ERROR;
    {
        std::lock_guard<std::mutex> lock(worker->mutex);
        if (!worker->completed)
        {
            return;
        }
        work = worker->completed_work;
        *scan = worker->scan_result;
        *operation = worker->operation_result;
        scan_hresult = worker->scan_hresult;
        result = worker->work_result;
        worker->completed = 0;
    }

    if (work != REACH_SETTINGS_UPDATE_WORK_SCAN)
    {
        for (size_t result_index = 0; result_index < operation->per_update_result_count;
             ++result_index)
        {
            for (size_t update_index = 0; update_index < app->model.update_list.count;
                 ++update_index)
            {
                const reach_windows_update_identity *left =
                    &operation->per_update_results[result_index].identity;
                const reach_windows_update_identity *right =
                    &app->model.update_list.updates[update_index].identity;
                int32_t equal = left->revision_number == right->revision_number;
                for (size_t character = 0; equal && character < REACH_WINDOWS_UPDATE_ID_CAPACITY;
                     ++character)
                {
                    if (left->update_id[character] != right->update_id[character])
                    {
                        equal = 0;
                    }
                    if (left->update_id[character] == 0)
                    {
                        break;
                    }
                }
                if (equal)
                {
                    reach_copy_utf16(operation->per_update_results[result_index].selected_reason,
                                     REACH_WINDOWS_UPDATE_TEXT_CAPACITY,
                                     app->model.update_list.updates[update_index].selected_reason);
                }
            }
        }
    }
    if (result != REACH_OK && operation->failure_class == REACH_WINDOWS_UPDATE_FAILURE_NONE)
    {
        operation->failure_class = work == REACH_SETTINGS_UPDATE_WORK_VERIFY
                                       ? REACH_WINDOWS_UPDATE_VERIFICATION_FAILED
                                       : REACH_WINDOWS_UPDATE_INSTALL_FAILED;
        if (operation->overall_install_hresult == 0)
        {
            operation->overall_install_hresult = -1;
        }
    }
    if (work == REACH_SETTINGS_UPDATE_WORK_SCAN)
    {
        reach_settings_model_apply_update_scan(
            &app->model, result == REACH_OK ? scan.get() : nullptr,
            result == REACH_OK ? scan_hresult : (scan_hresult != 0 ? scan_hresult : -1));
    }
    else
    {
        reach_settings_model_apply_update_operation(&app->model, operation.get());
    }
    app->dirty = 1;
}

static void reach_settings_reach_progress(void *user, uint64_t received, uint64_t total)
{
    reach_settings_app *app = static_cast<reach_settings_app *>(user);
    if (app != nullptr)
    {
        app->reach_worker.received = received;
        app->reach_worker.total = total;
    }
}

static void reach_settings_reach_worker_main(reach_settings_app *app)
{
    reach_settings_reach_worker *worker = &app->reach_worker;
    for (;;)
    {
        reach_settings_reach_work_type work = REACH_SETTINGS_REACH_WORK_NONE;
        uint16_t url[REACH_APP_UPDATE_URL_CAPACITY] = {};
        uint16_t dest[260] = {};
        {
            std::unique_lock<std::mutex> lock(worker->mutex);
            worker->cv.wait(lock, [worker] { return worker->stop || worker->pending; });
            if (worker->stop)
            {
                return;
            }
            work = worker->pending_work;
            reach_copy_utf16(url, REACH_APP_UPDATE_URL_CAPACITY, worker->url);
            reach_copy_utf16(dest, 260, worker->dest);
            worker->pending = 0;
            worker->in_flight = 1;
        }

        reach_app_update_info info = {};
        reach_result result = REACH_ERROR;
        if (work == REACH_SETTINGS_REACH_WORK_CHECK && app->app_update.check != nullptr)
        {
            result = app->app_update.check(app->app_update.userdata,
                                           (const uint16_t *)u"aymanervn",
                                           (const uint16_t *)u"reach", &info);
        }
        else if (work == REACH_SETTINGS_REACH_WORK_DOWNLOAD && app->app_update.download != nullptr)
        {
            result = app->app_update.download(app->app_update.userdata, url, dest,
                                              reach_settings_reach_progress, app);
        }

        {
            std::lock_guard<std::mutex> lock(worker->mutex);
            worker->info = info;
            worker->work_result = result;
            worker->completed_work = work;
            worker->completed = 1;
            worker->in_flight = 0;
        }
    }
}

static reach_result reach_settings_ensure_reach_worker(reach_settings_app *app)
{
    if (app->reach_worker.thread_started)
    {
        return REACH_OK;
    }
    try
    {
        app->reach_worker.stop = 0;
        app->reach_worker.thread = std::thread(reach_settings_reach_worker_main, app);
        app->reach_worker.thread_started = 1;
        return REACH_OK;
    }
    catch (...)
    {
        return REACH_ERROR;
    }
}

static int32_t reach_settings_reach_worker_busy(reach_settings_app *app)
{
    std::lock_guard<std::mutex> lock(app->reach_worker.mutex);
    return app->reach_worker.pending || app->reach_worker.in_flight;
}

static void reach_settings_schedule_reach_check(reach_settings_app *app)
{
    if (app == nullptr || app->app_update.check == nullptr ||
        reach_settings_reach_worker_busy(app) ||
        reach_settings_ensure_reach_worker(app) != REACH_OK)
    {
        return;
    }
    reach_settings_model_begin_reach_check(&app->model);
    {
        std::lock_guard<std::mutex> lock(app->reach_worker.mutex);
        app->reach_worker.pending_work = REACH_SETTINGS_REACH_WORK_CHECK;
        app->reach_worker.pending = 1;
    }
    app->reach_worker.cv.notify_one();
    app->dirty = 1;
}

static void reach_settings_schedule_reach_download(reach_settings_app *app)
{
    if (app == nullptr || app->app_update.download == nullptr ||
        app->model.reach_update_info.download_url[0] == 0 ||
        reach_settings_reach_worker_busy(app) ||
        reach_settings_ensure_reach_worker(app) != REACH_OK)
    {
        return;
    }

    wchar_t temp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, temp);
    std::wstring dir = std::wstring(temp) + L"reach_update";
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring zip = dir + L"\\reach_update.zip";
    reach_copy_utf16(app->app_update_zip, 260, reinterpret_cast<const uint16_t *>(zip.c_str()));

    reach_settings_model_begin_reach_download(&app->model);
    {
        std::lock_guard<std::mutex> lock(app->reach_worker.mutex);
        app->reach_worker.received = 0;
        app->reach_worker.total = 0;
        reach_copy_utf16(app->reach_worker.url, REACH_APP_UPDATE_URL_CAPACITY,
                         app->model.reach_update_info.download_url);
        reach_copy_utf16(app->reach_worker.dest, 260,
                         reinterpret_cast<const uint16_t *>(zip.c_str()));
        app->reach_worker.pending_work = REACH_SETTINGS_REACH_WORK_DOWNLOAD;
        app->reach_worker.pending = 1;
    }
    app->reach_worker.cv.notify_one();
    app->dirty = 1;
}

static void reach_settings_launch_updater(reach_settings_app *app)
{
    wchar_t module[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, module, MAX_PATH) == 0)
    {
        return;
    }
    wchar_t install_dir[MAX_PATH] = {};
    reach_copy_utf16(reinterpret_cast<uint16_t *>(install_dir), MAX_PATH,
                     reinterpret_cast<const uint16_t *>(module));
    PathRemoveFileSpecW(install_dir);

    wchar_t source_updater[MAX_PATH] = {};
    reach_copy_utf16(reinterpret_cast<uint16_t *>(source_updater), MAX_PATH,
                     reinterpret_cast<const uint16_t *>(install_dir));
    PathAppendW(source_updater, L"reachUpdater.exe");

    wchar_t temp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, temp);
    std::wstring temp_updater = std::wstring(temp) + L"reachUpdater.exe";
    if (!CopyFileW(source_updater, temp_updater.c_str(), FALSE))
    {
        return;
    }

    std::wstring parameters = L"\"";
    parameters += reinterpret_cast<const wchar_t *>(app->app_update_zip);
    parameters += L"\" \"";
    parameters += install_dir;
    parameters += L"\" 1";
    ShellExecuteW(nullptr, L"open", temp_updater.c_str(), parameters.c_str(), nullptr, SW_HIDE);
    app->running = 0;
}

static void reach_settings_apply_reach_progress(reach_settings_app *app)
{
    uint64_t received = app->reach_worker.received.load();
    uint64_t total = app->reach_worker.total.load();
    if (received != app->model.reach_download_received ||
        total != app->model.reach_download_total)
    {
        app->model.reach_download_received = received;
        app->model.reach_download_total = total;
        app->dirty = 1;
    }
}

static void reach_settings_apply_reach_result(reach_settings_app *app)
{
    reach_settings_reach_worker *worker = &app->reach_worker;
    reach_settings_reach_work_type work = REACH_SETTINGS_REACH_WORK_NONE;
    reach_app_update_info info = {};
    reach_result result = REACH_ERROR;
    {
        std::lock_guard<std::mutex> lock(worker->mutex);
        if (!worker->completed)
        {
            return;
        }
        work = worker->completed_work;
        info = worker->info;
        result = worker->work_result;
        worker->completed = 0;
    }

    if (work == REACH_SETTINGS_REACH_WORK_CHECK)
    {
        reach_settings_model_apply_reach_check(&app->model, result == REACH_OK ? &info : nullptr,
                                               result == REACH_OK ? 1 : 0);
    }
    else if (work == REACH_SETTINGS_REACH_WORK_DOWNLOAD)
    {
        if (result == REACH_OK)
        {
            reach_settings_model_apply_reach_download(&app->model, 1);
            reach_settings_launch_updater(app);
        }
        else
        {
            reach_settings_model_apply_reach_download(&app->model, 0);
        }
    }
    app->dirty = 1;
}

static void reach_settings_submit_password_change(reach_settings_app *app)
{
    if (app->user_account.ops.verify_password != nullptr)
    {
        int32_t current_valid = 1;
        if (app->user_account.ops.verify_password(
                app->user_account.account,
                app->model.account_password_edits[REACH_SETTINGS_ACCOUNT_FIELD_CURRENT].text,
                &current_valid) == REACH_OK &&
            !current_valid)
        {
            reach_settings_model_account_apply_status(
                &app->model, REACH_SETTINGS_ACCOUNT_STATUS_WRONG_CURRENT);
            return;
        }
    }
    if (!reach_settings_model_account_submit_ready(&app->model))
    {
        return;
    }
    if (app->user_account.ops.change_password == nullptr)
    {
        reach_settings_model_account_apply_status(&app->model,
                                                  REACH_SETTINGS_ACCOUNT_STATUS_ERROR);
        return;
    }
    reach_user_account_password_status status = REACH_USER_ACCOUNT_PASSWORD_FAILED;
    reach_result result = app->user_account.ops.change_password(
        app->user_account.account,
        app->model.account_password_edits[REACH_SETTINGS_ACCOUNT_FIELD_CURRENT].text,
        app->model.account_password_edits[REACH_SETTINGS_ACCOUNT_FIELD_NEW].text, &status);
    int32_t model_status = REACH_SETTINGS_ACCOUNT_STATUS_ERROR;
    if (result == REACH_OK && status == REACH_USER_ACCOUNT_PASSWORD_CHANGED)
    {
        model_status = REACH_SETTINGS_ACCOUNT_STATUS_SUCCESS;
    }
    else if (status == REACH_USER_ACCOUNT_PASSWORD_WRONG_CURRENT)
    {
        model_status = REACH_SETTINGS_ACCOUNT_STATUS_WRONG_CURRENT;
    }
    else if (status == REACH_USER_ACCOUNT_PASSWORD_POLICY)
    {
        model_status = REACH_SETTINGS_ACCOUNT_STATUS_POLICY;
    }
    reach_settings_model_account_apply_status(&app->model, model_status);
}

static void reach_settings_handle_pointer_up(reach_settings_app *app, const reach_ui_event *event)
{
    if (app->model.pressed_button != REACH_SETTINGS_HIT_NONE)
    {
        reach_settings_model_release_button(&app->model);
        app->dirty = 1;
    }
    if (app->update_scrollbar_drag.active)
    {
        reach_scrollbar_end_drag(&app->update_scrollbar_drag);
        if (app->window.ops.set_pointer_capture != nullptr)
        {
            (void)app->window.ops.set_pointer_capture(app->window.window, 0);
        }
        return;
    }
    reach_settings_refresh_bounds(app);
    float x = (float)event->x - app->bounds.x;
    float y = (float)event->y - app->bounds.y;
    reach_settings_hit_result hit = reach_settings_hit_test(&app->layout, x, y);
    if (hit.type == REACH_SETTINGS_HIT_CLOSE)
    {
        app->running = 0;
    }
    else if (hit.type == REACH_SETTINGS_HIT_MINIMIZE)
    {
        if (app->window.ops.minimize != nullptr)
        {
            (void)app->window.ops.minimize(app->window.window);
        }
    }
    else if (hit.type == REACH_SETTINGS_HIT_NAV_ITEM)
    {
        reach_settings_model_select_page(&app->model, hit.page);
        app->dirty = 1;
    }
    else if (app->model.selected_page == REACH_SETTINGS_PAGE_UPDATE)
    {
        if (hit.type == REACH_SETTINGS_HIT_UPDATE_REFRESH)
        {
            reach_settings_schedule_scan(app);
            reach_settings_schedule_reach_check(app);
        }
        else if (hit.type == REACH_SETTINGS_HIT_REACH_UPDATE)
        {
            if (app->model.reach_update_state == REACH_SETTINGS_REACH_UPDATE_AVAILABLE)
            {
                reach_settings_schedule_reach_download(app);
            }
            else
            {
                reach_settings_schedule_reach_check(app);
            }
        }
        else if (hit.type == REACH_SETTINGS_HIT_UPDATE_INSTALL)
        {
            reach_settings_schedule_install(app);
        }
        else if (hit.type == REACH_SETTINGS_HIT_UPDATE_RESTART &&
                 !reach_settings_model_update_busy(&app->model) &&
                 reach_settings_model_restart_required_count(&app->model) > 0 &&
                 app->power_session.ops.restart != nullptr)
        {
            (void)app->power_session.ops.restart(app->power_session.session);
        }
        else if (hit.type == REACH_SETTINGS_HIT_UPDATE_CHECKBOX)
        {
            reach_settings_model_toggle_update(&app->model, hit.update_index);
            app->dirty = 1;
        }
    }
    else if (app->model.selected_page == REACH_SETTINGS_PAGE_ACCOUNT)
    {
        if (hit.type == REACH_SETTINGS_HIT_ACCOUNT_PASSWORD_FIELD)
        {
            reach_settings_model_account_focus_password(&app->model, hit.account_field);
        }
        else if (hit.type == REACH_SETTINGS_HIT_ACCOUNT_PASSWORD)
        {
            reach_settings_submit_password_change(app);
        }
        else
        {
            reach_settings_model_account_blur(&app->model);
        }
        app->dirty = 1;
    }
    else if (app->model.selected_page == REACH_SETTINGS_PAGE_POWER_SLEEP)
    {
        if (hit.type == REACH_SETTINGS_HIT_POWER_OPTION)
        {
            if (hit.power_option == REACH_SETTINGS_POWER_CUSTOM_OPTION)
            {
                reach_settings_model_power_focus_custom(&app->model, hit.power_timer,
                                                        hit.power_custom_field);
            }
            else
            {
                reach_settings_model_power_blur(&app->model);
                reach_settings_model_select_power_option(&app->model, hit.power_timer,
                                                         hit.power_option);
            }
        }
        else if (hit.type == REACH_SETTINGS_HIT_POWER_WAIT_TOGGLE)
        {
            reach_settings_model_power_blur(&app->model);
            (void)reach_settings_model_toggle_power_wait_apps(&app->model, hit.power_timer);
        }
        else if (hit.type == REACH_SETTINGS_HIT_POWER_APPLY)
        {
            reach_settings_model_power_blur(&app->model);
            if (reach_settings_model_power_dirty(&app->model))
            {
                reach_settings_save_power_config(app);
                reach_settings_model_power_mark_applied(&app->model);
            }
        }
        else
        {
            reach_settings_model_power_blur(&app->model);
        }
        app->dirty = 1;
    }
}

static reach_text_edit_key reach_settings_map_edit_key(const reach_ui_event *event,
                                                       int32_t *out_select_all)
{
    *out_select_all = 0;
    switch ((reach_ui_edit_key)event->id)
    {
    case REACH_UI_EDIT_KEY_BACKSPACE:
        return REACH_TEXT_EDIT_KEY_BACKSPACE;
    case REACH_UI_EDIT_KEY_DELETE:
        return REACH_TEXT_EDIT_KEY_DELETE;
    case REACH_UI_EDIT_KEY_LEFT:
        return REACH_TEXT_EDIT_KEY_LEFT;
    case REACH_UI_EDIT_KEY_RIGHT:
        return REACH_TEXT_EDIT_KEY_RIGHT;
    case REACH_UI_EDIT_KEY_HOME:
        return REACH_TEXT_EDIT_KEY_HOME;
    case REACH_UI_EDIT_KEY_END:
        return REACH_TEXT_EDIT_KEY_END;
    case REACH_UI_EDIT_KEY_SELECT_ALL:
        *out_select_all = 1;
        return REACH_TEXT_EDIT_KEY_NONE;
    case REACH_UI_EDIT_KEY_NONE:
    default:
        return REACH_TEXT_EDIT_KEY_NONE;
    }
}

static void reach_settings_handle_text_event(reach_settings_app *app, const reach_ui_event *event)
{
    int32_t power_focused = app->model.selected_page == REACH_SETTINGS_PAGE_POWER_SLEEP &&
                            app->model.power_focused_timer >= 0;
    int32_t account_focused = app->model.selected_page == REACH_SETTINGS_PAGE_ACCOUNT &&
                              app->model.account_focused_field >= 0;
    if (!power_focused && !account_focused)
    {
        return;
    }
    int32_t handled = 0;
    if (event->type == REACH_UI_EVENT_TEXT_CHAR)
    {
        handled = power_focused
                      ? reach_settings_model_power_insert_char(&app->model, (uint16_t)event->id)
                      : reach_settings_model_account_insert_char(&app->model, (uint16_t)event->id);
    }
    else
    {
        reach_text_edit_modifiers modifiers = {};
        modifiers.shift = (event->modifiers & REACH_UI_EVENT_MODIFIER_SHIFT) ? 1 : 0;
        modifiers.ctrl = (event->modifiers & REACH_UI_EVENT_MODIFIER_CTRL) ? 1 : 0;
        int32_t select_all = 0;
        reach_text_edit_key key = reach_settings_map_edit_key(event, &select_all);
        if (select_all)
        {
            if (power_focused)
            {
                reach_text_edit_select_all(
                    &app->model.power_custom_edits[app->model.power_focused_timer]
                                                  [app->model.power_focused_field]);
            }
            else
            {
                reach_text_edit_select_all(
                    &app->model.account_password_edits[app->model.account_focused_field]);
            }
            handled = 1;
        }
        if (key != REACH_TEXT_EDIT_KEY_NONE)
        {
            handled = power_focused
                          ? reach_settings_model_power_handle_edit_key(&app->model, key, modifiers)
                          : reach_settings_model_account_handle_edit_key(&app->model, key,
                                                                         modifiers);
        }
    }
    if (handled)
    {
        app->dirty = 1;
    }
}

static int32_t reach_settings_hit_is_button(reach_settings_hit_type type)
{
    return type == REACH_SETTINGS_HIT_UPDATE_REFRESH || type == REACH_SETTINGS_HIT_UPDATE_INSTALL ||
           type == REACH_SETTINGS_HIT_UPDATE_RESTART || type == REACH_SETTINGS_HIT_REACH_UPDATE ||
           type == REACH_SETTINGS_HIT_POWER_APPLY || type == REACH_SETTINGS_HIT_ACCOUNT_PASSWORD;
}

static void reach_settings_handle_pointer_down(reach_settings_app *app, const reach_ui_event *event)
{
    if (app == nullptr || event == nullptr)
    {
        return;
    }
    reach_settings_refresh_bounds(app);
    float x = (float)event->x - app->bounds.x;
    float y = (float)event->y - app->bounds.y;
    reach_settings_hit_result hit = reach_settings_hit_test(&app->layout, x, y);
    if (reach_settings_hit_is_button(hit.type))
    {
        reach_settings_model_press_button(&app->model, hit.type);
        app->dirty = 1;
    }
    if (app->model.selected_page != REACH_SETTINGS_PAGE_UPDATE)
    {
        return;
    }
    if (hit.type != REACH_SETTINGS_HIT_UPDATE_SCROLLBAR_TRACK &&
        hit.type != REACH_SETTINGS_HIT_UPDATE_SCROLLBAR_THUMB)
    {
        return;
    }
    reach_scrollbar_layout layout = {app->layout.update_scrollbar_track,
                                     app->layout.update_scrollbar_thumb};
    reach_scrollbar_begin_drag(&app->model.update_scrollbar, &app->update_scrollbar_drag, &layout,
                               y, hit.type == REACH_SETTINGS_HIT_UPDATE_SCROLLBAR_THUMB);
    if (app->window.ops.set_pointer_capture != nullptr)
    {
        (void)app->window.ops.set_pointer_capture(app->window.window, 1);
    }
    app->dirty = 1;
}

static void reach_settings_handle_pointer_move(reach_settings_app *app, const reach_ui_event *event)
{
    if (app == nullptr || event == nullptr || !app->update_scrollbar_drag.active)
    {
        return;
    }
    reach_settings_refresh_bounds(app);
    float y = (float)event->y - app->bounds.y;
    reach_scrollbar_layout layout = {app->layout.update_scrollbar_track,
                                     app->layout.update_scrollbar_thumb};
    reach_scrollbar_update_drag(&app->model.update_scrollbar, &app->update_scrollbar_drag, &layout,
                                y);
    app->dirty = 1;
}

static void reach_settings_handle_event(void *user, const reach_ui_event *event)
{
    reach_settings_app *app = static_cast<reach_settings_app *>(user);
    if (app == nullptr || event == nullptr)
    {
        return;
    }
    else if (event->type == REACH_UI_EVENT_POINTER_DOWN)
    {
        reach_settings_handle_pointer_down(app, event);
    }
    else if (event->type == REACH_UI_EVENT_POINTER_MOVE)
    {
        reach_settings_handle_pointer_move(app, event);
    }
    else if (event->type == REACH_UI_EVENT_POINTER_UP)
    {
        reach_settings_handle_pointer_up(app, event);
    }
    else if (event->type == REACH_UI_EVENT_POINTER_CANCEL)
    {
        reach_scrollbar_end_drag(&app->update_scrollbar_drag);
        if (app->model.pressed_button != REACH_SETTINGS_HIT_NONE)
        {
            reach_settings_model_release_button(&app->model);
            app->dirty = 1;
        }
    }
    else if (event->type == REACH_UI_EVENT_TEXT_CHAR || event->type == REACH_UI_EVENT_TEXT_EDIT)
    {
        reach_settings_handle_text_event(app, event);
    }
    else if (event->type == REACH_UI_EVENT_ENTER || event->type == REACH_UI_EVENT_ESCAPE)
    {
        if (app->model.power_focused_timer >= 0)
        {
            reach_settings_model_power_blur(&app->model);
            app->dirty = 1;
        }
        if (app->model.account_focused_field >= 0)
        {
            if (event->type == REACH_UI_EVENT_ENTER)
            {
                reach_settings_submit_password_change(app);
            }
            else
            {
                reach_settings_model_account_blur(&app->model);
            }
            app->dirty = 1;
        }
    }
    else if (event->type == REACH_UI_EVENT_POINTER_WHEEL &&
             app->model.selected_page == REACH_SETTINGS_PAGE_UPDATE && event->wheel_delta != 0)
    {
        reach_settings_model_scroll_updates(
            &app->model, event->wheel_delta > 0 ? -86.0f * reach_settings_app_scale(app)
                                                : 86.0f * reach_settings_app_scale(app));
        app->dirty = 1;
    }
    else if (event->type == REACH_UI_EVENT_DISPLAY_CHANGED ||
             event->type == REACH_UI_EVENT_WINDOW_BOUNDS_CHANGED)
    {
        if (event->type == REACH_UI_EVENT_DISPLAY_CHANGED && app->monitors.ops.refresh != nullptr)
        {
            (void)app->monitors.ops.refresh(app->monitors.list);
        }
        reach_settings_refresh_bounds(app);
        reach_settings_refresh_layout(app);
        (void)reach_settings_apply_window_style(app);
        app->dirty = 1;
    }
}

reach_result reach_settings_app_create(reach_settings_app **out_app)
{
    if (out_app == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_app = nullptr;

    reach_settings_app *app = new (std::nothrow) reach_settings_app();
    if (app == nullptr)
    {
        return REACH_ERROR;
    }
    app->theme = reach_theme_default();
    reach_settings_model_init(&app->model);

    reach_result result =
        reach_windows_create_platform_window(REACH_SURFACE_SETTINGS, &app->window);
    if (result == REACH_OK)
    {
        result = reach_windows_create_dcomp_render_backend(app->window.window, &app->renderer);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_monitor_list(&app->monitors);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_power_session(&app->power_session);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_windows_update(&app->windows_update);
    }
    if (result != REACH_OK)
    {
        reach_settings_app_destroy(app);
        return result;
    }

    (void)reach_windows_create_app_update(&app->app_update);

    if (reach_windows_create_user_account(&app->user_account) == REACH_OK &&
        app->user_account.ops.query != nullptr)
    {
        reach_user_account_info info = {};
        if (app->user_account.ops.query(app->user_account.account, &info) == REACH_OK)
        {
            reach_settings_model_set_account(&app->model, info.display_name, info.user_name,
                                             info.is_administrator, info.picture_icon_id);
        }
    }

    uint16_t config_path[260] = {};
    if (reach_windows_default_config_path(config_path, 260) == REACH_OK &&
        reach_windows_create_config_store(config_path, &app->config_store) == REACH_OK)
    {
        reach_settings_load_power_config(app);
    }

    app->bounds = reach_settings_default_bounds(app);
    reach_settings_refresh_layout(app);
    *out_app = app;
    return REACH_OK;
}

reach_result reach_settings_app_start(reach_settings_app *app)
{
    if (app == nullptr || app->window.ops.set_bounds == nullptr ||
        app->window.ops.set_event_callback == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_result result =
        app->window.ops.set_event_callback(app->window.window, reach_settings_handle_event, app);
    if (result == REACH_OK)
    {
        result = app->window.ops.set_bounds(app->window.window, app->bounds);
    }
    if (result == REACH_OK)
    {
        result = reach_settings_apply_window_style(app);
    }
    if (result == REACH_OK && app->window.ops.show != nullptr)
    {
        result = app->window.ops.show(app->window.window);
    }
    if (result != REACH_OK)
    {
        return result;
    }
    app->running = 1;
    app->dirty = 1;
    reach_settings_schedule_verification(app);
    reach_settings_schedule_reach_check(app);
    return REACH_OK;
}

reach_result reach_settings_app_update(reach_settings_app *app, double delta_seconds)
{
    if (app == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_settings_apply_progress(app);
    reach_settings_apply_result(app);
    reach_settings_apply_reach_progress(app);
    reach_settings_apply_reach_result(app);
    if (reach_settings_model_update_scroll(&app->model, delta_seconds))
    {
        app->dirty = 1;
    }
    if (reach_settings_model_tick_power_animations(&app->model, delta_seconds))
    {
        app->dirty = 1;
    }
    if (reach_settings_model_tick_power_caret(&app->model, delta_seconds))
    {
        app->dirty = 1;
    }
    if (reach_settings_model_tick_account_caret(&app->model, delta_seconds))
    {
        app->dirty = 1;
    }
    if (reach_settings_model_tick_button_press(&app->model, delta_seconds))
    {
        app->dirty = 1;
    }
    if (app->dirty)
    {
        reach_settings_refresh_bounds(app);
        reach_settings_refresh_layout(app);
        reach_result result = reach_settings_render(app);
        if (result != REACH_OK)
        {
            return result;
        }
        app->dirty = 0;
    }
    return REACH_OK;
}

reach_result reach_settings_app_dispatch_events(reach_settings_app *app)
{
    if (app == nullptr || app->window.ops.dispatch_events == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    return app->window.ops.dispatch_events(app->window.window);
}

int32_t reach_settings_app_has_pending_events(const reach_settings_app *app)
{
    return app != nullptr && app->window.ops.has_pending_events != nullptr &&
           app->window.ops.has_pending_events(app->window.window);
}

int32_t reach_settings_app_needs_frame(const reach_settings_app *app)
{
    if (app == nullptr)
    {
        return 0;
    }
    reach_settings_reach_worker *reach_worker =
        const_cast<reach_settings_reach_worker *>(&app->reach_worker);
    int32_t reach_busy = 0;
    {
        std::lock_guard<std::mutex> reach_lock(reach_worker->mutex);
        reach_busy = reach_worker->pending || reach_worker->in_flight || reach_worker->completed;
    }
    reach_settings_update_worker *worker =
        const_cast<reach_settings_update_worker *>(&app->update_worker);
    std::lock_guard<std::mutex> lock(worker->mutex);
    return app->dirty || reach_busy || worker->pending || worker->in_flight || worker->completed ||
           app->update_worker.progress_state.load() != 0 ||
           app->model.update_scrollbar.offset != app->model.update_scrollbar.target ||
           app->update_scrollbar_drag.active ||
           reach_settings_model_power_animations_active(&app->model) ||
           reach_settings_model_button_press_active(&app->model) ||
           app->model.power_focused_timer >= 0 || app->model.account_focused_field >= 0;
}

int32_t reach_settings_app_running(const reach_settings_app *app)
{
    return app != nullptr && app->running;
}

void reach_settings_app_activate(reach_settings_app *app)
{
    if (app != nullptr && app->window.ops.raise != nullptr)
    {
        (void)app->window.ops.raise(app->window.window);
    }
}

void reach_settings_app_destroy(reach_settings_app *app)
{
    if (app == nullptr)
    {
        return;
    }
    if (app->update_worker.thread_started)
    {
        {
            std::lock_guard<std::mutex> lock(app->update_worker.mutex);
            app->update_worker.stop = 1;
            app->update_worker.pending = 0;
        }
        if (app->windows_update.cancel != nullptr)
        {
            app->windows_update.cancel(app->windows_update.userdata);
        }
        app->update_worker.cv.notify_one();
        app->update_worker.thread.join();
    }
    if (app->reach_worker.thread_started)
    {
        {
            std::lock_guard<std::mutex> lock(app->reach_worker.mutex);
            app->reach_worker.stop = 1;
            app->reach_worker.pending = 0;
        }
        if (app->app_update.cancel != nullptr)
        {
            app->app_update.cancel(app->app_update.userdata);
        }
        app->reach_worker.cv.notify_one();
        app->reach_worker.thread.join();
    }
    if (app->app_update.destroy != nullptr)
    {
        app->app_update.destroy(app->app_update.userdata);
    }
    if (app->windows_update.destroy != nullptr)
    {
        app->windows_update.destroy(app->windows_update.userdata);
    }
    if (app->config_store.ops.destroy != nullptr)
    {
        app->config_store.ops.destroy(app->config_store.store);
    }
    if (app->user_account.ops.destroy != nullptr)
    {
        app->user_account.ops.destroy(app->user_account.account);
    }
    if (app->power_session.ops.destroy != nullptr)
    {
        app->power_session.ops.destroy(app->power_session.session);
    }
    if (app->monitors.ops.destroy != nullptr)
    {
        app->monitors.ops.destroy(app->monitors.list);
    }
    if (app->renderer.ops.destroy != nullptr)
    {
        app->renderer.ops.destroy(app->renderer.backend);
    }
    if (app->window.ops.destroy != nullptr)
    {
        app->window.ops.destroy(app->window.window);
    }
    delete app;
}
