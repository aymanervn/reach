#include "reach/services/app_control.h"
#include "reach/core/limits.h"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <vector>
#include <utility>
#include <mutex>
#include <new>
#include <thread>

#include "reach/support/util.h"

enum
{
    REACH_APP_CONTROL_LAUNCH_QUEUE_CAPACITY = 16,
    REACH_APP_CONTROL_LAUNCH_MAX_WORKERS = 8,
    REACH_APP_CONTROL_MAX_WINDOWS = REACH_MAX_OPEN_WINDOWS
};

static const int64_t REACH_APP_CONTROL_LAUNCH_IDLE_EXIT_MILLISECONDS = 10000;

enum reach_app_control_launch_item_kind
{
    REACH_APP_CONTROL_ITEM_LAUNCH = 0,
    REACH_APP_CONTROL_ITEM_REVEAL = 1,
    REACH_APP_CONTROL_ITEM_TERMINAL = 2,
    REACH_APP_CONTROL_ITEM_OPEN_LOCATION = 3
};

struct reach_app_control_launch_item
{
    int32_t kind;
    reach_app_launch_request launch;
    reach_terminal_launch_request terminal;
    reach_app_control_location_kind location;
};

struct reach_app_control_launch_state
{
    std::mutex mutex;
    std::condition_variable cv;
    reach_app_launcher_port launcher = {};
    reach_terminal_launcher_port terminal_launcher = {};
    reach_explorer_service_port explorer = {};
    reach_app_control_launch_item queue[REACH_APP_CONTROL_LAUNCH_QUEUE_CAPACITY] = {};
    size_t queue_head = 0;
    size_t queue_count = 0;
    int32_t idle_workers = 0;
    int32_t total_workers = 0;
    int32_t stop = 0;
    int32_t refs = 1;
};

static void reach_app_control_launch_state_release(reach_app_control_launch_state *state)
{
    int32_t last = 0;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        --state->refs;
        last = state->refs == 0;
    }
    if (last)
    {
        delete state;
    }
}

static void reach_app_control_open_default(const reach_app_control_launch_state *state)
{
    if (state->explorer.ops.open_default != nullptr)
    {
        (void)state->explorer.ops.open_default(state->explorer.service);
    }
}

static void reach_app_control_open_location(const reach_app_control_launch_state *state,
                                            reach_app_control_location_kind kind,
                                            const uint16_t *path)
{
    switch (kind)
    {
    case REACH_APP_CONTROL_LOCATION_PATH:
        if (state->explorer.ops.path_exists != nullptr &&
            state->explorer.ops.path_exists(state->explorer.service, path) &&
            state->explorer.ops.open_path != nullptr)
        {
            (void)state->explorer.ops.open_path(state->explorer.service, path);
            return;
        }
        reach_app_control_open_default(state);
        return;

    case REACH_APP_CONTROL_LOCATION_SHELL:
        if (state->explorer.ops.open_shell_location != nullptr)
        {
            (void)state->explorer.ops.open_shell_location(state->explorer.service, path);
            return;
        }
        reach_app_control_open_default(state);
        return;

    case REACH_APP_CONTROL_LOCATION_DEFAULT:
    default:
        reach_app_control_open_default(state);
        return;
    }
}

static void reach_app_control_launch_worker_main(reach_app_control_launch_state *state)
{
    for (;;)
    {
        reach_app_control_launch_item item = {};

        {
            std::unique_lock<std::mutex> lock(state->mutex);
            while (!state->stop && state->queue_count == 0)
            {
                ++state->idle_workers;
                std::cv_status waited = state->cv.wait_for(
                    lock,
                    std::chrono::milliseconds(REACH_APP_CONTROL_LAUNCH_IDLE_EXIT_MILLISECONDS));
                --state->idle_workers;
                if (waited == std::cv_status::timeout && state->queue_count == 0)
                {
                    --state->total_workers;
                    lock.unlock();
                    reach_app_control_launch_state_release(state);
                    return;
                }
            }
            if (state->stop)
            {
                --state->total_workers;
                lock.unlock();
                reach_app_control_launch_state_release(state);
                return;
            }

            item = state->queue[state->queue_head];
            state->queue_head = (state->queue_head + 1) % REACH_APP_CONTROL_LAUNCH_QUEUE_CAPACITY;
            --state->queue_count;
        }

        if (item.kind == REACH_APP_CONTROL_ITEM_OPEN_LOCATION)
        {
            reach_app_control_open_location(state, item.location, item.launch.path);
        }
        else if (item.kind == REACH_APP_CONTROL_ITEM_REVEAL)
        {
            if (state->explorer.ops.reveal_path != nullptr)
            {
                (void)state->explorer.ops.reveal_path(state->explorer.service, item.launch.path);
            }
        }
        else if (item.kind == REACH_APP_CONTROL_ITEM_TERMINAL)
        {
            if (state->terminal_launcher.ops.launch != nullptr)
            {
                (void)state->terminal_launcher.ops.launch(state->terminal_launcher.launcher,
                                                          &item.terminal);
            }
        }
        else if (state->launcher.ops.launch != nullptr)
        {
            (void)state->launcher.ops.launch(state->launcher.launcher, &item.launch);
        }
    }
}

struct reach_app_control_window_request
{
    reach_window_control_action action = REACH_WINDOW_CONTROL_ACTIVATE;
    std::vector<uintptr_t> windows;
    int32_t is_snap = 0;
    reach_split_mode snap_mode = REACH_SPLIT_LEFT;
    reach_window_id cover = 0;
    uint64_t preparation = 0;
};

struct reach_app_control
{
    reach_app_control_launch_state *launch = nullptr;

    reach_window_manager_port window_manager = {};
    void (*notify)(void *user) = nullptr;
    void *notify_user = nullptr;

    std::thread window_thread;
    std::mutex window_mutex;
    std::condition_variable window_cv;
    int32_t window_thread_started = 0;
    int32_t window_stop = 0;
    int32_t window_completed = 0;
    reach_result window_completed_result = REACH_OK;
    std::deque<reach_app_control_window_request> window_requests;
    uint64_t preparation_hold = 0;
    uint64_t running_preparation = 0;
    reach_window_preparation_result preparation = {};
};

static reach_result reach_app_control_window_dispatch(reach_app_control *service,
                                                      reach_window_control_action action,
                                                      uintptr_t window_id, reach_window_id cover)
{
    switch (action)
    {
    case REACH_WINDOW_CONTROL_PREPARE:
        return service->window_manager.ops.prepare != nullptr
                   ? service->window_manager.ops.prepare(service->window_manager.manager, window_id,
                                                         cover)
                   : REACH_ERROR;
    case REACH_WINDOW_CONTROL_ACTIVATE:
        return service->window_manager.ops.activate != nullptr
                   ? service->window_manager.ops.activate(service->window_manager.manager,
                                                          window_id)
                   : REACH_ERROR;
    case REACH_WINDOW_CONTROL_MINIMIZE:
        return service->window_manager.ops.minimize != nullptr
                   ? service->window_manager.ops.minimize(service->window_manager.manager,
                                                          window_id)
                   : REACH_ERROR;
    case REACH_WINDOW_CONTROL_CLOSE:
        return service->window_manager.ops.close != nullptr
                   ? service->window_manager.ops.close(service->window_manager.manager, window_id)
                   : REACH_ERROR;
    default:
        return REACH_INVALID_ARGUMENT;
    }
}

static reach_result reach_app_control_window_execute(reach_app_control *service,
                                                     reach_window_control_action action,
                                                     uintptr_t window_id, reach_window_id cover = 0)
{
    if (service == nullptr || window_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (service->window_manager.ops.privileged_control_available != nullptr &&
        service->window_manager.ops.privileged_control_available(service->window_manager.manager))
    {
        return reach_app_control_window_dispatch(service, action, window_id, cover);
    }

    if (service->window_manager.ops.start_privileged_control == nullptr ||
        service->window_manager.ops.start_privileged_control(service->window_manager.manager) !=
            REACH_OK)
    {
        return REACH_ERROR;
    }

    if (service->window_manager.ops.privileged_control_available != nullptr &&
        !service->window_manager.ops.privileged_control_available(service->window_manager.manager))
    {
        return REACH_ERROR;
    }

    return reach_app_control_window_dispatch(service, action, window_id, cover);
}

static reach_result reach_app_control_snap_dispatch(reach_app_control *service, uintptr_t window_id,
                                                    reach_split_mode mode)
{
    return service->window_manager.ops.snap != nullptr
               ? service->window_manager.ops.snap(service->window_manager.manager, window_id, mode)
               : REACH_ERROR;
}

static reach_result reach_app_control_snap_execute(reach_app_control *service, uintptr_t window_id,
                                                   reach_split_mode mode)
{
    if (service == nullptr || window_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (service->window_manager.ops.privileged_control_available != nullptr &&
        service->window_manager.ops.privileged_control_available(service->window_manager.manager))
    {
        return reach_app_control_snap_dispatch(service, window_id, mode);
    }

    if (service->window_manager.ops.start_privileged_control == nullptr ||
        service->window_manager.ops.start_privileged_control(service->window_manager.manager) !=
            REACH_OK)
    {
        return REACH_ERROR;
    }

    if (service->window_manager.ops.privileged_control_available != nullptr &&
        !service->window_manager.ops.privileged_control_available(service->window_manager.manager))
    {
        return REACH_ERROR;
    }

    return reach_app_control_snap_dispatch(service, window_id, mode);
}

static void reach_app_control_window_thread_main(reach_app_control *service)
{
    for (;;)
    {
        reach_app_control_window_request request;
        {
            std::unique_lock<std::mutex> lock(service->window_mutex);
            service->window_cv.wait(lock,
                                    [service]()
                                    {
                                        return service->window_stop ||
                                               (service->preparation_hold == 0 &&
                                                !service->window_requests.empty());
                                    });
            if (service->window_stop)
            {
                return;
            }
            request = std::move(service->window_requests.front());
            service->window_requests.pop_front();
            service->running_preparation = request.preparation;
            service->preparation_hold = request.preparation;
        }

        reach_result result = REACH_OK;
        for (uintptr_t window : request.windows)
        {
            reach_result window_result =
                request.is_snap ? reach_app_control_snap_execute(service, window, request.snap_mode)
                                : reach_app_control_window_execute(service, request.action, window,
                                                                   request.cover);
            if (window_result != REACH_OK && result == REACH_OK)
            {
                result = window_result;
            }
        }

        {
            std::lock_guard<std::mutex> lock(service->window_mutex);
            service->running_preparation = 0;
            if (!service->window_stop)
            {
                if (request.preparation != 0)
                {
                    service->preparation = {
                        request.preparation,
                        request.action == REACH_WINDOW_CONTROL_PREPARE ? request.windows[0] : 0,
                        result};
                }
                else
                {
                    if (!service->window_completed || result != REACH_OK)
                    {
                        service->window_completed_result = result;
                    }
                    service->window_completed = 1;
                }
            }
        }
        if (service->notify != nullptr)
        {
            service->notify(service->notify_user);
        }
    }
}

static reach_result reach_app_control_start_window_worker(reach_app_control *service)
{
    if (service->window_thread_started)
    {
        return REACH_OK;
    }

    service->window_stop = 0;
    try
    {
        service->window_thread = std::thread(reach_app_control_window_thread_main, service);
    }
    catch (...)
    {
        return REACH_ERROR;
    }

    service->window_thread_started = 1;
    return REACH_OK;
}

reach_result reach_app_control_create(reach_app_launcher_port launcher,
                                      reach_terminal_launcher_port terminal_launcher,
                                      reach_explorer_service_port explorer,
                                      reach_window_manager_port window_manager,
                                      void (*notify)(void *user), void *notify_user,
                                      reach_app_control **out_service)
{
    if (out_service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_app_control *service = new (std::nothrow) reach_app_control();
    reach_app_control_launch_state *launch = new (std::nothrow) reach_app_control_launch_state();
    if (service == nullptr || launch == nullptr)
    {
        delete service;
        delete launch;
        return REACH_ERROR;
    }
    launch->launcher = launcher;
    launch->terminal_launcher = terminal_launcher;
    launch->explorer = explorer;
    service->launch = launch;
    service->window_manager = window_manager;
    service->notify = notify;
    service->notify_user = notify_user;
    *out_service = service;
    return REACH_OK;
}

void reach_app_control_stop(reach_app_control *service)
{
    if (service == nullptr)
    {
        return;
    }

    if (service->launch != nullptr)
    {
        std::lock_guard<std::mutex> lock(service->launch->mutex);
        service->launch->stop = 1;
        service->launch->queue_count = 0;
        service->launch->cv.notify_all();
    }

    if (service->window_thread_started)
    {
        {
            std::lock_guard<std::mutex> lock(service->window_mutex);
            service->window_stop = 1;
            service->window_requests.clear();
        }
        service->window_cv.notify_one();

        if (service->window_thread.joinable())
        {
            service->window_thread.join();
        }

        service->window_thread_started = 0;
        service->window_stop = 0;
        service->window_completed = 0;
        service->preparation_hold = 0;
        service->running_preparation = 0;
        service->preparation = {};
    }
}

void reach_app_control_destroy(reach_app_control *service)
{
    if (service == nullptr)
    {
        return;
    }
    reach_app_control_stop(service);
    if (service->launch != nullptr)
    {
        reach_app_control_launch_state_release(service->launch);
    }
    delete service;
}

int32_t reach_app_control_launch_available(const reach_app_control *service)
{
    return service != nullptr && service->launch != nullptr &&
           service->launch->launcher.ops.launch != nullptr;
}

static reach_result reach_app_control_enqueue(reach_app_control_launch_state *state,
                                              const reach_app_control_launch_item *item)
{
    int32_t spawn = 0;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->stop)
        {
            return REACH_ERROR;
        }
        REACH_ASSERT(state->queue_count < REACH_APP_CONTROL_LAUNCH_QUEUE_CAPACITY);
        if (state->queue_count >= REACH_APP_CONTROL_LAUNCH_QUEUE_CAPACITY)
        {
            return REACH_ERROR;
        }
        state->queue[(state->queue_head + state->queue_count) %
                     REACH_APP_CONTROL_LAUNCH_QUEUE_CAPACITY] = *item;
        ++state->queue_count;

        if (state->idle_workers > 0)
        {
            state->cv.notify_one();
        }
        else if (state->total_workers < REACH_APP_CONTROL_LAUNCH_MAX_WORKERS)
        {
            ++state->total_workers;
            ++state->refs;
            spawn = 1;
        }
    }

    if (spawn)
    {
        try
        {
            std::thread(reach_app_control_launch_worker_main, state).detach();
        }
        catch (...)
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            --state->total_workers;
            --state->refs;
            if (state->queue_count > 0)
            {
                --state->queue_count;
            }
            return REACH_ERROR;
        }
    }

    return REACH_OK;
}

reach_result reach_app_control_schedule_launch(reach_app_control *service,
                                               const reach_app_launch_request *request)
{
    if (service == nullptr || service->launch == nullptr || request == nullptr ||
        (request->path[0] == 0 && request->app_user_model_id[0] == 0))
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (service->launch->launcher.ops.launch == nullptr)
    {
        return REACH_ERROR;
    }

    reach_app_control_launch_item item = {};
    item.kind = REACH_APP_CONTROL_ITEM_LAUNCH;
    item.launch = *request;
    return reach_app_control_enqueue(service->launch, &item);
}

reach_result
reach_app_control_schedule_terminal_launch(reach_app_control *service,
                                           const reach_terminal_launch_request *request)
{
    if (service == nullptr || service->launch == nullptr || request == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (service->launch->terminal_launcher.ops.launch == nullptr)
    {
        return REACH_ERROR;
    }

    reach_app_control_launch_item item = {};
    item.kind = REACH_APP_CONTROL_ITEM_TERMINAL;
    item.terminal = *request;
    return reach_app_control_enqueue(service->launch, &item);
}

int32_t reach_app_control_reveal_available(const reach_app_control *service)
{
    return service != nullptr && service->launch != nullptr &&
           service->launch->explorer.ops.reveal_path != nullptr;
}

reach_result reach_app_control_schedule_reveal(reach_app_control *service, const uint16_t *path)
{
    if (service == nullptr || service->launch == nullptr || path == nullptr || path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (service->launch->explorer.ops.reveal_path == nullptr)
    {
        return REACH_ERROR;
    }

    reach_app_control_launch_item item = {};
    item.kind = REACH_APP_CONTROL_ITEM_REVEAL;
    reach_copy_utf16(item.launch.path, 260, path);
    return reach_app_control_enqueue(service->launch, &item);
}

reach_result reach_app_control_schedule_open_location(reach_app_control *service,
                                                      reach_app_control_location_kind kind,
                                                      const uint16_t *path)
{
    if (service == nullptr || service->launch == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (service->launch->explorer.service == nullptr)
    {
        return REACH_ERROR;
    }

    reach_app_control_launch_item item = {};
    item.kind = REACH_APP_CONTROL_ITEM_OPEN_LOCATION;
    item.location = kind;
    if (path != nullptr)
    {
        reach_copy_utf16(item.launch.path, 260, path);
    }
    return reach_app_control_enqueue(service->launch, &item);
}

static reach_result reach_app_control_enqueue_window(reach_app_control *service,
                                                     reach_app_control_window_request request)
{
    reach_result result = reach_app_control_start_window_worker(service);
    if (result != REACH_OK)
    {
        return result;
    }
    {
        std::lock_guard<std::mutex> lock(service->window_mutex);
        if (request.action == REACH_WINDOW_CONTROL_ACTIVATE && !request.is_snap &&
            request.preparation == 0)
        {
            for (auto it = service->window_requests.begin(); it != service->window_requests.end();)
            {
                if (it->action == REACH_WINDOW_CONTROL_ACTIVATE && !it->is_snap &&
                    it->preparation == 0)
                {
                    it = service->window_requests.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
        service->window_requests.push_back(std::move(request));
    }
    service->window_cv.notify_one();
    return REACH_OK;
}

reach_result reach_app_control_schedule_window(reach_app_control *service,
                                               reach_window_control_action action,
                                               uintptr_t window_id)
{
    return reach_app_control_schedule_windows(service, action, &window_id, 1);
}

static reach_result reach_app_control_queue_preparation(reach_app_control *service,
                                                        reach_window_control_action action,
                                                        const uintptr_t *windows, size_t count,
                                                        reach_window_id cover, uint64_t request_id)
{
    if (service == nullptr || (count != 0 && windows == nullptr) ||
        count > REACH_APP_CONTROL_MAX_WINDOWS || cover == 0 || request_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_app_control_window_request request;
    request.action = action;
    if (count != 0)
    {
        request.windows.assign(windows, windows + count);
    }
    request.cover = cover;
    request.preparation = request_id;
    return reach_app_control_enqueue_window(service, std::move(request));
}

reach_result reach_app_control_schedule_preparation(reach_app_control *service,
                                                    reach_window_id window, reach_window_id cover,
                                                    uint64_t request)
{
    return window != 0 ? reach_app_control_queue_preparation(service, REACH_WINDOW_CONTROL_PREPARE,
                                                             &window, 1, cover, request)
                       : REACH_INVALID_ARGUMENT;
}

reach_result reach_app_control_schedule_desktop_preparation(reach_app_control *service,
                                                            const uintptr_t *windows, size_t count,
                                                            reach_window_id cover, uint64_t request)
{
    return reach_app_control_queue_preparation(service, REACH_WINDOW_CONTROL_MINIMIZE, windows,
                                               count, cover, request);
}

int32_t reach_app_control_cancel_preparation(reach_app_control *service, uint64_t request)
{
    if (service == nullptr || request == 0)
    {
        return 1;
    }
    std::lock_guard<std::mutex> lock(service->window_mutex);
    for (auto it = service->window_requests.begin(); it != service->window_requests.end();)
    {
        if (it->preparation == request)
        {
            it = service->window_requests.erase(it);
        }
        else
        {
            ++it;
        }
    }
    return service->running_preparation != request;
}

void reach_app_control_release_preparation(reach_app_control *service, uint64_t request)
{
    if (service == nullptr || request == 0)
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(service->window_mutex);
        if (service->running_preparation == request)
        {
            return;
        }
        if (service->preparation_hold == request)
        {
            service->preparation_hold = 0;
        }
        if (service->preparation.request == request)
        {
            service->preparation = {};
        }
    }
    service->window_cv.notify_one();
}

int32_t reach_app_control_take_preparation(reach_app_control *service,
                                           reach_window_preparation_result *out)
{
    if (service == nullptr || out == nullptr)
    {
        return 0;
    }
    std::lock_guard<std::mutex> lock(service->window_mutex);
    *out = service->preparation;
    service->preparation = {};
    return out->request != 0;
}

reach_result reach_app_control_schedule_snap(reach_app_control *service, uintptr_t window_id,
                                             reach_split_mode mode)
{
    if (service == nullptr || window_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_app_control_window_request request;
    request.windows.push_back(window_id);
    request.is_snap = 1;
    request.snap_mode = mode;
    return reach_app_control_enqueue_window(service, std::move(request));
}

reach_result reach_app_control_schedule_windows(reach_app_control *service,
                                                reach_window_control_action action,
                                                const uintptr_t *window_ids, size_t window_count)
{
    if (service == nullptr || window_ids == nullptr || window_count == 0 ||
        action == REACH_WINDOW_CONTROL_PREPARE)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (window_count > REACH_APP_CONTROL_MAX_WINDOWS)
    {
        window_count = REACH_APP_CONTROL_MAX_WINDOWS;
    }
    for (size_t index = 0; index < window_count; ++index)
    {
        if (window_ids[index] == 0)
        {
            return REACH_INVALID_ARGUMENT;
        }
    }
    reach_app_control_window_request request;
    request.action = action;
    request.windows.assign(window_ids, window_ids + window_count);
    return reach_app_control_enqueue_window(service, std::move(request));
}

reach_result reach_app_control_window_bounds(const reach_app_control *service, uintptr_t window_id,
                                             reach_rect_f32 *out_bounds)
{
    if (service == nullptr || out_bounds == nullptr || window_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (service->window_manager.ops.outer_bounds == nullptr)
    {
        return REACH_NOT_IMPLEMENTED;
    }
    return service->window_manager.ops.outer_bounds(service->window_manager.manager, window_id,
                                                    out_bounds);
}

reach_result reach_app_control_window_frame_bounds(const reach_app_control *service,
                                                   uintptr_t window_id, reach_rect_f32 *out_bounds)
{
    if (service == nullptr || out_bounds == nullptr || window_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (service->window_manager.ops.frame_bounds == nullptr)
    {
        return REACH_NOT_IMPLEMENTED;
    }
    return service->window_manager.ops.frame_bounds(service->window_manager.manager, window_id,
                                                    out_bounds);
}

reach_result reach_app_control_move_windows(reach_app_control *service,
                                            const reach_window_move *windows, size_t count)
{
    if (service == nullptr || windows == nullptr || count == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (service->window_manager.ops.move_windows == nullptr)
    {
        return REACH_NOT_IMPLEMENTED;
    }
    return service->window_manager.ops.move_windows(service->window_manager.manager, windows,
                                                    count);
}

int32_t reach_app_control_take_window_completed(reach_app_control *service,
                                                reach_result *out_result)
{
    if (out_result != nullptr)
    {
        *out_result = REACH_OK;
    }
    if (service == nullptr)
    {
        return 0;
    }

    std::lock_guard<std::mutex> lock(service->window_mutex);
    int32_t completed = service->window_completed;
    if (out_result != nullptr)
    {
        *out_result = service->window_completed_result;
    }
    service->window_completed = 0;
    return completed;
}
