#include "reach/app/settings_app.h"

#include "reach/core/theme.h"
#include "reach/features/settings.h"
#include "reach/platform/windows_adapters.h"

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
    reach_settings_model model;
    reach_settings_layout layout;
    reach_render_command_buffer render_commands;
    reach_rect_f32 bounds;
    const reach_theme *theme;
    reach_settings_update_worker update_worker;
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

static void reach_settings_handle_pointer_up(reach_settings_app *app, const reach_ui_event *event)
{
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
}

static void reach_settings_handle_pointer_down(reach_settings_app *app, const reach_ui_event *event)
{
    if (app == nullptr || event == nullptr ||
        app->model.selected_page != REACH_SETTINGS_PAGE_UPDATE)
    {
        return;
    }
    reach_settings_refresh_bounds(app);
    float x = (float)event->x - app->bounds.x;
    float y = (float)event->y - app->bounds.y;
    reach_settings_hit_result hit = reach_settings_hit_test(&app->layout, x, y);
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
    if (reach_settings_model_update_scroll(&app->model, delta_seconds))
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
    reach_settings_update_worker *worker =
        const_cast<reach_settings_update_worker *>(&app->update_worker);
    std::lock_guard<std::mutex> lock(worker->mutex);
    return app->dirty || worker->pending || worker->in_flight || worker->completed ||
           app->update_worker.progress_state.load() != 0 ||
           app->model.update_scrollbar.offset != app->model.update_scrollbar.target ||
           app->update_scrollbar_drag.active;
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
    if (app->windows_update.destroy != nullptr)
    {
        app->windows_update.destroy(app->windows_update.userdata);
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
