#include "reach/services/app_control.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>

#include "reach/support/util.h"

enum
{
    REACH_APP_CONTROL_LAUNCH_QUEUE_CAPACITY = 16,
    REACH_APP_CONTROL_LAUNCH_MAX_WORKERS = 8,
    REACH_APP_CONTROL_MAX_WINDOWS = 32
};

static const int64_t REACH_APP_CONTROL_LAUNCH_IDLE_EXIT_MILLISECONDS = 10000;

struct reach_app_control_launch_state
{
    std::mutex mutex;
    std::condition_variable cv;
    reach_app_launcher_port launcher = {};
    reach_app_launch_request queue[REACH_APP_CONTROL_LAUNCH_QUEUE_CAPACITY] = {};
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

static void reach_app_control_launch_worker_main(reach_app_control_launch_state *state)
{
    for (;;)
    {
        reach_app_launch_request request = {};

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

            request = state->queue[state->queue_head];
            state->queue_head = (state->queue_head + 1) % REACH_APP_CONTROL_LAUNCH_QUEUE_CAPACITY;
            --state->queue_count;
        }

        if (state->launcher.ops.launch != nullptr)
        {
            (void)state->launcher.ops.launch(state->launcher.launcher, &request);
        }
    }
}

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
    int32_t window_pending = 0;
    int32_t window_completed = 0;
    reach_window_control_action window_pending_action = REACH_WINDOW_CONTROL_ACTIVATE;
    uintptr_t window_pending_windows[REACH_APP_CONTROL_MAX_WINDOWS] = {};
    size_t window_pending_window_count = 0;

    int32_t window_pending_is_snap = 0;
    reach_split_mode window_pending_snap_mode = REACH_SPLIT_LEFT;
    reach_result window_completed_result = REACH_OK;
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
        reach_window_control_action action = REACH_WINDOW_CONTROL_ACTIVATE;
        uintptr_t windows[REACH_APP_CONTROL_MAX_WINDOWS] = {};
        size_t window_count = 0;
        int32_t is_snap = 0;
        reach_split_mode snap_mode = REACH_SPLIT_LEFT;

        {
            std::unique_lock<std::mutex> lock(service->window_mutex);
            service->window_cv.wait(lock, [service]()
                                    { return service->window_stop || service->window_pending; });

            if (service->window_stop)
            {
                return;
            }

            action = service->window_pending_action;
            is_snap = service->window_pending_is_snap;
            snap_mode = service->window_pending_snap_mode;
            window_count = service->window_pending_window_count;
            if (window_count > REACH_APP_CONTROL_MAX_WINDOWS)
            {
                window_count = REACH_APP_CONTROL_MAX_WINDOWS;
            }
            for (size_t index = 0; index < window_count; ++index)
            {
                windows[index] = service->window_pending_windows[index];
            }
            service->window_pending = 0;
        }

        reach_result result = window_count > 0 ? REACH_OK : REACH_INVALID_ARGUMENT;
        for (size_t index = 0; index < window_count; ++index)
        {
            reach_result window_result =
                is_snap ? reach_app_control_snap_execute(service, windows[index], snap_mode)
                        : reach_app_control_window_execute(service, action, windows[index]);
            if (window_result != REACH_OK && result == REACH_OK)
            {
                result = window_result;
            }
        }

        {
            std::lock_guard<std::mutex> lock(service->window_mutex);
            if (!service->window_stop)
            {
                service->window_completed_result = result;
                service->window_completed = 1;
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
            service->window_pending = 0;
            service->window_pending_window_count = 0;
        }
        service->window_cv.notify_one();

        if (service->window_thread.joinable())
        {
            service->window_thread.join();
        }

        service->window_thread_started = 0;
        service->window_stop = 0;
        service->window_pending = 0;
        service->window_completed = 0;
        service->window_pending_window_count = 0;
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

reach_result reach_app_control_schedule_launch(reach_app_control *service,
                                               const reach_app_launch_request *request)
{
    if (service == nullptr || service->launch == nullptr || request == nullptr ||
        request->path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_app_control_launch_state *state = service->launch;
    if (state->launcher.ops.launch == nullptr)
    {
        return REACH_ERROR;
    }

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
                     REACH_APP_CONTROL_LAUNCH_QUEUE_CAPACITY] = *request;
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

reach_result reach_app_control_schedule_window(reach_app_control *service,
                                               reach_window_control_action action,
                                               uintptr_t window_id)
{
    if (service == nullptr || window_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = reach_app_control_start_window_worker(service);
    if (result != REACH_OK)
    {
        return result;
    }

    {
        std::lock_guard<std::mutex> lock(service->window_mutex);
        service->window_pending_action = action;
        service->window_pending_windows[0] = window_id;
        service->window_pending_window_count = 1;
        service->window_pending_is_snap = 0;
        service->window_pending = 1;
    }

    service->window_cv.notify_one();
    return REACH_OK;
}

reach_result reach_app_control_schedule_snap(reach_app_control *service, uintptr_t window_id,
                                             reach_split_mode mode)
{
    if (service == nullptr || window_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = reach_app_control_start_window_worker(service);
    if (result != REACH_OK)
    {
        return result;
    }

    {
        std::lock_guard<std::mutex> lock(service->window_mutex);
        service->window_pending_windows[0] = window_id;
        service->window_pending_window_count = 1;
        service->window_pending_is_snap = 1;
        service->window_pending_snap_mode = mode;
        service->window_pending = 1;
    }

    service->window_cv.notify_one();
    return REACH_OK;
}

reach_result reach_app_control_schedule_minimize(reach_app_control *service,
                                                 const uintptr_t *window_ids, size_t window_count)
{
    if (service == nullptr || window_ids == nullptr || window_count == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (window_count > REACH_APP_CONTROL_MAX_WINDOWS)
    {
        window_count = REACH_APP_CONTROL_MAX_WINDOWS;
    }

    reach_result result = reach_app_control_start_window_worker(service);
    if (result != REACH_OK)
    {
        return result;
    }

    {
        std::lock_guard<std::mutex> lock(service->window_mutex);
        service->window_pending_action = REACH_WINDOW_CONTROL_MINIMIZE;
        service->window_pending_is_snap = 0;
        service->window_pending_window_count = window_count;
        for (size_t index = 0; index < window_count; ++index)
        {
            service->window_pending_windows[index] = window_ids[index];
        }
        for (size_t index = window_count; index < REACH_APP_CONTROL_MAX_WINDOWS; ++index)
        {
            service->window_pending_windows[index] = 0;
        }
        service->window_pending = 1;
    }

    service->window_cv.notify_one();
    return REACH_OK;
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
