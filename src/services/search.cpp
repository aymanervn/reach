#include "reach/services/search.h"

#include "reach/support/util.h"

#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>

struct reach_search_service
{
    reach_search_provider_port provider;

    void (*notify)(void *user);
    void *notify_user;

    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;
    int32_t thread_started = 0;
    int32_t stop = 0;
    int32_t pending = 0;
    int32_t in_flight = 0;

    uint32_t pending_generation = 0;
    uint16_t pending_query[REACH_MAX_SEARCH_CHARS + 1] = {};

    int32_t completed = 0;
    uint32_t completed_generation = 0;
    reach_search_candidate completed_results[REACH_SEARCH_MAX_RESULTS] = {};
    size_t completed_count = 0;
};

static void reach_search_service_query(reach_search_service *service, const uint16_t *query,
                                       reach_search_candidate *out_results, size_t *out_count)
{
    *out_count = 0;
    if (query == nullptr || query[0] == 0 || service->provider.ops.query == nullptr ||
        service->provider.ops.result_count == nullptr || service->provider.ops.result_at == nullptr)
    {
        return;
    }

    if (service->provider.ops.query(service->provider.provider, query) != REACH_OK)
    {
        return;
    }

    size_t count = service->provider.ops.result_count(service->provider.provider);
    if (count > REACH_SEARCH_MAX_RESULTS)
    {
        count = REACH_SEARCH_MAX_RESULTS;
    }

    size_t write_index = 0;
    for (size_t index = 0; index < count; ++index)
    {
        reach_search_result result = {};
        if (service->provider.ops.result_at(service->provider.provider, index, &result) == REACH_OK)
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

static void reach_search_service_thread_main(reach_search_service *service)
{
    for (;;)
    {
        uint32_t generation = 0;
        uint16_t query[REACH_MAX_SEARCH_CHARS + 1] = {};

        {
            std::unique_lock<std::mutex> lock(service->mutex);
            service->cv.wait(lock, [service]() { return service->stop || service->pending; });

            if (service->stop)
            {
                return;
            }

            generation = service->pending_generation;
            reach_copy_utf16(query, REACH_MAX_SEARCH_CHARS + 1, service->pending_query);
            service->pending = 0;
            service->in_flight = 1;
        }

        reach_search_candidate results[REACH_SEARCH_MAX_RESULTS] = {};
        size_t count = 0;
        reach_search_service_query(service, query, results, &count);

        void (*notify)(void *user) = nullptr;
        void *notify_user = nullptr;
        {
            std::lock_guard<std::mutex> lock(service->mutex);
            service->in_flight = 0;
            if (!service->stop)
            {
                service->completed_generation = generation;
                service->completed_count = count;
                for (size_t index = 0; index < REACH_SEARCH_MAX_RESULTS; ++index)
                {
                    service->completed_results[index] = results[index];
                }
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

reach_result reach_search_service_create(reach_search_provider_port provider,
                                         void (*notify)(void *user), void *notify_user,
                                         reach_search_service **out_service)
{
    if (out_service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_search_service *service = new (std::nothrow) reach_search_service();
    if (service == nullptr)
    {
        return REACH_ERROR;
    }
    service->provider = provider;
    service->notify = notify;
    service->notify_user = notify_user;
    *out_service = service;
    return REACH_OK;
}

void reach_search_service_destroy(reach_search_service *service)
{
    if (service == nullptr)
    {
        return;
    }
    reach_search_service_stop(service);
    delete service;
}

void reach_search_service_stop(reach_search_service *service)
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
    service->in_flight = 0;
    service->pending = 0;
    service->completed = 0;
}

static reach_result reach_search_service_start(reach_search_service *service)
{
    if (service->thread_started)
    {
        return REACH_OK;
    }

    service->stop = 0;
    try
    {
        service->thread = std::thread(reach_search_service_thread_main, service);
    }
    catch (...)
    {
        return REACH_ERROR;
    }

    service->thread_started = 1;
    return REACH_OK;
}

reach_result reach_search_service_submit(reach_search_service *service, const uint16_t *query,
                                         uint32_t generation)
{
    if (service == nullptr || query == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = reach_search_service_start(service);
    if (result != REACH_OK)
    {
        return result;
    }

    {
        std::lock_guard<std::mutex> lock(service->mutex);
        service->pending_generation = generation;
        reach_copy_utf16(service->pending_query, REACH_MAX_SEARCH_CHARS + 1, query);
        service->pending = 1;
    }
    service->cv.notify_one();
    return REACH_OK;
}

void reach_search_service_cancel(reach_search_service *service)
{
    if (service == nullptr)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(service->mutex);
    service->pending = 0;
    service->pending_query[0] = 0;
    service->completed = 0;
}

int32_t reach_search_service_take_completed(reach_search_service *service, uint32_t *out_generation,
                                            reach_search_candidate *out_results, size_t *out_count)
{
    if (service == nullptr || out_generation == nullptr || out_results == nullptr ||
        out_count == nullptr)
    {
        return 0;
    }

    std::lock_guard<std::mutex> lock(service->mutex);
    if (!service->completed)
    {
        return 0;
    }
    *out_generation = service->completed_generation;
    *out_count = service->completed_count;
    for (size_t index = 0; index < REACH_SEARCH_MAX_RESULTS; ++index)
    {
        out_results[index] = service->completed_results[index];
    }
    service->completed = 0;
    return 1;
}

int32_t reach_search_service_work_pending(const reach_search_service *service)
{
    if (service == nullptr)
    {
        return 0;
    }
    reach_search_service *mutable_service = const_cast<reach_search_service *>(service);
    std::lock_guard<std::mutex> lock(mutable_service->mutex);
    return mutable_service->pending || mutable_service->in_flight || mutable_service->completed;
}
