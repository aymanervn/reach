#include "reach/services/system_status.h"

#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>

typedef struct reach_system_status_worker
{
    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;
    int32_t thread_started = 0;
    int32_t stop = 0;
    int32_t pending = 0;
    int32_t in_flight = 0;
    uint32_t generation = 0;
    uint32_t pending_generation = 0;
    int32_t completed = 0;
    uint32_t completed_generation = 0;
} reach_system_status_worker;

struct reach_system_status
{
    reach_audio_volume_port audio_volume;
    reach_system_controls_port system_controls;

    void (*notify)(void *user);
    void *notify_user;

    reach_system_status_worker audio;
    reach_system_status_audio_snapshot audio_snapshot = {};

    reach_system_status_worker system;
    uint32_t system_pending_change_flags = 0;
    reach_system_status_system_snapshot system_snapshot = {};
};

static float reach_system_status_clamp01(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    if (value > 1.0f)
    {
        return 1.0f;
    }
    return value;
}

static void reach_system_status_audio_thread_main(reach_system_status *service)
{
    reach_system_status_worker *worker = &service->audio;
    for (;;)
    {
        uint32_t generation = 0;
        {
            std::unique_lock<std::mutex> lock(worker->mutex);
            worker->cv.wait(lock, [worker]() { return worker->stop || worker->pending; });
            if (worker->stop)
            {
                return;
            }
            generation = worker->pending_generation;
            worker->pending = 0;
            worker->in_flight = 1;
        }

        reach_system_status_audio_snapshot snapshot = {};

        if (service->audio_volume.get_state != nullptr &&
            service->audio_volume.get_state(service->audio_volume.userdata, &snapshot.state) ==
                REACH_OK)
        {
            snapshot.state.level = reach_system_status_clamp01(snapshot.state.level);
            snapshot.state.muted = snapshot.state.muted ? 1 : 0;
            snapshot.state_valid = 1;
        }

        if (service->audio_volume.list_sessions != nullptr &&
            service->audio_volume.list_sessions(service->audio_volume.userdata,
                                                &snapshot.sessions) == REACH_OK)
        {
            snapshot.sessions_valid = 1;
        }

        if (service->audio_volume.list_output_devices != nullptr &&
            service->audio_volume.list_output_devices(service->audio_volume.userdata,
                                                      &snapshot.output_devices) == REACH_OK)
        {
            snapshot.output_devices_valid = 1;
        }

        {
            std::lock_guard<std::mutex> lock(worker->mutex);
            service->audio_snapshot = snapshot;
            worker->completed_generation = generation;
            worker->completed = 1;
            worker->in_flight = 0;
        }

        if (service->notify != nullptr)
        {
            service->notify(service->notify_user);
        }
    }
}

static void reach_system_status_system_thread_main(reach_system_status *service)
{
    reach_system_status_worker *worker = &service->system;
    for (;;)
    {
        uint32_t generation = 0;
        uint32_t change_flags = 0;
        {
            std::unique_lock<std::mutex> lock(worker->mutex);
            worker->cv.wait(lock, [worker]() { return worker->stop || worker->pending; });
            if (worker->stop)
            {
                return;
            }
            generation = worker->pending_generation;
            change_flags = service->system_pending_change_flags;
            worker->pending = 0;
            service->system_pending_change_flags = 0;
            worker->in_flight = 1;
        }

        reach_system_status_system_snapshot snapshot = {};
        snapshot.change_flags = change_flags;

        if (service->system_controls.get_network_state != nullptr &&
            service->system_controls.get_network_state(service->system_controls.userdata,
                                                       &snapshot.network) == REACH_OK)
        {
            snapshot.network_valid = 1;
        }
        else
        {
            snapshot.network.kind = REACH_NETWORK_KIND_NONE;
            snapshot.network.connected = 0;
        }

        if (service->system_controls.get_bluetooth_state != nullptr &&
            service->system_controls.get_bluetooth_state(service->system_controls.userdata,
                                                         &snapshot.bluetooth) == REACH_OK)
        {
            snapshot.bluetooth_valid = 1;
        }
        else
        {
            snapshot.bluetooth.available = 0;
            snapshot.bluetooth.enabled = 0;
        }

        if (service->system_controls.get_power_state != nullptr &&
            service->system_controls.get_power_state(service->system_controls.userdata,
                                                     &snapshot.power) == REACH_OK)
        {
            snapshot.power_valid = 1;
        }

        if (service->system_controls.get_brightness_state != nullptr &&
            service->system_controls.get_brightness_state(service->system_controls.userdata,
                                                          &snapshot.brightness) == REACH_OK)
        {
            snapshot.brightness_valid = 1;
        }

        {
            std::lock_guard<std::mutex> lock(worker->mutex);
            service->system_snapshot = snapshot;
            worker->completed_generation = generation;
            worker->completed = 1;
            worker->in_flight = 0;
        }

        if (service->notify != nullptr)
        {
            service->notify(service->notify_user);
        }
    }
}

reach_result reach_system_status_create(reach_audio_volume_port audio_volume,
                                        reach_system_controls_port system_controls,
                                        void (*notify)(void *user), void *notify_user,
                                        reach_system_status **out_service)
{
    if (out_service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_system_status *service = new (std::nothrow) reach_system_status();
    if (service == nullptr)
    {
        return REACH_ERROR;
    }
    service->audio_volume = audio_volume;
    service->system_controls = system_controls;
    service->notify = notify;
    service->notify_user = notify_user;
    *out_service = service;
    return REACH_OK;
}

static void reach_system_status_stop_worker(reach_system_status_worker *worker)
{
    if (!worker->thread_started)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(worker->mutex);
        worker->stop = 1;
        worker->pending = 0;
    }
    worker->cv.notify_one();

    if (worker->thread.joinable())
    {
        worker->thread.join();
    }

    worker->thread_started = 0;
    worker->stop = 0;
    worker->pending = 0;
    worker->in_flight = 0;
    worker->completed = 0;
}

void reach_system_status_stop(reach_system_status *service)
{
    if (service == nullptr)
    {
        return;
    }
    reach_system_status_stop_worker(&service->audio);
    reach_system_status_stop_worker(&service->system);
    service->system_pending_change_flags = 0;
}

void reach_system_status_destroy(reach_system_status *service)
{
    if (service == nullptr)
    {
        return;
    }
    reach_system_status_stop(service);
    delete service;
}

static reach_result reach_system_status_start_worker(reach_system_status *service,
                                                     reach_system_status_worker *worker,
                                                     void (*thread_main)(reach_system_status *))
{
    if (worker->thread_started)
    {
        return REACH_OK;
    }
    worker->stop = 0;
    try
    {
        worker->thread = std::thread(thread_main, service);
    }
    catch (...)
    {
        return REACH_ERROR;
    }
    worker->thread_started = 1;
    return REACH_OK;
}

void reach_system_status_refresh_audio(reach_system_status *service)
{
    if (service == nullptr ||
        reach_system_status_start_worker(service, &service->audio,
                                         reach_system_status_audio_thread_main) != REACH_OK)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(service->audio.mutex);
        ++service->audio.generation;
        service->audio.pending_generation = service->audio.generation;
        service->audio.pending = 1;
    }
    service->audio.cv.notify_one();
}

void reach_system_status_refresh_system(reach_system_status *service, uint32_t change_flags)
{
    if (service == nullptr ||
        reach_system_status_start_worker(service, &service->system,
                                         reach_system_status_system_thread_main) != REACH_OK)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(service->system.mutex);
        ++service->system.generation;
        service->system.pending_generation = service->system.generation;
        service->system_pending_change_flags |= change_flags;
        service->system.pending = 1;
    }
    service->system.cv.notify_one();
}

int32_t reach_system_status_take_audio(reach_system_status *service,
                                       reach_system_status_audio_snapshot *out_snapshot)
{
    if (service == nullptr || out_snapshot == nullptr)
    {
        return 0;
    }

    std::lock_guard<std::mutex> lock(service->audio.mutex);
    if (!service->audio.completed)
    {
        return 0;
    }
    service->audio.completed = 0;
    if (service->audio.completed_generation != service->audio.generation)
    {
        return 0;
    }
    *out_snapshot = service->audio_snapshot;
    return 1;
}

int32_t reach_system_status_take_system(reach_system_status *service,
                                        reach_system_status_system_snapshot *out_snapshot)
{
    if (service == nullptr || out_snapshot == nullptr)
    {
        return 0;
    }

    std::lock_guard<std::mutex> lock(service->system.mutex);
    if (!service->system.completed)
    {
        return 0;
    }
    service->system.completed = 0;
    if (service->system.completed_generation != service->system.generation)
    {
        return 0;
    }
    *out_snapshot = service->system_snapshot;
    return 1;
}

static int32_t reach_system_status_worker_pending(const reach_system_status_worker *worker)
{
    reach_system_status_worker *mutable_worker =
        const_cast<reach_system_status_worker *>(worker);
    std::lock_guard<std::mutex> lock(mutable_worker->mutex);
    return mutable_worker->pending || mutable_worker->in_flight || mutable_worker->completed;
}

int32_t reach_system_status_audio_pending(const reach_system_status *service)
{
    return service != nullptr && reach_system_status_worker_pending(&service->audio);
}

int32_t reach_system_status_system_pending(const reach_system_status *service)
{
    return service != nullptr && reach_system_status_worker_pending(&service->system);
}
