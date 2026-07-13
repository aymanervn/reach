#include "reach/services/icon_service.h"

#include "reach/support/util.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <vector>

static const double REACH_ICON_FAILURE_COOLDOWN_SECONDS = 30.0;

typedef struct reach_icon_cache_entry
{
    std::wstring path;
    int32_t size_px = 0;
    uint64_t id = 0;
    reach_icon_handle handle = {};
    int32_t loading = 0;
    double last_used = 0.0;
    double failed_until = 0.0;
} reach_icon_cache_entry;

typedef struct reach_icon_load_job
{
    uint16_t path[260];
    int32_t size_px;
} reach_icon_load_job;

struct reach_icon_service
{
    reach_icon_provider_port provider;

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<reach_icon_cache_entry> entries;
    std::vector<reach_icon_load_job> jobs;
    std::vector<uint64_t> evicted;

    std::thread thread;
    int32_t thread_started = 0;
    int32_t stop = 0;
    int32_t in_flight = 0;

    int32_t loads_completed = 0;

    void (*notify)(void *user) = nullptr;
    void *notify_user = nullptr;
};

static double reach_icon_service_now(void)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

static int32_t reach_icon_path_matches(const std::wstring &cached, const uint16_t *path)
{
    size_t index = 0;
    for (; index < cached.size(); ++index)
    {
        if (path[index] == 0 || (uint16_t)cached[index] != path[index])
        {
            return 0;
        }
    }
    return path[index] == 0;
}

static reach_icon_cache_entry *reach_icon_service_find(reach_icon_service *service,
                                                       const uint16_t *path, int32_t size_px)
{
    for (reach_icon_cache_entry &entry : service->entries)
    {
        if (entry.size_px == size_px && reach_icon_path_matches(entry.path, path))
        {
            return &entry;
        }
    }
    return nullptr;
}

static void reach_icon_service_thread_main(reach_icon_service *service)
{
    for (;;)
    {
        reach_icon_load_job job = {};
        {
            std::unique_lock<std::mutex> lock(service->mutex);
            service->cv.wait(lock,
                             [service]() { return service->stop || !service->jobs.empty(); });

            if (service->stop)
            {
                return;
            }

            job = service->jobs.front();
            service->jobs.erase(service->jobs.begin());
            service->in_flight = 1;
        }

        reach_icon_request request = {};
        reach_copy_utf16(request.path, sizeof(request.path) / sizeof(request.path[0]), job.path);
        request.size_px = job.size_px;
        reach_icon_handle handle = {};
        reach_result result = service->provider.ops.load != nullptr
                                  ? service->provider.ops.load(service->provider.provider,
                                                               &request, &handle)
                                  : REACH_ERROR;

        void (*notify)(void *user) = nullptr;
        void *notify_user = nullptr;
        {
            std::lock_guard<std::mutex> lock(service->mutex);
            service->in_flight = 0;
            reach_icon_cache_entry *entry =
                reach_icon_service_find(service, job.path, job.size_px);
            if (service->stop || entry == nullptr)
            {

                if (result == REACH_OK && handle.id != 0 &&
                    service->provider.ops.release != nullptr)
                {
                    service->provider.ops.release(service->provider.provider, handle);
                }
                if (service->stop)
                {
                    return;
                }
                continue;
            }

            entry->loading = 0;
            double now = reach_icon_service_now();
            if (result == REACH_OK && handle.id != 0)
            {
                entry->handle = handle;
                entry->id = handle.id;
                entry->last_used = now;
            }
            else
            {

                entry->failed_until = now + REACH_ICON_FAILURE_COOLDOWN_SECONDS;
            }
            service->loads_completed = 1;
            notify = service->notify;
            notify_user = service->notify_user;
        }

        if (notify != nullptr)
        {
            notify(notify_user);
        }
    }
}

reach_result reach_icon_service_create(reach_icon_provider_port provider,
                                       reach_icon_service **out_service)
{
    if (out_service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_service = nullptr;

    reach_icon_service *service = new (std::nothrow) reach_icon_service();
    if (service == nullptr)
    {
        return REACH_ERROR;
    }

    service->provider = provider;
    *out_service = service;
    return REACH_OK;
}

void reach_icon_service_stop(reach_icon_service *service)
{
    if (service == nullptr || !service->thread_started)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(service->mutex);
        service->stop = 1;
        service->jobs.clear();
    }
    service->cv.notify_one();

    if (service->thread.joinable())
    {
        service->thread.join();
    }

    service->thread_started = 0;
    service->stop = 0;
    service->in_flight = 0;
}

void reach_icon_service_destroy(reach_icon_service *service)
{
    if (service == nullptr)
    {
        return;
    }

    reach_icon_service_stop(service);
    reach_icon_service_clear(service);

    if (service->provider.ops.destroy != nullptr)
    {
        service->provider.ops.destroy(service->provider.provider);
    }

    delete service;
}

void reach_icon_service_set_notify(reach_icon_service *service, void (*notify)(void *user),
                                   void *notify_user)
{
    if (service == nullptr)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(service->mutex);
    service->notify = notify;
    service->notify_user = notify_user;
}

static reach_result reach_icon_service_start_worker(reach_icon_service *service)
{
    if (service->thread_started)
    {
        return REACH_OK;
    }
    service->stop = 0;
    try
    {
        service->thread = std::thread(reach_icon_service_thread_main, service);
    }
    catch (...)
    {
        return REACH_ERROR;
    }
    service->thread_started = 1;
    return REACH_OK;
}

uint64_t reach_icon_service_get(reach_icon_service *service, const uint16_t *path,
                                int32_t size_px)
{
    if (service == nullptr || path == nullptr || path[0] == 0 ||
        service->provider.ops.load == nullptr)
    {
        return 0;
    }

    int32_t queued = 0;
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        double now = reach_icon_service_now();
        reach_icon_cache_entry *entry = reach_icon_service_find(service, path, size_px);
        if (entry != nullptr)
        {
            if (entry->id != 0)
            {
                entry->last_used = now;
                return entry->id;
            }
            if (entry->loading || entry->failed_until > now)
            {
                return 0;
            }

            entry->loading = 1;
            entry->failed_until = 0.0;
        }
        else
        {
            size_t length = 0;
            while (path[length] != 0)
            {
                ++length;
            }
            reach_icon_cache_entry fresh;
            fresh.path.assign(path, path + length);
            fresh.size_px = size_px;
            fresh.loading = 1;
            fresh.last_used = now;
            service->entries.push_back(std::move(fresh));
        }

        reach_icon_load_job job = {};
        reach_copy_utf16(job.path, 260, path);
        job.size_px = size_px;
        service->jobs.push_back(job);
        queued = 1;
    }

    if (queued)
    {
        if (reach_icon_service_start_worker(service) != REACH_OK)
        {
            std::lock_guard<std::mutex> lock(service->mutex);
            service->jobs.clear();
        }
        service->cv.notify_one();
    }
    return 0;
}

void reach_icon_service_touch(reach_icon_service *service, const uint16_t *path,
                              int32_t size_px)
{
    if (service == nullptr || path == nullptr || path[0] == 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(service->mutex);
    reach_icon_cache_entry *entry = reach_icon_service_find(service, path, size_px);
    if (entry != nullptr && entry->id != 0)
    {
        entry->last_used = reach_icon_service_now();
    }
}

void reach_icon_service_trim(reach_icon_service *service, double max_age_seconds)
{
    if (service == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(service->mutex);
    double now = reach_icon_service_now();
    for (size_t index = service->entries.size(); index > 0; --index)
    {
        reach_icon_cache_entry &entry = service->entries[index - 1];
        if (entry.loading || now - entry.last_used <= max_age_seconds)
        {
            continue;
        }
        if (entry.id != 0)
        {
            if (service->provider.ops.release != nullptr)
            {
                service->provider.ops.release(service->provider.provider, entry.handle);
            }
            service->evicted.push_back(entry.id);
        }
        service->entries.erase(service->entries.begin() + static_cast<ptrdiff_t>(index - 1));
    }
}

size_t reach_icon_service_take_evicted(reach_icon_service *service, uint64_t *out_icon_ids,
                                       size_t capacity)
{
    if (service == nullptr || out_icon_ids == nullptr || capacity == 0)
    {
        return 0;
    }

    std::lock_guard<std::mutex> lock(service->mutex);
    size_t taken = service->evicted.size() < capacity ? service->evicted.size() : capacity;
    for (size_t index = 0; index < taken; ++index)
    {
        out_icon_ids[index] = service->evicted[index];
    }
    service->evicted.erase(service->evicted.begin(),
                           service->evicted.begin() + static_cast<ptrdiff_t>(taken));
    return taken;
}

int32_t reach_icon_service_take_loads_completed(reach_icon_service *service)
{
    if (service == nullptr)
    {
        return 0;
    }
    std::lock_guard<std::mutex> lock(service->mutex);
    int32_t completed = service->loads_completed;
    service->loads_completed = 0;
    return completed;
}

int32_t reach_icon_service_work_pending(const reach_icon_service *service)
{
    if (service == nullptr)
    {
        return 0;
    }
    reach_icon_service *mutable_service = const_cast<reach_icon_service *>(service);
    std::lock_guard<std::mutex> lock(mutable_service->mutex);
    return !mutable_service->jobs.empty() || mutable_service->in_flight ||
           !mutable_service->evicted.empty();
}

void reach_icon_service_clear(reach_icon_service *service)
{
    if (service == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(service->mutex);
    for (reach_icon_cache_entry &entry : service->entries)
    {
        if (entry.id != 0 && service->provider.ops.release != nullptr)
        {
            service->provider.ops.release(service->provider.provider, entry.handle);
        }
    }
    service->entries.clear();
    service->jobs.clear();

    service->evicted.clear();
}
