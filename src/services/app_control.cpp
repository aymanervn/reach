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
    uint64_t request_id;
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
    int32_t callbacks_active = 0;
    uint64_t next_request_id = 1;
    std::deque<reach_app_launch_completion> completions;
    void (*notify)(void *user) = nullptr;
    void *notify_user = nullptr;
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

static void reach_app_control_publish_launch_completion(reach_app_control_launch_state *state,
                                                        uint64_t request_id, reach_result result,
                                                        reach_app_launch_failure failure)
{
    void (*notify)(void *user) = nullptr;
    void *notify_user = nullptr;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->stop)
        {
            return;
        }
        reach_app_launch_completion completion = {};
        completion.request_id = request_id;
        completion.result = result;
        completion.failure = failure;
        state->completions.push_back(completion);
        notify = state->notify;
        notify_user = state->notify_user;
        if (notify != nullptr)
        {
            ++state->callbacks_active;
        }
    }
    if (notify == nullptr)
    {
        return;
    }
    notify(notify_user);
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        --state->callbacks_active;
        if (state->stop && state->callbacks_active == 0)
        {
            state->cv.notify_all();
        }
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
            reach_app_launch_failure failure = REACH_APP_LAUNCH_FAILURE_NONE;
            reach_result result =
                state->launcher.ops.launch(state->launcher.launcher, &item.launch, &failure);
            reach_app_control_publish_launch_completion(state, item.request_id, result, failure);
        }
    }
}

struct reach_app_control_window_request
{
    uint64_t request_id = 0;
    reach_window_control_action action = REACH_WINDOW_CONTROL_ACTIVATE;
    std::vector<uintptr_t> windows;
    int32_t is_snap = 0;
    reach_split_mode snap_mode = REACH_SPLIT_LEFT;
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
    uint64_t next_window_request_id = 1;
    std::deque<reach_app_control_window_request> window_requests;
    std::deque<reach_window_control_completion> window_completions;
};

static reach_result reach_app_control_window_dispatch(reach_app_control *service,
                                                      reach_window_control_action action,
                                                      uintptr_t window_id)
{
    switch (action)
    {
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
                                                     uintptr_t window_id)
{
    if (service == nullptr || window_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (service->window_manager.ops.privileged_control_available != nullptr &&
        service->window_manager.ops.privileged_control_available(service->window_manager.manager))
    {
        return reach_app_control_window_dispatch(service, action, window_id);
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

    return reach_app_control_window_dispatch(service, action, window_id);
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
            service->window_cv.wait(
                lock,
                [service]() { return service->window_stop || !service->window_requests.empty(); });
            if (service->window_stop)
            {
                return;
            }
            request = std::move(service->window_requests.front());
            service->window_requests.pop_front();
        }

        reach_result result = REACH_OK;
        for (uintptr_t window : request.windows)
        {
            reach_result window_result =
                request.is_snap ? reach_app_control_snap_execute(service, window, request.snap_mode)
                                : reach_app_control_window_execute(service, request.action, window);
            if (window_result != REACH_OK && result == REACH_OK)
            {
                result = window_result;
            }
        }

        {
            std::lock_guard<std::mutex> lock(service->window_mutex);
            if (!service->window_stop)
            {
                reach_window_control_completion completion = {};
                completion.request_id = request.request_id;
                completion.action = request.action;
                completion.result = result;
                service->window_completions.push_back(completion);
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
    launch->notify = notify;
    launch->notify_user = notify_user;
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
        std::unique_lock<std::mutex> lock(service->launch->mutex);
        service->launch->stop = 1;
        service->launch->queue_count = 0;
        service->launch->completions.clear();
        service->launch->notify = nullptr;
        service->launch->notify_user = nullptr;
        service->launch->cv.notify_all();
        service->launch->cv.wait(lock,
                                 [service]() { return service->launch->callbacks_active == 0; });
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
        service->window_completions.clear();
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
                                              const reach_app_control_launch_item *item,
                                              uint64_t *out_request_id)
{
    int32_t spawn = 0;
    uint64_t request_id = 0;
    if (out_request_id != nullptr)
    {
        *out_request_id = 0;
    }
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
        reach_app_control_launch_item queued = *item;
        if (item->kind == REACH_APP_CONTROL_ITEM_LAUNCH)
        {
            request_id = state->next_request_id++;
            if (state->next_request_id == 0)
            {
                state->next_request_id = 1;
            }
            queued.request_id = request_id;
        }
        state->queue[(state->queue_head + state->queue_count) %
                     REACH_APP_CONTROL_LAUNCH_QUEUE_CAPACITY] = queued;
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

    if (out_request_id != nullptr)
    {
        *out_request_id = request_id;
    }
    return REACH_OK;
}

reach_result reach_app_control_schedule_launch(reach_app_control *service,
                                               const reach_app_launch_request *request,
                                               uint64_t *out_request_id)
{
    if (out_request_id != nullptr)
    {
        *out_request_id = 0;
    }
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
    return reach_app_control_enqueue(service->launch, &item, out_request_id);
}

int32_t reach_app_control_take_launch_completion(reach_app_control *service,
                                                 reach_app_launch_completion *out_completion)
{
    if (out_completion != nullptr)
    {
        *out_completion = {};
    }
    if (service == nullptr || service->launch == nullptr)
    {
        return 0;
    }
    std::lock_guard<std::mutex> lock(service->launch->mutex);
    if (service->launch->completions.empty())
    {
        return 0;
    }
    if (out_completion != nullptr)
    {
        *out_completion = service->launch->completions.front();
    }
    service->launch->completions.pop_front();
    return 1;
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
    return reach_app_control_enqueue(service->launch, &item, nullptr);
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
    return reach_app_control_enqueue(service->launch, &item, nullptr);
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
    return reach_app_control_enqueue(service->launch, &item, nullptr);
}

static reach_result reach_app_control_enqueue_window(reach_app_control *service,
                                                     reach_app_control_window_request request,
                                                     uint64_t *out_request_id)
{
    int32_t completion_added = 0;
    if (out_request_id != nullptr)
    {
        *out_request_id = 0;
    }
    reach_result result = reach_app_control_start_window_worker(service);
    if (result != REACH_OK)
    {
        return result;
    }
    {
        std::lock_guard<std::mutex> lock(service->window_mutex);
        request.request_id = service->next_window_request_id++;
        if (service->next_window_request_id == 0)
        {
            service->next_window_request_id = 1;
        }
        if (request.action == REACH_WINDOW_CONTROL_ACTIVATE && !request.is_snap)
        {
            for (auto it = service->window_requests.begin(); it != service->window_requests.end();)
            {
                if (it->action == REACH_WINDOW_CONTROL_ACTIVATE && !it->is_snap)
                {
                    reach_window_control_completion completion = {};
                    completion.request_id = it->request_id;
                    completion.action = it->action;
                    completion.result = REACH_ERROR;
                    service->window_completions.push_back(completion);
                    completion_added = 1;
                    it = service->window_requests.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
        if (out_request_id != nullptr)
        {
            *out_request_id = request.request_id;
        }
        service->window_requests.push_back(std::move(request));
    }
    service->window_cv.notify_one();
    if (completion_added && service->notify != nullptr)
    {
        service->notify(service->notify_user);
    }
    return REACH_OK;
}

reach_result reach_app_control_schedule_window(reach_app_control *service,
                                               reach_window_control_action action,
                                               uintptr_t window_id, uint64_t *out_request_id)
{
    return reach_app_control_schedule_windows(service, action, &window_id, 1, out_request_id);
}

reach_result reach_app_control_schedule_snap(reach_app_control *service, uintptr_t window_id,
                                             reach_split_mode mode, uint64_t *out_request_id)
{
    if (out_request_id != nullptr)
    {
        *out_request_id = 0;
    }
    if (service == nullptr || window_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_app_control_window_request request;
    request.action = REACH_WINDOW_CONTROL_SNAP;
    request.windows.push_back(window_id);
    request.is_snap = 1;
    request.snap_mode = mode;
    return reach_app_control_enqueue_window(service, std::move(request), out_request_id);
}

reach_result reach_app_control_schedule_windows(reach_app_control *service,
                                                reach_window_control_action action,
                                                const uintptr_t *window_ids, size_t window_count,
                                                uint64_t *out_request_id)
{
    if (out_request_id != nullptr)
    {
        *out_request_id = 0;
    }
    if (service == nullptr || window_ids == nullptr || window_count == 0 ||
        action < REACH_WINDOW_CONTROL_ACTIVATE || action > REACH_WINDOW_CONTROL_CLOSE)
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
    return reach_app_control_enqueue_window(service, std::move(request), out_request_id);
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

int32_t reach_app_control_take_window_completion(reach_app_control *service,
                                                 reach_window_control_completion *out_completion)
{
    if (out_completion != nullptr)
    {
        *out_completion = {};
    }
    if (service == nullptr)
    {
        return 0;
    }

    std::lock_guard<std::mutex> lock(service->window_mutex);
    if (service->window_completions.empty())
    {
        return 0;
    }
    if (out_completion != nullptr)
    {
        *out_completion = service->window_completions.front();
    }
    service->window_completions.pop_front();
    return 1;
}
