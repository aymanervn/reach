#include "reach/services/system_stats.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>

#define REACH_SYSTEM_STATS_INTERVAL_SECONDS 1.0

struct reach_system_stats
{
    reach_system_stats_port source;
    reach_system_controls_port system_controls;

    void (*notify)(void *user);
    void *notify_user;

    std::thread thread;
    mutable std::mutex mutex;
    std::condition_variable cv;
    int32_t thread_started = 0;
    int32_t stop = 0;
    int32_t enabled = 0;
    int32_t rebaseline = 0;
    int32_t changed = 0;

    reach_system_stats_snapshot snapshot = {};
};

static uint64_t reach_system_stats_delta(uint64_t current, uint64_t previous)
{
    return current > previous ? current - previous : 0;
}

static float reach_system_stats_percent(uint64_t used, uint64_t total)
{
    if (total == 0)
    {
        return 0.0f;
    }
    float percent = (float)((double)used / (double)total * 100.0);
    if (percent < 0.0f)
    {
        return 0.0f;
    }
    return percent > 100.0f ? 100.0f : percent;
}

static int32_t reach_system_stats_snapshot_differs(const reach_system_stats_snapshot *a,
                                                   const reach_system_stats_snapshot *b)
{
    return a->valid != b->valid || (int32_t)a->cpu_percent != (int32_t)b->cpu_percent ||
           (int32_t)a->memory_percent != (int32_t)b->memory_percent ||
           a->network_received_bytes_per_second != b->network_received_bytes_per_second ||
           a->network_sent_bytes_per_second != b->network_sent_bytes_per_second ||
           a->power_valid != b->power_valid || a->power.has_battery != b->power.has_battery ||
           a->power.battery_percent != b->power.battery_percent ||
           a->power.battery_saver_on != b->power.battery_saver_on ||
           a->power.battery_charging != b->power.battery_charging;
}

static void reach_system_stats_thread_main(reach_system_stats *service)
{
    const auto interval = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(REACH_SYSTEM_STATS_INTERVAL_SECONDS));

    reach_system_stats_snapshot published = {};
    reach_system_stats_sample previous = {};
    int32_t has_previous = 0;
    auto previous_time = std::chrono::steady_clock::now();

    for (;;)
    {
        {
            std::unique_lock<std::mutex> lock(service->mutex);
            if (!service->stop && !service->enabled)
            {
                service->cv.wait(lock,
                                 [service]() { return service->stop || service->enabled; });
            }
            else if (!service->stop)
            {
                service->cv.wait_for(lock, interval, [service]() {
                    return service->stop || !service->enabled;
                });
            }
            if (service->stop)
            {
                return;
            }
            if (!service->enabled)
            {
                continue;
            }
            if (service->rebaseline)
            {
                service->rebaseline = 0;
                has_previous = 0;
            }
        }

        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - previous_time).count();
        previous_time = now;

        reach_system_stats_snapshot next = published;
        next.power = reach_power_state{};
        next.power_valid = 0;
        if (service->system_controls.get_power_state != nullptr &&
            service->system_controls.get_power_state(service->system_controls.userdata,
                                                     &next.power) == REACH_OK)
        {
            next.power_valid = 1;
        }

        reach_system_stats_sample sample = {};
        if (service->source.ops.sample != nullptr &&
            service->source.ops.sample(service->source.source, &sample) == REACH_OK)
        {
            next.memory_percent =
                reach_system_stats_percent(sample.memory_used_bytes, sample.memory_total_bytes);

            if (has_previous)
            {
                uint64_t total_delta =
                    reach_system_stats_delta(sample.cpu_total_time, previous.cpu_total_time);
                uint64_t idle_delta =
                    reach_system_stats_delta(sample.cpu_idle_time, previous.cpu_idle_time);
                uint64_t busy_delta = total_delta > idle_delta ? total_delta - idle_delta : 0;
                next.cpu_percent = reach_system_stats_percent(busy_delta, total_delta);

                if (elapsed > 0.0)
                {
                    uint64_t received = reach_system_stats_delta(sample.network_received_bytes,
                                                                 previous.network_received_bytes);
                    uint64_t sent = reach_system_stats_delta(sample.network_sent_bytes,
                                                             previous.network_sent_bytes);
                    next.network_received_bytes_per_second = (uint64_t)((double)received / elapsed);
                    next.network_sent_bytes_per_second = (uint64_t)((double)sent / elapsed);
                }
                next.valid = 1;
            }

            previous = sample;
            has_previous = 1;
        }

        int32_t differs = reach_system_stats_snapshot_differs(&next, &published);
        published = next;

        {
            std::lock_guard<std::mutex> lock(service->mutex);
            service->snapshot = next;
            if (differs)
            {
                service->changed = 1;
            }
        }

        if (differs && service->notify != nullptr)
        {
            service->notify(service->notify_user);
        }
    }
}

reach_result reach_system_stats_create(reach_system_stats_port source,
                                       reach_system_controls_port system_controls,
                                       void (*notify)(void *user), void *notify_user,
                                       reach_system_stats **out_service)
{
    if (out_service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_system_stats *service = new (std::nothrow) reach_system_stats();
    if (service == nullptr)
    {
        return REACH_ERROR;
    }

    service->source = source;
    service->system_controls = system_controls;
    service->notify = notify;
    service->notify_user = notify_user;

    try
    {
        service->thread = std::thread(reach_system_stats_thread_main, service);
    }
    catch (...)
    {
        delete service;
        return REACH_ERROR;
    }
    service->thread_started = 1;

    *out_service = service;
    return REACH_OK;
}

void reach_system_stats_stop(reach_system_stats *service)
{
    if (service == nullptr || !service->thread_started)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(service->mutex);
        service->stop = 1;
    }
    service->cv.notify_one();

    if (service->thread.joinable())
    {
        service->thread.join();
    }
    service->thread_started = 0;
}

void reach_system_stats_destroy(reach_system_stats *service)
{
    if (service == nullptr)
    {
        return;
    }
    reach_system_stats_stop(service);
    if (service->source.ops.destroy != nullptr)
    {
        service->source.ops.destroy(service->source.source);
    }
    delete service;
}

void reach_system_stats_set_enabled(reach_system_stats *service, int32_t enabled)
{
    if (service == nullptr)
    {
        return;
    }

    enabled = enabled ? 1 : 0;
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        if (service->enabled == enabled)
        {
            return;
        }
        service->enabled = enabled;
        if (enabled)
        {
            service->rebaseline = 1;
        }
    }
    service->cv.notify_one();
}

int32_t reach_system_stats_take_changed(reach_system_stats *service)
{
    if (service == nullptr)
    {
        return 0;
    }
    std::lock_guard<std::mutex> lock(service->mutex);
    int32_t changed = service->changed;
    service->changed = 0;
    return changed;
}

void reach_system_stats_snapshot_take(const reach_system_stats *service,
                                      reach_system_stats_snapshot *out_snapshot)
{
    if (out_snapshot == nullptr)
    {
        return;
    }
    if (service == nullptr)
    {
        *out_snapshot = reach_system_stats_snapshot{};
        return;
    }
    std::lock_guard<std::mutex> lock(service->mutex);
    *out_snapshot = service->snapshot;
}
