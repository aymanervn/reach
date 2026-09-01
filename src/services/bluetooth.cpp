#include "reach/services/bluetooth.h"

#include "reach/support/util.h"

#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>

struct reach_bluetooth_service
{
    reach_bluetooth_port port;
    void (*notify)(void *user);
    void *notify_user;

    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;
    int32_t thread_started = 0;
    int32_t stop = 0;
    int32_t in_flight = 0;
    int32_t completed = 0;
    uint32_t generation = 0;
    uint32_t pending_generation = 0;
    uint32_t completed_generation = 0;

    reach_bluetooth_service_command pending_command = REACH_BLUETOOTH_SERVICE_COMMAND_NONE;
    uint16_t pending_device_id[REACH_BLUETOOTH_DEVICE_ID_CAPACITY] = {};
    int32_t pending_flag = 0;
    int32_t scanning = 0;

    reach_bluetooth_snapshot snapshot = {};
};

static void reach_bluetooth_service_notify(reach_bluetooth_service *service)
{
    if (service->notify != nullptr)
    {
        service->notify(service->notify_user);
    }
}

static void reach_bluetooth_service_on_change(void *user, uint32_t change_flags)
{
    reach_bluetooth_service *service = static_cast<reach_bluetooth_service *>(user);
    if (service == nullptr || change_flags == 0)
    {
        return;
    }
    reach_bluetooth_service_refresh(service);
}

static void reach_bluetooth_service_read_state(reach_bluetooth_service *service,
                                               reach_bluetooth_snapshot *snapshot)
{
    if (service->port.read_devices != nullptr)
    {
        (void)service->port.read_devices(service->port.userdata, &snapshot->devices);
    }
    if (service->port.read_pairing_request != nullptr)
    {
        (void)service->port.read_pairing_request(service->port.userdata, &snapshot->pairing);
    }
    if (service->port.take_pair_result != nullptr)
    {
        (void)service->port.take_pair_result(service->port.userdata, snapshot->pair_device_id,
                                             REACH_BLUETOOTH_DEVICE_ID_CAPACITY,
                                             &snapshot->pair_result);
    }
    snapshot->scanning = service->scanning;
}

static void reach_bluetooth_service_thread_main(reach_bluetooth_service *service)
{
    if (service->port.thread_attach != nullptr)
    {
        service->port.thread_attach(service->port.userdata);
    }

    for (;;)
    {
        reach_bluetooth_service_command command = REACH_BLUETOOTH_SERVICE_COMMAND_NONE;
        uint16_t device_id[REACH_BLUETOOTH_DEVICE_ID_CAPACITY] = {};
        int32_t flag = 0;
        uint32_t generation = 0;
        {
            std::unique_lock<std::mutex> lock(service->mutex);
            service->cv.wait(lock,
                             [service]()
                             {
                                 return service->stop || service->pending_command !=
                                                             REACH_BLUETOOTH_SERVICE_COMMAND_NONE;
                             });
            if (service->stop)
            {
                break;
            }
            command = service->pending_command;
            reach_copy_utf16(device_id, REACH_BLUETOOTH_DEVICE_ID_CAPACITY,
                             service->pending_device_id);
            flag = service->pending_flag;
            generation = service->pending_generation;
            service->pending_command = REACH_BLUETOOTH_SERVICE_COMMAND_NONE;
            service->in_flight = 1;
        }

        reach_bluetooth_snapshot snapshot = {};
        snapshot.completed_command = command;
        snapshot.command_succeeded = 1;

        switch (command)
        {
        case REACH_BLUETOOTH_SERVICE_COMMAND_SET_SCAN:
            snapshot.command_succeeded =
                service->port.set_scan_enabled != nullptr &&
                service->port.set_scan_enabled(service->port.userdata, flag) == REACH_OK;
            if (snapshot.command_succeeded)
            {
                service->scanning = flag;
            }
            break;
        case REACH_BLUETOOTH_SERVICE_COMMAND_PAIR:
            snapshot.command_succeeded =
                service->port.pair != nullptr &&
                service->port.pair(service->port.userdata, device_id) == REACH_OK;
            break;
        case REACH_BLUETOOTH_SERVICE_COMMAND_RESPOND_PAIRING:
            snapshot.command_succeeded =
                service->port.respond_pairing != nullptr &&
                service->port.respond_pairing(service->port.userdata, flag) == REACH_OK;
            break;
        case REACH_BLUETOOTH_SERVICE_COMMAND_UNPAIR:
            snapshot.command_succeeded =
                service->port.unpair != nullptr &&
                service->port.unpair(service->port.userdata, device_id) == REACH_OK;
            break;
        case REACH_BLUETOOTH_SERVICE_COMMAND_REFRESH:
        default:
            break;
        }

        reach_bluetooth_service_read_state(service, &snapshot);

        {
            std::lock_guard<std::mutex> lock(service->mutex);
            service->snapshot = snapshot;
            service->completed_generation = generation;
            service->completed = 1;
            service->in_flight = 0;
        }
        reach_bluetooth_service_notify(service);
    }

    if (service->port.thread_detach != nullptr)
    {
        service->port.thread_detach(service->port.userdata);
    }
}

static reach_result reach_bluetooth_service_start_thread(reach_bluetooth_service *service)
{
    if (service->thread_started)
    {
        return REACH_OK;
    }
    service->stop = 0;
    try
    {
        service->thread = std::thread(reach_bluetooth_service_thread_main, service);
    }
    catch (...)
    {
        return REACH_ERROR;
    }
    service->thread_started = 1;
    return REACH_OK;
}

static void reach_bluetooth_service_submit(reach_bluetooth_service *service,
                                           reach_bluetooth_service_command command,
                                           const uint16_t *device_id, int32_t flag)
{
    if (service == nullptr || reach_bluetooth_service_start_thread(service) != REACH_OK)
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        /* A refresh raised by an OS change must never displace a queued user action. */
        if (command == REACH_BLUETOOTH_SERVICE_COMMAND_REFRESH &&
            service->pending_command != REACH_BLUETOOTH_SERVICE_COMMAND_NONE)
        {
            return;
        }
        ++service->generation;
        service->pending_generation = service->generation;
        service->pending_command = command;
        service->pending_flag = flag;
        service->pending_device_id[0] = 0;
        if (device_id != nullptr)
        {
            reach_copy_utf16(service->pending_device_id, REACH_BLUETOOTH_DEVICE_ID_CAPACITY,
                             device_id);
        }
    }
    service->cv.notify_one();
}

reach_result reach_bluetooth_service_create(reach_bluetooth_port port, void (*notify)(void *user),
                                            void *notify_user,
                                            reach_bluetooth_service **out_service)
{
    if (out_service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_service = nullptr;

    reach_bluetooth_service *service = new (std::nothrow) reach_bluetooth_service();
    if (service == nullptr)
    {
        return REACH_ERROR;
    }
    service->port = port;
    service->notify = notify;
    service->notify_user = notify_user;

    if (service->port.start_watching != nullptr)
    {
        (void)service->port.start_watching(service->port.userdata,
                                           reach_bluetooth_service_on_change, service);
    }

    *out_service = service;
    return REACH_OK;
}

void reach_bluetooth_service_stop(reach_bluetooth_service *service)
{
    if (service == nullptr)
    {
        return;
    }
    if (service->port.stop_watching != nullptr)
    {
        service->port.stop_watching(service->port.userdata);
    }
    if (!service->thread_started)
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        service->stop = 1;
    }
    service->cv.notify_all();
    if (service->thread.joinable())
    {
        service->thread.join();
    }
    service->thread_started = 0;
    service->scanning = 0;
}

void reach_bluetooth_service_destroy(reach_bluetooth_service *service)
{
    if (service == nullptr)
    {
        return;
    }
    reach_bluetooth_service_stop(service);
    if (service->port.destroy != nullptr)
    {
        service->port.destroy(service->port.userdata);
    }
    delete service;
}

void reach_bluetooth_service_refresh(reach_bluetooth_service *service)
{
    reach_bluetooth_service_submit(service, REACH_BLUETOOTH_SERVICE_COMMAND_REFRESH, nullptr, 0);
}

void reach_bluetooth_service_set_scan_enabled(reach_bluetooth_service *service, int32_t enabled)
{
    reach_bluetooth_service_submit(service, REACH_BLUETOOTH_SERVICE_COMMAND_SET_SCAN, nullptr,
                                   enabled);
}

void reach_bluetooth_service_pair(reach_bluetooth_service *service, const uint16_t *device_id)
{
    if (device_id == nullptr || device_id[0] == 0)
    {
        return;
    }
    reach_bluetooth_service_submit(service, REACH_BLUETOOTH_SERVICE_COMMAND_PAIR, device_id, 0);
}

void reach_bluetooth_service_respond_pairing(reach_bluetooth_service *service, int32_t accept)
{
    reach_bluetooth_service_submit(service, REACH_BLUETOOTH_SERVICE_COMMAND_RESPOND_PAIRING,
                                   nullptr, accept);
}

void reach_bluetooth_service_unpair(reach_bluetooth_service *service, const uint16_t *device_id)
{
    if (device_id == nullptr || device_id[0] == 0)
    {
        return;
    }
    reach_bluetooth_service_submit(service, REACH_BLUETOOTH_SERVICE_COMMAND_UNPAIR, device_id, 0);
}

int32_t reach_bluetooth_service_take(reach_bluetooth_service *service,
                                     reach_bluetooth_snapshot *out_snapshot)
{
    if (service == nullptr || out_snapshot == nullptr)
    {
        return 0;
    }
    std::lock_guard<std::mutex> lock(service->mutex);
    if (!service->completed)
    {
        return 0;
    }
    service->completed = 0;
    if (service->completed_generation != service->generation)
    {
        return 0;
    }
    *out_snapshot = service->snapshot;
    return 1;
}

void reach_bluetooth_service_read(const reach_bluetooth_service *service,
                                  reach_bluetooth_snapshot *out_snapshot)
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
    reach_bluetooth_service *mutable_service = const_cast<reach_bluetooth_service *>(service);
    std::lock_guard<std::mutex> lock(mutable_service->mutex);
    *out_snapshot = mutable_service->snapshot;
}

int32_t reach_bluetooth_service_pending(const reach_bluetooth_service *service)
{
    if (service == nullptr)
    {
        return 0;
    }
    reach_bluetooth_service *mutable_service = const_cast<reach_bluetooth_service *>(service);
    std::lock_guard<std::mutex> lock(mutable_service->mutex);
    return mutable_service->pending_command != REACH_BLUETOOTH_SERVICE_COMMAND_NONE ||
           mutable_service->in_flight || mutable_service->completed;
}

int32_t reach_bluetooth_service_busy(const reach_bluetooth_service *service)
{
    if (service == nullptr)
    {
        return 0;
    }
    reach_bluetooth_service *mutable_service = const_cast<reach_bluetooth_service *>(service);
    std::lock_guard<std::mutex> lock(mutable_service->mutex);
    return mutable_service->pending_command != REACH_BLUETOOTH_SERVICE_COMMAND_NONE ||
           mutable_service->in_flight;
}
