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
    int32_t brightness_target_valid = 0;
    float brightness_target = 0.0f;
    uint32_t brightness_target_generation = 0;
    float brightness_delta = 0.0f;
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
        int32_t brightness_target_valid = 0;
        float brightness_target = 0.0f;
        uint32_t brightness_target_generation = 0;
        float brightness_delta = 0.0f;
        reach_system_status_system_snapshot snapshot = {};
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
            brightness_target_valid = service->brightness_target_valid;
            brightness_target = service->brightness_target;
            brightness_target_generation = service->brightness_target_generation;
            brightness_delta = service->brightness_delta;
            service->brightness_delta = 0.0f;
            snapshot = service->system_snapshot;
        }

        snapshot.change_flags |= change_flags;

        if ((change_flags & REACH_SYSTEM_CONTROLS_CHANGE_NETWORK) != 0 &&
            service->system_controls.get_network_state != nullptr)
        {
            reach_network_state network = {};
            if (service->system_controls.get_network_state(service->system_controls.userdata,
                                                           &network) == REACH_OK)
            {
                snapshot.network = network;
                snapshot.network_valid = 1;
            }
        }

        if ((change_flags & REACH_SYSTEM_CONTROLS_CHANGE_BLUETOOTH) != 0 &&
            service->system_controls.get_bluetooth_state != nullptr)
        {
            reach_bluetooth_state bluetooth = {};
            if (service->system_controls.get_bluetooth_state(service->system_controls.userdata,
                                                             &bluetooth) == REACH_OK)
            {
                snapshot.bluetooth = bluetooth;
                snapshot.bluetooth_valid = 1;
            }
        }

        if ((change_flags & REACH_SYSTEM_CONTROLS_CHANGE_POWER) != 0 &&
            service->system_controls.get_power_state != nullptr)
        {
            reach_power_state power = {};
            if (service->system_controls.get_power_state(service->system_controls.userdata,
                                                         &power) == REACH_OK)
            {
                snapshot.power = power;
                snapshot.power_valid = 1;
            }
        }

        if (brightness_target_valid || brightness_delta != 0.0f)
        {
            reach_brightness_state brightness = snapshot.brightness;
            int32_t brightness_valid = snapshot.brightness_valid;
            float target = brightness_target;
            if (!brightness_target_valid)
            {
                if ((!brightness_valid || !brightness.available) &&
                    service->system_controls.get_brightness_state != nullptr)
                {
                    brightness = {};
                    brightness_valid =
                        service->system_controls.get_brightness_state(
                            service->system_controls.userdata, &brightness) == REACH_OK;
                }
                if (brightness_valid && brightness.available)
                {
                    target = reach_system_status_clamp01(brightness.level + brightness_delta);
                    brightness_target_valid = 1;
                }
            }
            if (brightness_target_valid &&
                service->system_controls.set_brightness_level != nullptr &&
                service->system_controls.set_brightness_level(service->system_controls.userdata,
                                                              target) == REACH_OK)
            {
                brightness.available = 1;
                brightness.level = target;
                snapshot.brightness = brightness;
                snapshot.brightness_valid = 1;
            }
        }
        else if ((change_flags & REACH_SYSTEM_CONTROLS_CHANGE_BRIGHTNESS) != 0 &&
                 service->system_controls.get_brightness_state != nullptr)
        {
            reach_brightness_state brightness = {};
            if (service->system_controls.get_brightness_state(service->system_controls.userdata,
                                                              &brightness) == REACH_OK)
            {
                snapshot.brightness = brightness;
                snapshot.brightness_valid = 1;
            }
        }

        {
            std::lock_guard<std::mutex> lock(worker->mutex);
            service->system_snapshot = snapshot;
            if (service->brightness_target_valid &&
                service->brightness_target_generation == brightness_target_generation)
            {
                service->brightness_target_valid = 0;
            }
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
    service->brightness_target_valid = 0;
    service->brightness_target = 0.0f;
    service->brightness_target_generation = 0;
    service->brightness_delta = 0.0f;
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
        service->system_pending_change_flags |=
            change_flags != 0 ? change_flags : (uint32_t)REACH_SYSTEM_CONTROLS_CHANGE_ALL;
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
    service->system_snapshot.change_flags = 0;
    return 1;
}

void reach_system_status_read_audio(const reach_system_status *service,
                                    reach_system_status_audio_snapshot *out_snapshot)
{
    if (out_snapshot == nullptr)
    {
        return;
    }
    *out_snapshot = {};
    if (service == nullptr)
    {
        return;
    }

    reach_system_status *mutable_service = const_cast<reach_system_status *>(service);
    std::lock_guard<std::mutex> lock(mutable_service->audio.mutex);
    *out_snapshot = mutable_service->audio_snapshot;
}

void reach_system_status_read_system(const reach_system_status *service,
                                     reach_system_status_system_snapshot *out_snapshot)
{
    if (out_snapshot == nullptr)
    {
        return;
    }
    *out_snapshot = {};
    if (service == nullptr)
    {
        return;
    }

    reach_system_status *mutable_service = const_cast<reach_system_status *>(service);
    std::lock_guard<std::mutex> lock(mutable_service->system.mutex);
    *out_snapshot = mutable_service->system_snapshot;
}

static int32_t reach_system_status_worker_pending(const reach_system_status_worker *worker)
{
    reach_system_status_worker *mutable_worker = const_cast<reach_system_status_worker *>(worker);
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

reach_result reach_system_status_set_main_volume(reach_system_status *service, float level,
                                                 int32_t *in_out_muted)
{
    if (service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    level = reach_system_status_clamp01(level);

    int32_t muted = in_out_muted != nullptr ? *in_out_muted : 0;
    if (service->audio_volume.get_state != nullptr)
    {
        reach_audio_volume_state state = {};
        if (service->audio_volume.get_state(service->audio_volume.userdata, &state) == REACH_OK)
        {
            muted = state.muted ? 1 : 0;
        }
    }

    if (muted && service->audio_volume.set_muted != nullptr &&
        service->audio_volume.set_muted(service->audio_volume.userdata, 0) == REACH_OK)
    {
        muted = 0;
    }

    if (in_out_muted != nullptr)
    {
        *in_out_muted = muted;
    }

    return service->audio_volume.set_level != nullptr
               ? service->audio_volume.set_level(service->audio_volume.userdata, level)
               : REACH_NOT_IMPLEMENTED;
}

reach_result reach_system_status_set_session_volume(reach_system_status *service,
                                                    const uint16_t *session_instance_id,
                                                    float level)
{
    if (service == nullptr || session_instance_id == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (service->audio_volume.set_session_level == nullptr)
    {
        return REACH_NOT_IMPLEMENTED;
    }
    return service->audio_volume.set_session_level(
        service->audio_volume.userdata, session_instance_id, reach_system_status_clamp01(level));
}

reach_result reach_system_status_set_default_output_device(reach_system_status *service,
                                                           const uint16_t *device_id)
{
    if (service == nullptr || device_id == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (service->audio_volume.set_default_output_device == nullptr)
    {
        return REACH_NOT_IMPLEMENTED;
    }
    return service->audio_volume.set_default_output_device(service->audio_volume.userdata,
                                                           device_id);
}

reach_result reach_system_status_set_brightness(reach_system_status *service, float level)
{
    if (service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (service->system_controls.set_brightness_level == nullptr)
    {
        return REACH_NOT_IMPLEMENTED;
    }
    if (reach_system_status_start_worker(service, &service->system,
                                         reach_system_status_system_thread_main) != REACH_OK)
    {
        return REACH_ERROR;
    }

    {
        std::lock_guard<std::mutex> lock(service->system.mutex);
        ++service->system.generation;
        service->system.pending_generation = service->system.generation;
        service->brightness_target_valid = 1;
        service->brightness_target = reach_system_status_clamp01(level);
        service->brightness_target_generation = service->system.generation;
        service->brightness_delta = 0.0f;
        service->system_pending_change_flags |= REACH_SYSTEM_CONTROLS_CHANGE_BRIGHTNESS;
        service->system.pending = 1;
    }
    service->system.cv.notify_one();
    return REACH_OK;
}

reach_result reach_system_status_step_brightness(reach_system_status *service, float delta,
                                                 reach_brightness_state *out_state)
{
    if (out_state != nullptr)
    {
        *out_state = {};
    }
    if (service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (service->system_controls.set_brightness_level == nullptr)
    {
        return REACH_NOT_IMPLEMENTED;
    }
    if (reach_system_status_start_worker(service, &service->system,
                                         reach_system_status_system_thread_main) != REACH_OK)
    {
        return REACH_ERROR;
    }

    {
        std::lock_guard<std::mutex> lock(service->system.mutex);
        reach_brightness_state next = service->system_snapshot.brightness;
        int32_t next_valid = service->system_snapshot.brightness_valid && next.available;
        if (service->brightness_target_valid)
        {
            next.available = 1;
            next.level = service->brightness_target;
            next_valid = 1;
        }
        if (!next_valid && service->system_controls.get_brightness_state == nullptr)
        {
            return REACH_NOT_IMPLEMENTED;
        }

        ++service->system.generation;
        service->system.pending_generation = service->system.generation;
        if (next_valid)
        {
            service->brightness_target_valid = 1;
            service->brightness_target =
                reach_system_status_clamp01(next.level + delta + service->brightness_delta);
            service->brightness_target_generation = service->system.generation;
            service->brightness_delta = 0.0f;
            next.level = service->brightness_target;
            if (out_state != nullptr)
            {
                *out_state = next;
            }
        }
        else
        {
            service->brightness_delta += delta;
        }
        service->system_pending_change_flags |= REACH_SYSTEM_CONTROLS_CHANGE_BRIGHTNESS;
        service->system.pending = 1;
    }
    service->system.cv.notify_one();
    return REACH_OK;
}

reach_system_status_bluetooth_outcome
reach_system_status_set_bluetooth_enabled(reach_system_status *service, int32_t enabled)
{
    if (service == nullptr)
    {
        return REACH_SYSTEM_STATUS_BLUETOOTH_UNSUPPORTED;
    }

    enabled = enabled ? 1 : 0;

    if (service->system_controls.request_bluetooth_enabled != nullptr)
    {
        return service->system_controls.request_bluetooth_enabled(service->system_controls.userdata,
                                                                  enabled) == REACH_OK
                   ? REACH_SYSTEM_STATUS_BLUETOOTH_PENDING
                   : REACH_SYSTEM_STATUS_BLUETOOTH_REJECTED;
    }

    if (service->system_controls.set_bluetooth_enabled != nullptr)
    {
        (void)service->system_controls.set_bluetooth_enabled(service->system_controls.userdata,
                                                             enabled);
        return REACH_SYSTEM_STATUS_BLUETOOTH_APPLIED;
    }

    return REACH_SYSTEM_STATUS_BLUETOOTH_UNSUPPORTED;
}

reach_result reach_system_status_set_battery_saver_enabled(reach_system_status *service,
                                                           int32_t enabled)
{
    if (service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (service->system_controls.set_battery_saver_enabled == nullptr)
    {
        return REACH_NOT_IMPLEMENTED;
    }
    return service->system_controls.set_battery_saver_enabled(service->system_controls.userdata,
                                                              enabled ? 1 : 0);
}

reach_result reach_system_status_open_system_quick_settings(reach_system_status *service)
{
    if (service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (service->system_controls.open_system_quick_settings == nullptr)
    {
        return REACH_NOT_IMPLEMENTED;
    }
    return service->system_controls.open_system_quick_settings(service->system_controls.userdata);
}

reach_result reach_system_status_open_project_menu(reach_system_status *service)
{
    if (service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (service->system_controls.open_project_menu == nullptr)
    {
        return REACH_NOT_IMPLEMENTED;
    }
    return service->system_controls.open_project_menu(service->system_controls.userdata);
}
