#include "reach/services/config.h"

#include "reach/services/pin_config.h"

#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>

enum
{
    REACH_CONFIG_JOB_RELOAD = 1,
    REACH_CONFIG_JOB_PIN_APP = 2,
    REACH_CONFIG_JOB_UNPIN_ID = 3,
    REACH_CONFIG_JOB_MOVE_PIN = 4
};

struct reach_config_service
{
    reach_config_store_port store;

    void (*notify)(void *user);
    void *notify_user;

    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;
    int32_t thread_started = 0;
    int32_t stop = 0;
    int32_t pending = 0;
    int32_t in_flight = 0;

    uint32_t generation = 0;
    uint32_t pending_generation = 0;
    int32_t pending_operation = 0;
    reach_pinned_app_model pending_app = {};
    uint32_t pending_pin_id = 0;
    size_t pending_target_index = 0;

    int32_t completed = 0;
    uint32_t completed_generation = 0;
    reach_result completed_result = REACH_OK;
    reach_config_snapshot completed_snapshot = {};
};

static void reach_config_service_thread_main(reach_config_service *service)
{
    for (;;)
    {
        uint32_t generation = 0;
        int32_t operation = 0;
        reach_pinned_app_model app = {};
        uint32_t pin_id = 0;
        size_t target_index = 0;
        {
            std::unique_lock<std::mutex> lock(service->mutex);
            service->cv.wait(lock, [service]() { return service->stop || service->pending; });

            if (service->stop)
            {
                return;
            }

            generation = service->pending_generation;
            operation = service->pending_operation;
            app = service->pending_app;
            pin_id = service->pending_pin_id;
            target_index = service->pending_target_index;
            service->pending = 0;
            service->in_flight = 1;
        }

        reach_config_snapshot snapshot = {};
        reach_result result = REACH_INVALID_ARGUMENT;
        switch (operation)
        {
        case REACH_CONFIG_JOB_RELOAD:
            result = service->store.ops.load != nullptr
                         ? service->store.ops.load(service->store.store, &snapshot)
                         : REACH_INVALID_ARGUMENT;
            break;
        case REACH_CONFIG_JOB_PIN_APP:
            result = reach_pin_config_pin_app(&service->store, &app);
            break;
        case REACH_CONFIG_JOB_UNPIN_ID:
            result = reach_pin_config_unpin_id(&service->store, pin_id);
            break;
        case REACH_CONFIG_JOB_MOVE_PIN:
            result = reach_pin_config_move_id(&service->store, pin_id, target_index);
            break;
        default:
            result = REACH_INVALID_ARGUMENT;
            break;
        }

        if (operation != REACH_CONFIG_JOB_RELOAD && result == REACH_OK)
        {
            result = service->store.ops.load != nullptr
                         ? service->store.ops.load(service->store.store, &snapshot)
                         : REACH_INVALID_ARGUMENT;
        }

        void (*notify)(void *user) = nullptr;
        void *notify_user = nullptr;
        {
            std::lock_guard<std::mutex> lock(service->mutex);
            service->in_flight = 0;
            if (!service->stop)
            {
                service->completed_generation = generation;
                service->completed_result = result;
                service->completed_snapshot = snapshot;
                service->completed = 1;
                notify = service->notify;
                notify_user = service->notify_user;
            }
        }

        if (notify != nullptr)
        {
            notify(notify_user);
        }
    }
}

reach_result reach_config_service_create(reach_config_store_port store,
                                         void (*notify)(void *user), void *notify_user,
                                         reach_config_service **out_service)
{
    if (out_service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_config_service *service = new (std::nothrow) reach_config_service();
    if (service == nullptr)
    {
        return REACH_ERROR;
    }
    service->store = store;
    service->notify = notify;
    service->notify_user = notify_user;
    *out_service = service;
    return REACH_OK;
}

void reach_config_service_destroy(reach_config_service *service)
{
    if (service == nullptr)
    {
        return;
    }
    reach_config_service_stop(service);
    delete service;
}

void reach_config_service_stop(reach_config_service *service)
{
    if (service == nullptr || !service->thread_started)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(service->mutex);
        service->stop = 1;
        service->pending = 0;
    }
    service->cv.notify_one();

    if (service->thread.joinable())
    {
        service->thread.join();
    }

    service->thread_started = 0;
    service->stop = 0;
    service->pending = 0;
    service->in_flight = 0;
    service->completed = 0;
}

static reach_result reach_config_service_schedule(reach_config_service *service,
                                                  int32_t operation,
                                                  const reach_pinned_app_model *app,
                                                  uint32_t pin_id, size_t target_index)
{
    if (service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (!service->thread_started)
    {
        service->stop = 0;
        try
        {
            service->thread = std::thread(reach_config_service_thread_main, service);
        }
        catch (...)
        {
            return REACH_ERROR;
        }
        service->thread_started = 1;
    }

    {
        std::lock_guard<std::mutex> lock(service->mutex);
        ++service->generation;
        service->pending_generation = service->generation;
        service->pending_operation = operation;
        service->pending_app = app != nullptr ? *app : reach_pinned_app_model{};
        service->pending_pin_id = pin_id;
        service->pending_target_index = target_index;
        service->pending = 1;
    }
    service->cv.notify_one();
    return REACH_OK;
}

reach_result reach_config_service_schedule_reload(reach_config_service *service)
{
    if (service == nullptr || service->store.ops.load == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    return reach_config_service_schedule(service, REACH_CONFIG_JOB_RELOAD, nullptr, 0, 0);
}

reach_result reach_config_service_schedule_pin_app(reach_config_service *service,
                                                   const reach_pinned_app_model *app)
{
    if (app == nullptr || app->path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    return reach_config_service_schedule(service, REACH_CONFIG_JOB_PIN_APP, app, 0, 0);
}

reach_result reach_config_service_schedule_unpin(reach_config_service *service, uint32_t pin_id)
{
    if (pin_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    return reach_config_service_schedule(service, REACH_CONFIG_JOB_UNPIN_ID, nullptr, pin_id, 0);
}

reach_result reach_config_service_schedule_move_pin(reach_config_service *service,
                                                    uint32_t pin_id, size_t target_index)
{
    if (pin_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    return reach_config_service_schedule(service, REACH_CONFIG_JOB_MOVE_PIN, nullptr, pin_id,
                                         target_index);
}

int32_t reach_config_service_take_completed(reach_config_service *service,
                                            reach_result *out_result,
                                            reach_config_snapshot *out_snapshot,
                                            int32_t *out_current)
{
    if (out_current != nullptr)
    {
        *out_current = 0;
    }
    if (service == nullptr || out_result == nullptr || out_snapshot == nullptr)
    {
        return 0;
    }

    std::lock_guard<std::mutex> lock(service->mutex);
    if (!service->completed)
    {
        return 0;
    }
    service->completed = 0;
    *out_result = service->completed_result;
    *out_snapshot = service->completed_snapshot;
    if (out_current != nullptr)
    {
        *out_current = service->completed_generation == service->generation;
    }
    return 1;
}

int32_t reach_config_service_work_pending(const reach_config_service *service)
{
    if (service == nullptr)
    {
        return 0;
    }
    reach_config_service *mutable_service = const_cast<reach_config_service *>(service);
    std::lock_guard<std::mutex> lock(mutable_service->mutex);
    return mutable_service->pending || mutable_service->in_flight || mutable_service->completed;
}
