#include "settings_app.h"

#include "reach/core/theme.h"
#include "reach/apps/settings/settings.h"
#include "reach/platform/windows_adapters.h"
#include "reach/services/bluetooth.h"
#include "reach/services/config.h"
#include "reach/services/wifi.h"

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
    REACH_SETTINGS_UPDATE_WORK_VERIFY,
    REACH_SETTINGS_UPDATE_WORK_RESUME
};

enum reach_settings_reach_work_type
{
    REACH_SETTINGS_REACH_WORK_NONE = 0,
    REACH_SETTINGS_REACH_WORK_CHECK,
    REACH_SETTINGS_REACH_WORK_DOWNLOAD
};

enum reach_settings_startup_work_type
{
    REACH_SETTINGS_STARTUP_WORK_NONE = 0,
    REACH_SETTINGS_STARTUP_WORK_ENUMERATE,
    REACH_SETTINGS_STARTUP_WORK_SET_ENABLED
};

struct reach_settings_startup_worker
{
    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;
    int32_t thread_started;
    int32_t stop;
    int32_t pending;
    int32_t in_flight;
    int32_t completed;
    reach_settings_startup_work_type pending_work;
    reach_settings_startup_work_type completed_work;
    reach_startup_app_entry target;
    int32_t target_enabled;
    size_t target_index;
    reach_startup_app_list list;
    reach_result work_result;
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
    reach_config_service *config_service;
    reach_user_account_port user_account;
    reach_startup_apps_port startup_apps;
    reach_icon_provider_port icon_provider;
    reach_system_controls_port system_controls;
    reach_wifi_service *wifi_service;
    reach_bluetooth_service *bluetooth_service;
    std::atomic<int32_t> radio_notify;
    std::atomic<uint32_t> system_controls_change_flags;
    reach_settings_model model;
    reach_settings_layout layout;
    reach_render_command_buffer render_commands;
    reach_rect_f32 bounds;
    const reach_theme *theme;
    reach_settings_update_worker update_worker;
    reach_settings_reach_worker reach_worker;
    reach_settings_startup_worker startup_worker;
    reach_icon_handle startup_icon_handles[REACH_STARTUP_APP_MAX_ENTRIES];
    size_t startup_icon_count;
    reach_icon_handle bluetooth_icon_handles[REACH_BLUETOOTH_MAX_DEVICES];
    size_t bluetooth_icon_count;
    uint16_t app_update_zip[260];
    reach_scrollbar_drag update_scrollbar_drag;
    reach_scrollbar_drag startup_scrollbar_drag;
    reach_scrollbar_drag wifi_scrollbar_drag;
    reach_scrollbar_drag bluetooth_scrollbar_drag;
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
    float height = 780.0f * scale;
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

static void reach_settings_apply_caption(reach_settings_app *app)
{
    if (app == nullptr || app->window.ops.set_caption == nullptr)
    {
        return;
    }
    const reach_settings_layout *layout = &app->layout;
    reach_platform_window_caption caption = {};
    caption.bounds = {layout->content.x, layout->content.y, layout->content.width,
                      layout->content_title.y + layout->content_title.height - layout->content.y};
    caption.exclusions[0] = layout->close_button;
    caption.exclusions[1] = layout->minimize_button;
    caption.exclusion_count = 2;
    (void)app->window.ops.set_caption(app->window.window, &caption);
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
    reach_settings_apply_caption(app);
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
    reach_result end_result = app->renderer.ops.end_frame(app->renderer.backend);
    return result != REACH_OK ? result : end_result;
}

static void reach_settings_load_power_config(reach_settings_app *app)
{
    if (app == nullptr || app->config_service == nullptr)
    {
        return;
    }
    std::unique_ptr<reach_config_snapshot> snapshot(new (std::nothrow) reach_config_snapshot());
    if (snapshot == nullptr ||
        reach_config_service_snapshot(app->config_service, snapshot.get()) != REACH_OK)
    {
        return;
    }
    reach_settings_model_set_power_minutes(&app->model, REACH_SETTINGS_POWER_TIMER_SCREEN_OFF,
                                           snapshot->power_screen_off_minutes);
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
    if (app == nullptr || app->config_service == nullptr)
    {
        return;
    }
    reach_config_power_settings settings = {};
    settings.screen_off_minutes =
        reach_settings_model_power_minutes(&app->model, REACH_SETTINGS_POWER_TIMER_SCREEN_OFF);
    settings.sleep_minutes =
        reach_settings_model_power_minutes(&app->model, REACH_SETTINGS_POWER_TIMER_SLEEP);
    settings.lock_minutes =
        reach_settings_model_power_minutes(&app->model, REACH_SETTINGS_POWER_TIMER_LOCK);
    settings.shutdown_minutes =
        reach_settings_model_power_minutes(&app->model, REACH_SETTINGS_POWER_TIMER_SHUTDOWN);
    settings.restart_minutes =
        reach_settings_model_power_minutes(&app->model, REACH_SETTINGS_POWER_TIMER_RESTART);
    settings.sleep_wait_apps =
        reach_settings_model_power_wait_apps(&app->model, REACH_SETTINGS_POWER_TIMER_SLEEP);
    settings.shutdown_wait_apps =
        reach_settings_model_power_wait_apps(&app->model, REACH_SETTINGS_POWER_TIMER_SHUTDOWN);
    settings.restart_wait_apps =
        reach_settings_model_power_wait_apps(&app->model, REACH_SETTINGS_POWER_TIMER_RESTART);
    (void)reach_config_service_set_power(app->config_service, &settings);
}

static void reach_settings_apply_ui_font(reach_settings_app *app)
{
    if (app == nullptr || app->renderer.ops.set_ui_font == nullptr)
    {
        return;
    }
    app->renderer.ops.set_ui_font(app->renderer.backend,
                                  reach_settings_model_bundled_font(&app->model));
    app->dirty = 1;
}

static void reach_settings_apply_theme(reach_settings_app *app)
{
    if (app == nullptr)
    {
        return;
    }
    app->theme =
        reach_theme_for_mode(reach_settings_model_light_theme(&app->model) ? REACH_THEME_MODE_LIGHT
                                                                           : REACH_THEME_MODE_DARK);
    reach_settings_refresh_layout(app);
    app->dirty = 1;
}

static void reach_settings_load_display_config(reach_settings_app *app)
{
    if (app == nullptr || app->config_service == nullptr)
    {
        return;
    }
    std::unique_ptr<reach_config_snapshot> snapshot(new (std::nothrow) reach_config_snapshot());
    if (snapshot == nullptr ||
        reach_config_service_snapshot(app->config_service, snapshot.get()) != REACH_OK)
    {
        return;
    }
    reach_settings_model_set_high_refresh_rate(&app->model, snapshot->high_refresh_rate);
    reach_settings_model_set_bundled_font(&app->model, snapshot->bundled_font);
    reach_settings_apply_ui_font(app);
    reach_settings_model_set_light_theme(&app->model, snapshot->light_theme);
    reach_settings_model_set_windows_system_theme(&app->model, snapshot->windows_system_theme);
    reach_settings_model_set_windows_app_theme(&app->model, snapshot->windows_app_theme);
    reach_settings_apply_theme(app);
}

static void reach_settings_save_display_config(reach_settings_app *app)
{
    if (app == nullptr || app->config_service == nullptr)
    {
        return;
    }
    reach_config_display_settings settings = {};
    settings.high_refresh_rate = reach_settings_model_high_refresh_rate(&app->model);
    settings.bundled_font = reach_settings_model_bundled_font(&app->model);
    settings.light_theme = reach_settings_model_light_theme(&app->model);
    settings.windows_system_theme = reach_settings_model_windows_system_theme(&app->model);
    settings.windows_app_theme = reach_settings_model_windows_app_theme(&app->model);
    (void)reach_config_service_set_display(app->config_service, &settings);
}

static void reach_settings_on_config_service_event(void *, reach_config_service_event event)
{
    if (event == REACH_CONFIG_SERVICE_PERSISTED)
    {
        (void)reach_windows_notify_config_changed();
    }
    else if (event == REACH_CONFIG_SERVICE_PERSIST_FAILED)
    {
        reach_log_error("Could not persist Reach settings.");
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
            else if (work == REACH_SETTINGS_UPDATE_WORK_RESUME &&
                     app->windows_update.wait_for_install != nullptr &&
                     app->windows_update.verify != nullptr)
            {
                std::unique_ptr<reach_windows_update_journal> journal(
                    new (std::nothrow) reach_windows_update_journal());
                if (journal != nullptr &&
                    app->windows_update.wait_for_install(app->windows_update.userdata,
                                                         journal.get()) == REACH_OK)
                {
                    selected_count = journal->count < REACH_WINDOWS_UPDATE_MAX_UPDATES
                                         ? journal->count
                                         : REACH_WINDOWS_UPDATE_MAX_UPDATES;
                    for (size_t index = 0; index < selected_count; ++index)
                    {
                        selected[index] = journal->updates[index];
                    }

                    result = selected_count > 0
                                 ? app->windows_update.verify(app->windows_update.userdata,
                                                              selected.get(), selected_count,
                                                              operation.get())
                                 : REACH_OK;

                    if (app->windows_update.clear_journal != nullptr)
                    {
                        app->windows_update.clear_journal(app->windows_update.userdata);
                    }
                }
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

static int32_t reach_settings_schedule_resume(reach_settings_app *app)
{
    if (app == nullptr || app->windows_update.load_journal == nullptr ||
        app->windows_update.wait_for_install == nullptr || app->windows_update.verify == nullptr)
    {
        return 0;
    }

    std::unique_ptr<reach_windows_update_journal> journal(new (std::nothrow)
                                                              reach_windows_update_journal());
    if (journal == nullptr ||
        app->windows_update.load_journal(app->windows_update.userdata, journal.get()) != REACH_OK ||
        journal->state == REACH_WINDOWS_UPDATE_JOURNAL_IDLE || journal->count == 0 ||
        reach_settings_ensure_worker(app) != REACH_OK)
    {
        return 0;
    }

    reach_settings_model_begin_update_resume(&app->model, journal.get());

    {
        std::lock_guard<std::mutex> lock(app->update_worker.mutex);
        app->update_worker.selected_count = 0;
        app->update_worker.pending_work = REACH_SETTINGS_UPDATE_WORK_RESUME;
        app->update_worker.pending = 1;
    }
    app->update_worker.cv.notify_one();
    app->dirty = 1;
    return 1;
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
        operation->failure_class =
            work == REACH_SETTINGS_UPDATE_WORK_VERIFY || work == REACH_SETTINGS_UPDATE_WORK_RESUME
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
            result = app->app_update.check(app->app_update.userdata, (const uint16_t *)u"aymanervn",
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
    parameters += L"\" 0";
    ShellExecuteW(nullptr, L"open", temp_updater.c_str(), parameters.c_str(), nullptr, SW_HIDE);
    app->running = 0;
}

static void reach_settings_apply_reach_progress(reach_settings_app *app)
{
    uint64_t received = app->reach_worker.received.load();
    uint64_t total = app->reach_worker.total.load();
    if (received != app->model.reach_download_received || total != app->model.reach_download_total)
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

static void reach_settings_startup_worker_main(reach_settings_app *app)
{
    reach_settings_startup_worker *worker = &app->startup_worker;
    for (;;)
    {
        reach_settings_startup_work_type work = REACH_SETTINGS_STARTUP_WORK_NONE;
        reach_startup_app_entry target = {};
        int32_t target_enabled = 0;
        {
            std::unique_lock<std::mutex> lock(worker->mutex);
            worker->cv.wait(lock, [worker] { return worker->stop || worker->pending; });
            if (worker->stop)
            {
                return;
            }
            work = worker->pending_work;
            target = worker->target;
            target_enabled = worker->target_enabled;
            worker->pending = 0;
            worker->in_flight = 1;
        }

        reach_startup_app_list list = {};
        reach_result result = REACH_ERROR;
        if (work == REACH_SETTINGS_STARTUP_WORK_ENUMERATE)
        {
            if (app->startup_apps.ops.enumerate != nullptr)
            {
                result = app->startup_apps.ops.enumerate(app->startup_apps.apps, &list);
            }
        }
        else if (work == REACH_SETTINGS_STARTUP_WORK_SET_ENABLED)
        {
            if (app->startup_apps.ops.set_enabled != nullptr)
            {
                result = app->startup_apps.ops.set_enabled(app->startup_apps.apps, &target,
                                                           target_enabled);
            }
        }

        {
            std::lock_guard<std::mutex> lock(worker->mutex);
            worker->in_flight = 0;
            worker->completed = 1;
            worker->completed_work = work;
            worker->work_result = result;
            worker->list = list;
        }
    }
}

static reach_result reach_settings_ensure_startup_worker(reach_settings_app *app)
{
    if (app->startup_worker.thread_started)
    {
        return REACH_OK;
    }
    app->startup_worker.thread = std::thread(reach_settings_startup_worker_main, app);
    app->startup_worker.thread_started = 1;
    return REACH_OK;
}

static int32_t reach_settings_startup_worker_busy(reach_settings_app *app)
{
    std::lock_guard<std::mutex> lock(app->startup_worker.mutex);
    return app->startup_worker.pending || app->startup_worker.in_flight ||
           app->startup_worker.completed;
}

static void reach_settings_schedule_startup_refresh(reach_settings_app *app)
{
    if (app->startup_apps.ops.enumerate == nullptr || reach_settings_startup_worker_busy(app))
    {
        return;
    }
    if (reach_settings_ensure_startup_worker(app) != REACH_OK)
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(app->startup_worker.mutex);
        app->startup_worker.pending = 1;
        app->startup_worker.pending_work = REACH_SETTINGS_STARTUP_WORK_ENUMERATE;
    }
    reach_settings_model_set_startup_status(&app->model, REACH_SETTINGS_STARTUP_STATUS_LOADING);
    app->startup_worker.cv.notify_one();
    app->dirty = 1;
}

static void reach_settings_schedule_startup_toggle(reach_settings_app *app, size_t index)
{
    if (app->startup_apps.ops.set_enabled == nullptr || index >= app->model.startup_apps.count ||
        app->model.startup_busy || reach_settings_startup_worker_busy(app))
    {
        return;
    }
    if (reach_settings_ensure_startup_worker(app) != REACH_OK)
    {
        return;
    }

    int32_t enabled = reach_settings_model_startup_enabled(&app->model, index) ? 0 : 1;
    {
        std::lock_guard<std::mutex> lock(app->startup_worker.mutex);
        app->startup_worker.pending = 1;
        app->startup_worker.pending_work = REACH_SETTINGS_STARTUP_WORK_SET_ENABLED;
        app->startup_worker.target = app->model.startup_apps.entries[index];
        app->startup_worker.target_enabled = enabled;
        app->startup_worker.target_index = index;
    }
    reach_settings_model_set_startup_enabled(&app->model, index, enabled);
    reach_settings_model_set_startup_busy(&app->model, 1);
    reach_settings_model_set_startup_status(&app->model, REACH_SETTINGS_STARTUP_STATUS_NONE);
    app->startup_worker.cv.notify_one();
    app->dirty = 1;
}

static void reach_settings_release_startup_icons(reach_settings_app *app)
{
    if (app->icon_provider.ops.release != nullptr)
    {
        for (size_t index = 0; index < app->startup_icon_count; ++index)
        {
            if (app->startup_icon_handles[index].id != 0)
            {
                (void)app->icon_provider.ops.release(app->icon_provider.provider,
                                                     app->startup_icon_handles[index]);
            }
        }
    }
    memset(app->startup_icon_handles, 0, sizeof(app->startup_icon_handles));
    app->startup_icon_count = 0;
}

static void reach_settings_load_startup_icons(reach_settings_app *app)
{
    reach_settings_release_startup_icons(app);
    if (app->icon_provider.ops.load == nullptr)
    {
        return;
    }

    app->startup_icon_count = app->model.startup_apps.count;
    for (size_t index = 0; index < app->startup_icon_count; ++index)
    {
        const reach_startup_app_entry *entry = &app->model.startup_apps.entries[index];
        const uint16_t *path = entry->executable[0] != 0 ? entry->executable : entry->command;
        if (path[0] == 0)
        {
            continue;
        }
        reach_icon_request request = {};
        request.size_px = (int32_t)(32.0f * reach_settings_app_scale(app));
        reach_copy_utf16(request.path, 260, path);
        reach_icon_handle handle = {};
        if (app->icon_provider.ops.load(app->icon_provider.provider, &request, &handle) == REACH_OK)
        {
            app->startup_icon_handles[index] = handle;
            reach_settings_model_set_startup_icon(&app->model, index, handle.id);
        }
    }
}

static void reach_settings_apply_startup_result(reach_settings_app *app)
{
    reach_settings_startup_worker *worker = &app->startup_worker;
    reach_settings_startup_work_type work = REACH_SETTINGS_STARTUP_WORK_NONE;
    reach_startup_app_list list = {};
    reach_result result = REACH_ERROR;
    size_t index = 0;
    int32_t enabled = 0;
    {
        std::lock_guard<std::mutex> lock(worker->mutex);
        if (!worker->completed)
        {
            return;
        }
        work = worker->completed_work;
        list = worker->list;
        result = worker->work_result;
        index = worker->target_index;
        enabled = worker->target_enabled;
        worker->completed = 0;
    }

    if (work == REACH_SETTINGS_STARTUP_WORK_ENUMERATE)
    {
        if (result == REACH_OK)
        {
            reach_settings_model_apply_startup_apps(&app->model, &list);
            reach_settings_load_startup_icons(app);
        }
        else
        {
            reach_settings_model_apply_startup_apps(&app->model, nullptr);
            reach_settings_model_set_startup_status(&app->model,
                                                    REACH_SETTINGS_STARTUP_STATUS_FAILED);
        }
    }
    else if (work == REACH_SETTINGS_STARTUP_WORK_SET_ENABLED)
    {
        reach_settings_model_set_startup_busy(&app->model, 0);
        if (result != REACH_OK)
        {
            reach_settings_model_set_startup_enabled(&app->model, index, !enabled);
            reach_settings_model_set_startup_status(&app->model,
                                                    REACH_SETTINGS_STARTUP_STATUS_FAILED);
        }
    }
    app->dirty = 1;
}

static void reach_settings_radio_notify(void *user)
{
    reach_settings_app *app = static_cast<reach_settings_app *>(user);
    if (app != nullptr)
    {
        app->radio_notify.store(1);
    }
}

static void reach_settings_refresh_bluetooth_radio(reach_settings_app *app)
{
    reach_bluetooth_state state = {};
    if (app->system_controls.get_bluetooth_state != nullptr)
    {
        (void)app->system_controls.get_bluetooth_state(app->system_controls.userdata, &state);
    }
    int32_t stop_scan = app->model.bluetooth_scanning ||
                        app->model.bluetooth_status == REACH_SETTINGS_BLUETOOTH_STATUS_SCANNING;
    reach_settings_model_set_bluetooth_radio(&app->model, state);
    if ((!state.available || !state.enabled) && app->bluetooth_service != nullptr && stop_scan)
    {
        reach_bluetooth_service_set_scan_enabled(app->bluetooth_service, 0);
    }
}

static void reach_settings_on_system_controls_changed(void *user, uint32_t change_flags)
{
    reach_settings_app *app = static_cast<reach_settings_app *>(user);
    if (app != nullptr && change_flags != 0)
    {
        app->system_controls_change_flags.fetch_or(change_flags);
    }
}

static void reach_settings_enter_wifi_page(reach_settings_app *app)
{
    if (app->wifi_service == nullptr)
    {
        return;
    }
    reach_bluetooth_service_set_scan_enabled(app->bluetooth_service, 0);
    reach_wifi_service_refresh(app->wifi_service);
    app->dirty = 1;
}

static void reach_settings_enter_bluetooth_page(reach_settings_app *app)
{
    if (app->bluetooth_service == nullptr)
    {
        return;
    }
    reach_settings_refresh_bluetooth_radio(app);
    reach_bluetooth_service_refresh(app->bluetooth_service);
    app->dirty = 1;
}

static void reach_settings_leave_radio_pages(reach_settings_app *app, reach_settings_page page)
{
    if (page != REACH_SETTINGS_PAGE_BLUETOOTH && app->bluetooth_service != nullptr &&
        app->model.bluetooth_scanning)
    {
        reach_bluetooth_service_set_scan_enabled(app->bluetooth_service, 0);
        reach_settings_model_set_bluetooth_status(&app->model,
                                                  REACH_SETTINGS_BLUETOOTH_STATUS_IDLE, nullptr);
    }
    if (page != REACH_SETTINGS_PAGE_WIFI)
    {
        reach_settings_model_wifi_clear_secrets(&app->model);
    }
}

static void reach_settings_apply_wifi_snapshot(reach_settings_app *app)
{
    reach_wifi_snapshot snapshot = {};
    if (app->wifi_service == nullptr || !reach_wifi_service_take(app->wifi_service, &snapshot))
    {
        return;
    }

    reach_settings_model_apply_wifi(&app->model, snapshot.radio, &snapshot.networks);

    switch (snapshot.completed_command)
    {
    case REACH_WIFI_SERVICE_COMMAND_SCAN:
        reach_settings_model_set_wifi_status(
            &app->model,
            snapshot.scan_result == REACH_WIFI_SCAN_RESULT_SUCCEEDED
                ? REACH_SETTINGS_WIFI_STATUS_IDLE
                : REACH_SETTINGS_WIFI_STATUS_SCAN_FAILED,
            nullptr);
        break;
    case REACH_WIFI_SERVICE_COMMAND_CONNECT:
    {
        int32_t status = REACH_SETTINGS_WIFI_STATUS_FAILED;
        if (snapshot.connect_result == REACH_WIFI_CONNECT_RESULT_SUCCEEDED)
        {
            status = REACH_SETTINGS_WIFI_STATUS_CONNECTED;
        }
        else if (snapshot.connect_result == REACH_WIFI_CONNECT_RESULT_INVALID_KEY)
        {
            status = REACH_SETTINGS_WIFI_STATUS_INVALID_KEY;
        }
        else if (snapshot.connect_result == REACH_WIFI_CONNECT_RESULT_NOT_FOUND)
        {
            status = REACH_SETTINGS_WIFI_STATUS_NOT_FOUND;
        }
        reach_settings_model_set_wifi_status(&app->model, status, snapshot.connect_ssid);
        if (status == REACH_SETTINGS_WIFI_STATUS_CONNECTED)
        {
            reach_settings_model_wifi_clear_secrets(&app->model);
            reach_settings_model_wifi_expand_row(&app->model, REACH_SETTINGS_WIFI_ROW_NONE);
        }
        break;
    }
    case REACH_WIFI_SERVICE_COMMAND_FORGET:
        reach_settings_model_set_wifi_status(
            &app->model,
            snapshot.command_succeeded ? REACH_SETTINGS_WIFI_STATUS_IDLE
                                       : REACH_SETTINGS_WIFI_STATUS_FORGET_FAILED,
            nullptr);
        break;
    case REACH_WIFI_SERVICE_COMMAND_DISCONNECT:
    case REACH_WIFI_SERVICE_COMMAND_SET_RADIO:
        reach_settings_model_set_wifi_status(&app->model, REACH_SETTINGS_WIFI_STATUS_IDLE,
                                             nullptr);
        break;
    default:
        break;
    }
    app->dirty = 1;
}

static void reach_settings_release_bluetooth_icons(reach_settings_app *app)
{
    if (app->icon_provider.ops.release != nullptr)
    {
        for (size_t index = 0; index < app->bluetooth_icon_count; ++index)
        {
            if (app->bluetooth_icon_handles[index].id != 0)
            {
                (void)app->icon_provider.ops.release(app->icon_provider.provider,
                                                     app->bluetooth_icon_handles[index]);
            }
        }
    }
    memset(app->bluetooth_icon_handles, 0, sizeof(app->bluetooth_icon_handles));
    app->bluetooth_icon_count = 0;
}

static void reach_settings_load_bluetooth_icons(reach_settings_app *app)
{
    reach_settings_release_bluetooth_icons(app);
    if (app->icon_provider.ops.load == nullptr)
    {
        return;
    }

    app->bluetooth_icon_count = app->model.bluetooth_devices.count;
    for (size_t index = 0; index < app->bluetooth_icon_count; ++index)
    {
        const uint16_t *path = app->model.bluetooth_devices.devices[index].icon_path;
        if (path[0] == 0)
        {
            continue;
        }
        reach_icon_request request = {};
        request.size_px = (int32_t)(32.0f * reach_settings_app_scale(app));
        reach_copy_utf16(request.path, 260, path);
        reach_icon_handle handle = {};
        if (app->icon_provider.ops.load(app->icon_provider.provider, &request, &handle) == REACH_OK)
        {
            app->bluetooth_icon_handles[index] = handle;
            reach_settings_model_set_bluetooth_icon(&app->model, index, handle.id);
        }
    }
}

static void reach_settings_apply_bluetooth_snapshot(reach_settings_app *app)
{
    reach_bluetooth_snapshot snapshot = {};
    if (app->bluetooth_service == nullptr ||
        !reach_bluetooth_service_take(app->bluetooth_service, &snapshot))
    {
        return;
    }

    reach_settings_model_apply_bluetooth(&app->model, &snapshot.devices, &snapshot.pairing,
                                         snapshot.scanning);
    reach_settings_load_bluetooth_icons(app);
    reach_settings_refresh_bluetooth_radio(app);

    if (snapshot.pair_result != REACH_BLUETOOTH_PAIR_RESULT_NONE)
    {
        int32_t status = REACH_SETTINGS_BLUETOOTH_STATUS_FAILED;
        if (snapshot.pair_result == REACH_BLUETOOTH_PAIR_RESULT_SUCCEEDED)
        {
            status = REACH_SETTINGS_BLUETOOTH_STATUS_PAIRED;
        }
        else if (snapshot.pair_result == REACH_BLUETOOTH_PAIR_RESULT_REJECTED)
        {
            status = REACH_SETTINGS_BLUETOOTH_STATUS_REJECTED;
        }
        reach_settings_model_set_bluetooth_status(&app->model, status, snapshot.pair_device_id);
    }
    else if (snapshot.pairing.active)
    {
        reach_settings_model_set_bluetooth_status(
            &app->model, REACH_SETTINGS_BLUETOOTH_STATUS_CONFIRM_PIN, snapshot.pairing.device_id);
    }
    else if (snapshot.completed_command == REACH_BLUETOOTH_SERVICE_COMMAND_SET_SCAN)
    {
        reach_settings_model_set_bluetooth_status(
            &app->model,
            snapshot.scanning && app->model.bluetooth_radio.available &&
                    app->model.bluetooth_radio.enabled
                ? REACH_SETTINGS_BLUETOOTH_STATUS_SCANNING
                : REACH_SETTINGS_BLUETOOTH_STATUS_IDLE,
            nullptr);
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
            reach_settings_model_account_apply_status(&app->model,
                                                      REACH_SETTINGS_ACCOUNT_STATUS_WRONG_CURRENT);
            return;
        }
    }
    if (!reach_settings_model_account_submit_ready(&app->model))
    {
        return;
    }
    if (app->user_account.ops.change_password == nullptr)
    {
        reach_settings_model_account_apply_status(&app->model, REACH_SETTINGS_ACCOUNT_STATUS_ERROR);
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

static int32_t reach_settings_hit_is_button(reach_settings_hit_type type)
{
    return type == REACH_SETTINGS_HIT_UPDATE_REFRESH || type == REACH_SETTINGS_HIT_UPDATE_INSTALL ||
           type == REACH_SETTINGS_HIT_UPDATE_RESTART || type == REACH_SETTINGS_HIT_REACH_UPDATE ||
           type == REACH_SETTINGS_HIT_POWER_APPLY || type == REACH_SETTINGS_HIT_ACCOUNT_PASSWORD ||
           type == REACH_SETTINGS_HIT_WIFI_SCAN || type == REACH_SETTINGS_HIT_WIFI_ADD ||
           type == REACH_SETTINGS_HIT_WIFI_KNOWN || type == REACH_SETTINGS_HIT_WIFI_BACK ||
           type == REACH_SETTINGS_HIT_WIFI_SHOW_KEY || type == REACH_SETTINGS_HIT_WIFI_CONNECT ||
           type == REACH_SETTINGS_HIT_WIFI_DISCONNECT || type == REACH_SETTINGS_HIT_WIFI_FORGET ||
           type == REACH_SETTINGS_HIT_WIFI_ADD_SHOW_KEY ||
           type == REACH_SETTINGS_HIT_WIFI_ADD_SUBMIT ||
           type == REACH_SETTINGS_HIT_BLUETOOTH_SCAN ||
           type == REACH_SETTINGS_HIT_BLUETOOTH_ACTION ||
           type == REACH_SETTINGS_HIT_BLUETOOTH_PIN_ACCEPT ||
           type == REACH_SETTINGS_HIT_BLUETOOTH_PIN_REJECT;
}

static int32_t reach_settings_hit_is_window_button(reach_settings_hit_type type)
{
    return type == REACH_SETTINGS_HIT_CLOSE || type == REACH_SETTINGS_HIT_MINIMIZE;
}

static uint64_t reach_settings_pressable_target(reach_settings_hit_result hit)
{
    uint32_t detail = 0;
    switch (hit.type)
    {
    case REACH_SETTINGS_HIT_NAV_ITEM:
        detail = (uint32_t)hit.page;
        break;
    case REACH_SETTINGS_HIT_UPDATE_CHECKBOX:
        detail = (uint32_t)hit.update_index;
        break;
    case REACH_SETTINGS_HIT_POWER_OPTION:
        detail = ((uint32_t)hit.power_timer << 16) | ((uint32_t)hit.power_option << 8) |
                 (uint32_t)hit.power_custom_field;
        break;
    case REACH_SETTINGS_HIT_POWER_WAIT_TOGGLE:
        detail = (uint32_t)hit.power_timer;
        break;
    case REACH_SETTINGS_HIT_ACCOUNT_PASSWORD_FIELD:
        detail = (uint32_t)hit.account_field;
        break;
    case REACH_SETTINGS_HIT_STARTUP_TOGGLE:
        detail = (uint32_t)hit.startup_index;
        break;
    case REACH_SETTINGS_HIT_DISPLAY_WINDOWS_SYSTEM_THEME:
    case REACH_SETTINGS_HIT_DISPLAY_WINDOWS_APP_THEME:
        detail = (uint32_t)hit.display_theme_preference;
        break;
    case REACH_SETTINGS_HIT_WIFI_ROW:
        detail = (uint32_t)hit.wifi_index;
        break;
    case REACH_SETTINGS_HIT_WIFI_ADD_SECURITY:
        detail = (uint32_t)hit.wifi_security_option;
        break;
    case REACH_SETTINGS_HIT_BLUETOOTH_ROW:
        detail = (uint32_t)hit.bluetooth_index;
        break;
    case REACH_SETTINGS_HIT_NONE:
    case REACH_SETTINGS_HIT_UPDATE_SCROLLBAR_TRACK:
    case REACH_SETTINGS_HIT_UPDATE_SCROLLBAR_THUMB:
    case REACH_SETTINGS_HIT_STARTUP_SCROLLBAR_TRACK:
    case REACH_SETTINGS_HIT_STARTUP_SCROLLBAR_THUMB:
    case REACH_SETTINGS_HIT_WIFI_SCROLLBAR_TRACK:
    case REACH_SETTINGS_HIT_WIFI_SCROLLBAR_THUMB:
    case REACH_SETTINGS_HIT_BLUETOOTH_SCROLLBAR_TRACK:
    case REACH_SETTINGS_HIT_BLUETOOTH_SCROLLBAR_THUMB:
        return REACH_PRESSABLE_TARGET_NONE;
    default:
        break;
    }
    return ((uint64_t)hit.type << 32) | detail;
}

static reach_pressable_feedback_style reach_settings_pressable_feedback(reach_settings_app *app)
{
    reach_pressable_feedback_style feedback = {};
    feedback.animations = app != nullptr ? &app->model.button_press_animation : nullptr;
    feedback.track = 0;
    feedback.pressed_value = 1.0f;
    feedback.release_seconds = 0.18;
    feedback.release_easing = REACH_EASING_EASE_OUT;
    return feedback;
}

static void reach_settings_apply_pressable_result(reach_settings_app *app,
                                                  const reach_pressable_result *result)
{
    if (app == nullptr || result == nullptr)
    {
        return;
    }
    app->dirty |= result->redraw;
    if (result->capture != 0 && app->window.ops.set_pointer_capture != nullptr)
    {
        (void)app->window.ops.set_pointer_capture(app->window.window, result->capture > 0);
    }
}

static void reach_settings_handle_wifi_action(reach_settings_app *app,
                                              reach_settings_hit_result hit)
{
    reach_settings_model *model = &app->model;
    switch (hit.type)
    {
    case REACH_SETTINGS_HIT_WIFI_RADIO_TOGGLE:
        if (model->wifi_radio != REACH_WIFI_RADIO_UNAVAILABLE)
        {
            int32_t enable = model->wifi_radio == REACH_WIFI_RADIO_ON ? 0 : 1;
            (void)reach_settings_model_toggle_wifi_radio(model);
            reach_wifi_service_set_radio_enabled(app->wifi_service,
                                                 enable);
        }
        break;
    case REACH_SETTINGS_HIT_WIFI_SCAN:
        if (model->wifi_radio == REACH_WIFI_RADIO_ON &&
            !reach_settings_model_wifi_busy(model))
        {
            reach_settings_model_set_wifi_status(model, REACH_SETTINGS_WIFI_STATUS_SCANNING,
                                                 nullptr);
            reach_wifi_service_scan(app->wifi_service);
        }
        break;
    case REACH_SETTINGS_HIT_WIFI_ADD:
        reach_settings_model_wifi_expand_row(model, REACH_SETTINGS_WIFI_ROW_ADD);
        break;
    case REACH_SETTINGS_HIT_WIFI_KNOWN:
        reach_settings_model_set_wifi_view(model, REACH_SETTINGS_WIFI_VIEW_KNOWN);
        break;
    case REACH_SETTINGS_HIT_WIFI_BACK:
        reach_settings_model_set_wifi_view(model, REACH_SETTINGS_WIFI_VIEW_AVAILABLE);
        break;
    case REACH_SETTINGS_HIT_WIFI_ROW:
        reach_settings_model_wifi_expand_row(model, (int32_t)hit.wifi_index);
        break;
    case REACH_SETTINGS_HIT_WIFI_KEY_FIELD:
        reach_settings_model_wifi_focus_field(model, REACH_SETTINGS_WIFI_FIELD_KEY);
        break;
    case REACH_SETTINGS_HIT_WIFI_ADD_NAME_FIELD:
        reach_settings_model_wifi_focus_field(model, REACH_SETTINGS_WIFI_FIELD_ADD_NAME);
        break;
    case REACH_SETTINGS_HIT_WIFI_ADD_KEY_FIELD:
        reach_settings_model_wifi_focus_field(model, REACH_SETTINGS_WIFI_FIELD_ADD_KEY);
        break;
    case REACH_SETTINGS_HIT_WIFI_SHOW_KEY:
    case REACH_SETTINGS_HIT_WIFI_ADD_SHOW_KEY:
        (void)reach_settings_model_wifi_toggle_show_key(model);
        break;
    case REACH_SETTINGS_HIT_WIFI_AUTO_TOGGLE:
    case REACH_SETTINGS_HIT_WIFI_ADD_AUTO_TOGGLE:
        (void)reach_settings_model_wifi_toggle_auto(model);
        break;
    case REACH_SETTINGS_HIT_WIFI_ADD_SECURITY:
        reach_settings_model_wifi_select_security(model, hit.wifi_security_option);
        break;
    case REACH_SETTINGS_HIT_WIFI_CONNECT:
    {
        reach_wifi_connect_request request = {};
        if (!reach_settings_model_wifi_busy(model) &&
            reach_settings_model_wifi_connect_ready(model) &&
            reach_settings_model_wifi_build_connect(model, (size_t)model->wifi_expanded_row,
                                                    &request))
        {
            reach_settings_model_wifi_blur(model);
            reach_settings_model_set_wifi_status(model, REACH_SETTINGS_WIFI_STATUS_CONNECTING,
                                                 request.ssid);
            reach_wifi_service_connect(app->wifi_service, &request);
        }
        break;
    }
    case REACH_SETTINGS_HIT_WIFI_ADD_SUBMIT:
    {
        reach_wifi_connect_request request = {};
        if (reach_settings_model_wifi_build_add(model, &request))
        {
            reach_settings_model_wifi_blur(model);
            reach_settings_model_set_wifi_status(model, REACH_SETTINGS_WIFI_STATUS_CONNECTING,
                                                 request.ssid);
            reach_wifi_service_connect(app->wifi_service, &request);
        }
        break;
    }
    case REACH_SETTINGS_HIT_WIFI_DISCONNECT:
        reach_wifi_service_disconnect(app->wifi_service);
        break;
    case REACH_SETTINGS_HIT_WIFI_FORGET:
        if (model->wifi_expanded_row >= 0 &&
            (size_t)model->wifi_expanded_row < model->wifi_networks.count)
        {
            reach_wifi_service_forget(
                app->wifi_service, model->wifi_networks.networks[model->wifi_expanded_row].ssid);
            reach_settings_model_wifi_expand_row(model, REACH_SETTINGS_WIFI_ROW_NONE);
        }
        break;
    default:
        reach_settings_model_wifi_blur(model);
        break;
    }
    app->dirty = 1;
}

static void reach_settings_handle_bluetooth_action(reach_settings_app *app,
                                                   reach_settings_hit_result hit)
{
    reach_settings_model *model = &app->model;
    switch (hit.type)
    {
    case REACH_SETTINGS_HIT_BLUETOOTH_RADIO_TOGGLE:
        if (model->bluetooth_radio.available &&
            app->system_controls.request_bluetooth_enabled != nullptr)
        {
            int32_t enabled = model->bluetooth_radio.enabled ? 0 : 1;
            if (app->system_controls.request_bluetooth_enabled(
                    app->system_controls.userdata, enabled) == REACH_OK)
            {
                (void)reach_settings_model_toggle_bluetooth_radio(model);
                if (!enabled)
                {
                    reach_bluetooth_service_set_scan_enabled(app->bluetooth_service, 0);
                }
            }
        }
        break;
    case REACH_SETTINGS_HIT_BLUETOOTH_SCAN:
    {
        if (!model->bluetooth_radio.available || !model->bluetooth_radio.enabled)
        {
            break;
        }
        int32_t enable = model->bluetooth_scanning ? 0 : 1;
        reach_settings_model_set_bluetooth_status(
            model,
            enable ? REACH_SETTINGS_BLUETOOTH_STATUS_SCANNING
                   : REACH_SETTINGS_BLUETOOTH_STATUS_IDLE,
            nullptr);
        reach_bluetooth_service_set_scan_enabled(app->bluetooth_service, enable);
        break;
    }
    case REACH_SETTINGS_HIT_BLUETOOTH_ROW:
        reach_settings_model_bluetooth_expand_row(model, (int32_t)hit.bluetooth_index);
        break;
    case REACH_SETTINGS_HIT_BLUETOOTH_ACTION:
        if (model->bluetooth_expanded_row >= 0 &&
            (size_t)model->bluetooth_expanded_row < model->bluetooth_devices.count)
        {
            const reach_bluetooth_device *device =
                &model->bluetooth_devices.devices[model->bluetooth_expanded_row];
            if (device->paired)
            {
                reach_bluetooth_service_unpair(app->bluetooth_service, device->id);
            }
            else
            {
                reach_settings_model_set_bluetooth_status(
                    model, REACH_SETTINGS_BLUETOOTH_STATUS_PAIRING, device->id);
                reach_bluetooth_service_pair(app->bluetooth_service, device->id);
            }
        }
        break;
    case REACH_SETTINGS_HIT_BLUETOOTH_PIN_ACCEPT:
        reach_settings_model_set_bluetooth_status(model, REACH_SETTINGS_BLUETOOTH_STATUS_PAIRING,
                                                  nullptr);
        reach_bluetooth_service_respond_pairing(app->bluetooth_service, 1);
        break;
    case REACH_SETTINGS_HIT_BLUETOOTH_PIN_REJECT:
        reach_bluetooth_service_respond_pairing(app->bluetooth_service, 0);
        break;
    default:
        break;
    }
    app->dirty = 1;
}

static void reach_settings_handle_pointer_up(reach_settings_app *app, const reach_ui_event *event)
{
    if (app->update_scrollbar_drag.active || app->startup_scrollbar_drag.active ||
        app->wifi_scrollbar_drag.active || app->bluetooth_scrollbar_drag.active)
    {
        reach_scrollbar_end_drag(&app->update_scrollbar_drag);
        reach_scrollbar_end_drag(&app->startup_scrollbar_drag);
        reach_scrollbar_end_drag(&app->wifi_scrollbar_drag);
        reach_scrollbar_end_drag(&app->bluetooth_scrollbar_drag);
        if (app->window.ops.set_pointer_capture != nullptr)
        {
            (void)app->window.ops.set_pointer_capture(app->window.window, 0);
        }
        return;
    }
    reach_settings_refresh_bounds(app);
    reach_settings_refresh_layout(app);
    float x = (float)event->x - app->bounds.x;
    float y = (float)event->y - app->bounds.y;
    reach_settings_hit_result hit = reach_settings_hit_test(&app->layout, x, y);
    reach_pressable_feedback_style feedback = reach_settings_pressable_feedback(app);
    reach_pressable_result pressable_result = {};
    reach_pressable_release(&app->model.button_pressable, REACH_POINTER_BUTTON_PRIMARY,
                            reach_settings_pressable_target(hit), &feedback, &pressable_result);
    reach_settings_apply_pressable_result(app, &pressable_result);
    if (!pressable_result.activated)
    {
        if (app->model.selected_page == REACH_SETTINGS_PAGE_ACCOUNT)
        {
            reach_settings_model_account_blur(&app->model);
            app->dirty = 1;
        }
        else if (app->model.selected_page == REACH_SETTINGS_PAGE_POWER_SLEEP)
        {
            reach_settings_model_power_blur(&app->model);
            app->dirty = 1;
        }
        else if (app->model.selected_page == REACH_SETTINGS_PAGE_WIFI)
        {
            reach_settings_model_wifi_blur(&app->model);
            app->dirty = 1;
        }
        return;
    }
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
        reach_settings_leave_radio_pages(app, hit.page);
        reach_settings_model_select_page(&app->model, hit.page);
        if (hit.page == REACH_SETTINGS_PAGE_STARTUP_APPS && !app->model.startup_loaded)
        {
            reach_settings_schedule_startup_refresh(app);
        }
        else if (hit.page == REACH_SETTINGS_PAGE_WIFI)
        {
            reach_settings_enter_wifi_page(app);
        }
        else if (hit.page == REACH_SETTINGS_PAGE_BLUETOOTH)
        {
            reach_settings_enter_bluetooth_page(app);
        }
        app->dirty = 1;
    }
    else if (app->model.selected_page == REACH_SETTINGS_PAGE_WIFI)
    {
        reach_settings_handle_wifi_action(app, hit);
    }
    else if (app->model.selected_page == REACH_SETTINGS_PAGE_BLUETOOTH)
    {
        reach_settings_handle_bluetooth_action(app, hit);
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
    else if (app->model.selected_page == REACH_SETTINGS_PAGE_STARTUP_APPS)
    {
        if (hit.type == REACH_SETTINGS_HIT_STARTUP_TOGGLE)
        {
            reach_settings_schedule_startup_toggle(app, hit.startup_index);
        }
    }
    else if (app->model.selected_page == REACH_SETTINGS_PAGE_DISPLAY)
    {
        if (hit.type == REACH_SETTINGS_HIT_DISPLAY_FPS_TOGGLE)
        {
            (void)reach_settings_model_toggle_high_refresh_rate(&app->model);
            reach_settings_save_display_config(app);
            app->dirty = 1;
        }
        else if (hit.type == REACH_SETTINGS_HIT_DISPLAY_FONT_TOGGLE)
        {
            (void)reach_settings_model_toggle_bundled_font(&app->model);
            reach_settings_save_display_config(app);
            reach_settings_apply_ui_font(app);
        }
        else if (hit.type == REACH_SETTINGS_HIT_DISPLAY_THEME_TOGGLE)
        {
            (void)reach_settings_model_toggle_light_theme(&app->model);
            reach_settings_save_display_config(app);
            reach_settings_apply_theme(app);
        }
        else if (hit.type == REACH_SETTINGS_HIT_DISPLAY_WINDOWS_SYSTEM_THEME)
        {
            if (reach_settings_model_select_windows_system_theme(&app->model,
                                                                 hit.display_theme_preference))
            {
                reach_settings_save_display_config(app);
                app->dirty = 1;
            }
        }
        else if (hit.type == REACH_SETTINGS_HIT_DISPLAY_WINDOWS_APP_THEME)
        {
            if (reach_settings_model_select_windows_app_theme(&app->model,
                                                              hit.display_theme_preference))
            {
                reach_settings_save_display_config(app);
                app->dirty = 1;
            }
        }
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
    int32_t wifi_focused = app->model.selected_page == REACH_SETTINGS_PAGE_WIFI &&
                           app->model.wifi_focused_field != REACH_SETTINGS_WIFI_FIELD_NONE;
    if (!power_focused && !account_focused && !wifi_focused)
    {
        return;
    }
    if (wifi_focused)
    {
        int32_t handled = 0;
        if (event->type == REACH_UI_EVENT_TEXT_CHAR)
        {
            handled = reach_settings_model_wifi_insert_char(&app->model, (uint16_t)event->id);
        }
        else
        {
            reach_text_edit_modifiers modifiers = {};
            modifiers.shift = (event->modifiers & REACH_UI_EVENT_MODIFIER_SHIFT) ? 1 : 0;
            modifiers.ctrl = (event->modifiers & REACH_UI_EVENT_MODIFIER_CTRL) ? 1 : 0;
            int32_t select_all = 0;
            reach_text_edit_key key = reach_settings_map_edit_key(event, &select_all);
            if (key != REACH_TEXT_EDIT_KEY_NONE)
            {
                handled =
                    reach_settings_model_wifi_handle_edit_key(&app->model, key, modifiers);
            }
        }
        if (handled)
        {
            app->dirty = 1;
        }
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
            handled =
                power_focused
                    ? reach_settings_model_power_handle_edit_key(&app->model, key, modifiers)
                    : reach_settings_model_account_handle_edit_key(&app->model, key, modifiers);
        }
    }
    if (handled)
    {
        app->dirty = 1;
    }
}

static void reach_settings_handle_pointer_down(reach_settings_app *app, const reach_ui_event *event)
{
    if (app == nullptr || event == nullptr)
    {
        return;
    }
    reach_settings_refresh_bounds(app);
    reach_settings_refresh_layout(app);
    float x = (float)event->x - app->bounds.x;
    float y = (float)event->y - app->bounds.y;
    reach_settings_hit_result hit = reach_settings_hit_test(&app->layout, x, y);
    uint64_t target = reach_settings_pressable_target(hit);
    if (target != REACH_PRESSABLE_TARGET_NONE)
    {
        reach_pressable_feedback_style feedback = reach_settings_pressable_feedback(app);
        reach_pressable_result result = {};
        size_t feedback_index = reach_settings_hit_is_button(hit.type)
                                    ? (size_t)hit.type
                                    : REACH_PRESSABLE_FEEDBACK_NONE;
        reach_pressable_press(&app->model.button_pressable, REACH_POINTER_BUTTON_PRIMARY, target,
                              feedback_index, &feedback, &result);
        reach_settings_apply_pressable_result(app, &result);
    }
    if (hit.type == REACH_SETTINGS_HIT_UPDATE_SCROLLBAR_TRACK ||
        hit.type == REACH_SETTINGS_HIT_UPDATE_SCROLLBAR_THUMB)
    {
        reach_scrollbar_layout layout = {app->layout.update_scrollbar_track,
                                         app->layout.update_scrollbar_thumb};
        reach_scrollbar_begin_drag(&app->model.update_scrollbar, &app->update_scrollbar_drag,
                                   &layout, y,
                                   hit.type == REACH_SETTINGS_HIT_UPDATE_SCROLLBAR_THUMB);
    }
    else if (hit.type == REACH_SETTINGS_HIT_STARTUP_SCROLLBAR_TRACK ||
             hit.type == REACH_SETTINGS_HIT_STARTUP_SCROLLBAR_THUMB)
    {
        reach_scrollbar_layout layout = {app->layout.startup_scrollbar_track,
                                         app->layout.startup_scrollbar_thumb};
        reach_scrollbar_begin_drag(&app->model.startup_scrollbar, &app->startup_scrollbar_drag,
                                   &layout, y,
                                   hit.type == REACH_SETTINGS_HIT_STARTUP_SCROLLBAR_THUMB);
    }
    else if (hit.type == REACH_SETTINGS_HIT_WIFI_SCROLLBAR_TRACK ||
             hit.type == REACH_SETTINGS_HIT_WIFI_SCROLLBAR_THUMB)
    {
        reach_scrollbar_layout layout = {app->layout.wifi_scrollbar_track,
                                         app->layout.wifi_scrollbar_thumb};
        reach_scrollbar_begin_drag(&app->model.wifi_scrollbar, &app->wifi_scrollbar_drag, &layout,
                                   y, hit.type == REACH_SETTINGS_HIT_WIFI_SCROLLBAR_THUMB);
    }
    else if (hit.type == REACH_SETTINGS_HIT_BLUETOOTH_SCROLLBAR_TRACK ||
             hit.type == REACH_SETTINGS_HIT_BLUETOOTH_SCROLLBAR_THUMB)
    {
        reach_scrollbar_layout layout = {app->layout.bluetooth_scrollbar_track,
                                         app->layout.bluetooth_scrollbar_thumb};
        reach_scrollbar_begin_drag(&app->model.bluetooth_scrollbar, &app->bluetooth_scrollbar_drag,
                                   &layout, y,
                                   hit.type == REACH_SETTINGS_HIT_BLUETOOTH_SCROLLBAR_THUMB);
    }
    else
    {
        return;
    }
    if (app->window.ops.set_pointer_capture != nullptr)
    {
        (void)app->window.ops.set_pointer_capture(app->window.window, 1);
    }
    app->dirty = 1;
}

static void reach_settings_handle_pointer_move(reach_settings_app *app, const reach_ui_event *event)
{
    if (app == nullptr || event == nullptr)
    {
        return;
    }
    reach_settings_refresh_bounds(app);
    reach_settings_refresh_layout(app);

    reach_settings_hit_result hover = reach_settings_hit_test(
        &app->layout, (float)event->x - app->bounds.x, (float)event->y - app->bounds.y);
    reach_pressable_result result = {};
    reach_pressable_update(&app->model.button_pressable, reach_settings_pressable_target(hover),
                           &result);
    reach_settings_apply_pressable_result(app, &result);
    int32_t hovered = reach_settings_hit_is_window_button(hover.type) ? (int32_t)hover.type
                                                                      : REACH_SETTINGS_HIT_NONE;
    if (reach_settings_model_set_hovered_button(&app->model, hovered))
    {
        app->dirty = 1;
    }

    if (!app->update_scrollbar_drag.active && !app->startup_scrollbar_drag.active &&
        !app->wifi_scrollbar_drag.active && !app->bluetooth_scrollbar_drag.active)
    {
        return;
    }
    reach_settings_refresh_layout(app);
    float y = (float)event->y - app->bounds.y;
    if (app->update_scrollbar_drag.active)
    {
        reach_scrollbar_layout layout = {app->layout.update_scrollbar_track,
                                         app->layout.update_scrollbar_thumb};
        reach_scrollbar_update_drag(&app->model.update_scrollbar, &app->update_scrollbar_drag,
                                    &layout, y);
    }
    else if (app->wifi_scrollbar_drag.active)
    {
        reach_scrollbar_layout layout = {app->layout.wifi_scrollbar_track,
                                         app->layout.wifi_scrollbar_thumb};
        reach_scrollbar_update_drag(&app->model.wifi_scrollbar, &app->wifi_scrollbar_drag, &layout,
                                    y);
    }
    else if (app->bluetooth_scrollbar_drag.active)
    {
        reach_scrollbar_layout layout = {app->layout.bluetooth_scrollbar_track,
                                         app->layout.bluetooth_scrollbar_thumb};
        reach_scrollbar_update_drag(&app->model.bluetooth_scrollbar,
                                    &app->bluetooth_scrollbar_drag, &layout, y);
    }
    else
    {
        reach_scrollbar_layout layout = {app->layout.startup_scrollbar_track,
                                         app->layout.startup_scrollbar_thumb};
        reach_scrollbar_update_drag(&app->model.startup_scrollbar, &app->startup_scrollbar_drag,
                                    &layout, y);
    }
    app->dirty = 1;
}

static void reach_settings_handle_event(void *user, const reach_ui_event *event)
{
    reach_settings_app *app = static_cast<reach_settings_app *>(user);
    if (app == nullptr || event == nullptr)
    {
        return;
    }
    else if (event->type == REACH_UI_EVENT_POINTER_DOWN &&
             event->button == REACH_POINTER_BUTTON_PRIMARY)
    {
        reach_settings_handle_pointer_down(app, event);
    }
    else if (event->type == REACH_UI_EVENT_POINTER_MOVE)
    {
        reach_settings_handle_pointer_move(app, event);
    }
    else if (event->type == REACH_UI_EVENT_POINTER_UP &&
             event->button == REACH_POINTER_BUTTON_PRIMARY)
    {
        reach_settings_handle_pointer_up(app, event);
    }
    else if (event->type == REACH_UI_EVENT_POINTER_LEAVE)
    {
        reach_pressable_result result = {};
        reach_pressable_update(&app->model.button_pressable, REACH_PRESSABLE_TARGET_NONE, &result);
        reach_settings_apply_pressable_result(app, &result);
        if (reach_settings_model_set_hovered_button(&app->model, REACH_SETTINGS_HIT_NONE))
        {
            app->dirty = 1;
        }
    }
    else if (event->type == REACH_UI_EVENT_POINTER_CANCEL)
    {
        reach_pressable_feedback_style feedback = reach_settings_pressable_feedback(app);
        reach_pressable_result result = {};
        reach_pressable_cancel(&app->model.button_pressable, &feedback, &result);
        reach_settings_apply_pressable_result(app, &result);
        reach_scrollbar_end_drag(&app->update_scrollbar_drag);
        reach_scrollbar_end_drag(&app->startup_scrollbar_drag);
        reach_scrollbar_end_drag(&app->wifi_scrollbar_drag);
        reach_scrollbar_end_drag(&app->bluetooth_scrollbar_drag);
        if (reach_settings_model_set_hovered_button(&app->model, REACH_SETTINGS_HIT_NONE))
        {
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
    else if (event->type == REACH_UI_EVENT_POINTER_WHEEL &&
             app->model.selected_page == REACH_SETTINGS_PAGE_STARTUP_APPS &&
             event->wheel_delta != 0)
    {
        reach_settings_model_scroll_startup(
            &app->model, event->wheel_delta > 0 ? -86.0f * reach_settings_app_scale(app)
                                                : 86.0f * reach_settings_app_scale(app));
        app->dirty = 1;
    }
    else if (event->type == REACH_UI_EVENT_POINTER_WHEEL &&
             app->model.selected_page == REACH_SETTINGS_PAGE_WIFI && event->wheel_delta != 0)
    {
        reach_settings_model_scroll_wifi(&app->model,
                                         event->wheel_delta > 0
                                             ? -86.0f * reach_settings_app_scale(app)
                                             : 86.0f * reach_settings_app_scale(app));
        app->dirty = 1;
    }
    else if (event->type == REACH_UI_EVENT_POINTER_WHEEL &&
             app->model.selected_page == REACH_SETTINGS_PAGE_BLUETOOTH && event->wheel_delta != 0)
    {
        reach_settings_model_scroll_bluetooth(&app->model,
                                              event->wheel_delta > 0
                                                  ? -86.0f * reach_settings_app_scale(app)
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
        if (reach_settings_render(app) == REACH_OK)
        {
            app->dirty = 0;
        }
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
    (void)reach_windows_create_startup_apps(&app->startup_apps);
    (void)reach_windows_create_icon_provider(&app->icon_provider);
    (void)reach_windows_create_system_controls(&app->system_controls);

    reach_wifi_port wifi_port = {};
    if (reach_windows_create_wifi(&wifi_port) == REACH_OK)
    {
        (void)reach_wifi_service_create(wifi_port, reach_settings_radio_notify, app,
                                        &app->wifi_service);
    }
    reach_bluetooth_port bluetooth_port = {};
    if (reach_windows_create_bluetooth(&bluetooth_port) == REACH_OK)
    {
        (void)reach_bluetooth_service_create(bluetooth_port, reach_settings_radio_notify, app,
                                             &app->bluetooth_service);
    }

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
        reach_windows_create_config_store(config_path, &app->config_store) == REACH_OK &&
        reach_config_service_create(app->config_store, reach_settings_on_config_service_event, app,
                                    &app->config_service) == REACH_OK)
    {
        reach_settings_load_power_config(app);
        reach_settings_load_display_config(app);
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
    if (app->system_controls.start_watching != nullptr)
    {
        (void)app->system_controls.start_watching(app->system_controls.userdata,
                                                  reach_settings_on_system_controls_changed, app);
    }
    if (!reach_settings_schedule_resume(app))
    {
        reach_settings_schedule_verification(app);
    }
    reach_settings_schedule_reach_check(app);
    if (app->model.selected_page == REACH_SETTINGS_PAGE_WIFI)
    {
        reach_settings_enter_wifi_page(app);
    }
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
    reach_settings_apply_startup_result(app);
    uint32_t system_changes = app->system_controls_change_flags.exchange(0);
    if ((system_changes & REACH_SYSTEM_CONTROLS_CHANGE_BLUETOOTH) != 0)
    {
        reach_settings_refresh_bluetooth_radio(app);
        app->dirty = 1;
    }
    if (app->radio_notify.exchange(0) != 0)
    {
        app->dirty = 1;
    }
    reach_settings_apply_wifi_snapshot(app);
    reach_settings_apply_bluetooth_snapshot(app);
    if (reach_settings_model_wifi_scroll(&app->model, delta_seconds))
    {
        app->dirty = 1;
    }
    if (reach_settings_model_bluetooth_scroll(&app->model, delta_seconds))
    {
        app->dirty = 1;
    }
    if (reach_settings_model_wifi_loader(&app->model, delta_seconds))
    {
        app->dirty = 1;
    }
    if (reach_settings_model_bluetooth_loader(&app->model, delta_seconds))
    {
        app->dirty = 1;
    }
    if (reach_settings_model_tick_wifi_animations(&app->model, delta_seconds))
    {
        app->dirty = 1;
    }
    if (reach_settings_model_tick_bluetooth_animations(&app->model, delta_seconds))
    {
        app->dirty = 1;
    }
    if (reach_settings_model_tick_wifi_caret(&app->model, delta_seconds))
    {
        app->dirty = 1;
    }
    if (reach_settings_model_update_scroll(&app->model, delta_seconds))
    {
        app->dirty = 1;
    }
    if (reach_settings_model_update_loader(&app->model, delta_seconds))
    {
        app->dirty = 1;
    }
    if (reach_settings_model_startup_scroll(&app->model, delta_seconds))
    {
        app->dirty = 1;
    }
    if (reach_settings_model_tick_startup_animations(&app->model, delta_seconds))
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
    if (reach_settings_model_tick_nav_selection(&app->model, delta_seconds))
    {
        app->dirty = 1;
    }
    if (reach_settings_model_tick_display_animations(&app->model, delta_seconds))
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
    reach_settings_startup_worker *startup_worker =
        const_cast<reach_settings_startup_worker *>(&app->startup_worker);
    int32_t startup_busy = 0;
    {
        std::lock_guard<std::mutex> startup_lock(startup_worker->mutex);
        startup_busy =
            startup_worker->pending || startup_worker->in_flight || startup_worker->completed;
    }
    reach_settings_update_worker *worker =
        const_cast<reach_settings_update_worker *>(&app->update_worker);
    std::lock_guard<std::mutex> lock(worker->mutex);
    return app->dirty || reach_busy || startup_busy || worker->pending || worker->in_flight ||
           worker->completed || app->update_worker.progress_state.load() != 0 ||
           app->model.update_scrollbar.offset != app->model.update_scrollbar.target ||
           app->model.startup_scrollbar.offset != app->model.startup_scrollbar.target ||
           app->update_scrollbar_drag.active || app->startup_scrollbar_drag.active ||
           reach_settings_model_startup_animations_active(&app->model) ||
           reach_settings_model_power_animations_active(&app->model) ||
           reach_settings_model_button_press_active(&app->model) ||
           reach_settings_model_nav_selection_active(&app->model) ||
           reach_settings_model_display_animations_active(&app->model) ||
           app->model.power_focused_timer >= 0 || app->model.account_focused_field >= 0 ||
           app->model.wifi_focused_field != REACH_SETTINGS_WIFI_FIELD_NONE ||
           app->radio_notify.load() != 0 || app->system_controls_change_flags.load() != 0 ||
           reach_wifi_service_pending(app->wifi_service) ||
           reach_bluetooth_service_pending(app->bluetooth_service) ||
           app->model.wifi_scrollbar.offset != app->model.wifi_scrollbar.target ||
           app->model.bluetooth_scrollbar.offset != app->model.bluetooth_scrollbar.target ||
           app->wifi_scrollbar_drag.active || app->bluetooth_scrollbar_drag.active ||
           reach_settings_model_wifi_animations_active(&app->model) ||
           reach_settings_model_bluetooth_animations_active(&app->model) ||
           app->model.wifi_status == REACH_SETTINGS_WIFI_STATUS_SCANNING ||
           app->model.bluetooth_status == REACH_SETTINGS_BLUETOOTH_STATUS_SCANNING;
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
    }
    if (app->startup_worker.thread_started)
    {
        {
            std::lock_guard<std::mutex> lock(app->startup_worker.mutex);
            app->startup_worker.stop = 1;
            app->startup_worker.pending = 0;
        }
        app->startup_worker.cv.notify_one();
    }

    if (app->renderer.ops.destroy != nullptr)
    {
        app->renderer.ops.destroy(app->renderer.backend);
        app->renderer.ops.destroy = nullptr;
    }
    if (app->window.ops.destroy != nullptr)
    {
        app->window.ops.destroy(app->window.window);
        app->window.ops.destroy = nullptr;
    }

    if (app->update_worker.thread_started)
    {
        app->update_worker.thread.join();
    }
    if (app->reach_worker.thread_started)
    {
        app->reach_worker.thread.join();
    }
    if (app->startup_worker.thread_started)
    {
        app->startup_worker.thread.join();
    }
    reach_settings_model_wifi_clear_secrets(&app->model);
    reach_settings_release_bluetooth_icons(app);
    reach_wifi_service_destroy(app->wifi_service);
    app->wifi_service = nullptr;
    reach_bluetooth_service_destroy(app->bluetooth_service);
    app->bluetooth_service = nullptr;
    if (app->system_controls.destroy != nullptr)
    {
        app->system_controls.destroy(app->system_controls.userdata);
    }
    reach_settings_release_startup_icons(app);
    if (app->startup_apps.ops.destroy != nullptr)
    {
        app->startup_apps.ops.destroy(app->startup_apps.apps);
    }
    if (app->icon_provider.ops.destroy != nullptr)
    {
        app->icon_provider.ops.destroy(app->icon_provider.provider);
    }
    if (app->app_update.destroy != nullptr)
    {
        app->app_update.destroy(app->app_update.userdata);
    }
    if (app->windows_update.destroy != nullptr)
    {
        app->windows_update.destroy(app->windows_update.userdata);
    }
    (void)reach_config_service_flush(app->config_service);
    reach_config_service_destroy(app->config_service);
    app->config_service = nullptr;
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
