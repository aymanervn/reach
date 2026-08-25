#include "reach/services/wifi.h"

#include "reach/support/util.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>

#define REACH_WIFI_SERVICE_SCAN_TIMEOUT_MS 6000

struct reach_wifi_service
{
    reach_wifi_port port;
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

    reach_wifi_service_command pending_command = REACH_WIFI_SERVICE_COMMAND_NONE;
    reach_wifi_connect_request pending_request = {};
    uint16_t pending_ssid[REACH_WIFI_SSID_CAPACITY] = {};
    int32_t pending_radio_enabled = 0;

    std::mutex scan_mutex;
    std::condition_variable scan_cv;
    int32_t scan_signalled = 0;
    reach_wifi_scan_result scan_signal_result = REACH_WIFI_SCAN_RESULT_NONE;

    reach_wifi_snapshot snapshot = {};
};

static void reach_wifi_service_notify(reach_wifi_service *service)
{
    if (service->notify != nullptr)
    {
        service->notify(service->notify_user);
    }
}

static void reach_wifi_service_on_change(void *user, uint32_t change_flags)
{
    reach_wifi_service *service = static_cast<reach_wifi_service *>(user);
    if (service == nullptr)
    {
        return;
    }

    if ((change_flags & (REACH_WIFI_CHANGE_SCAN_COMPLETE | REACH_WIFI_CHANGE_SCAN_FAILED)) != 0)
    {
        {
            std::lock_guard<std::mutex> lock(service->scan_mutex);
            service->scan_signalled = 1;
            service->scan_signal_result = (change_flags & REACH_WIFI_CHANGE_SCAN_COMPLETE) != 0
                                              ? REACH_WIFI_SCAN_RESULT_SUCCEEDED
                                              : REACH_WIFI_SCAN_RESULT_FAILED;
        }
        service->scan_cv.notify_all();
        return;
    }

    if ((change_flags & (REACH_WIFI_CHANGE_NETWORKS | REACH_WIFI_CHANGE_CONNECTION |
                         REACH_WIFI_CHANGE_RADIO)) != 0)
    {
        reach_wifi_service_refresh(service);
    }
}

static reach_wifi_scan_result reach_wifi_service_wait_for_scan(reach_wifi_service *service)
{
    std::unique_lock<std::mutex> lock(service->scan_mutex);
    if (!service->scan_cv.wait_for(lock,
                                   std::chrono::milliseconds(REACH_WIFI_SERVICE_SCAN_TIMEOUT_MS),
                                   [service]() { return service->scan_signalled != 0; }))
    {
        return REACH_WIFI_SCAN_RESULT_TIMED_OUT;
    }
    service->scan_signalled = 0;
    return service->scan_signal_result;
}

static void reach_wifi_service_read_state(reach_wifi_service *service,
                                          reach_wifi_snapshot *snapshot)
{
    if (service->port.get_radio_state != nullptr)
    {
        (void)service->port.get_radio_state(service->port.userdata, &snapshot->radio);
    }
    if (service->port.read_networks != nullptr && snapshot->radio != REACH_WIFI_RADIO_UNAVAILABLE)
    {
        (void)service->port.read_networks(service->port.userdata, &snapshot->networks);
    }
}

static void reach_wifi_service_thread_main(reach_wifi_service *service)
{
    if (service->port.thread_attach != nullptr)
    {
        service->port.thread_attach(service->port.userdata);
    }

    for (;;)
    {
        reach_wifi_service_command command = REACH_WIFI_SERVICE_COMMAND_NONE;
        reach_wifi_connect_request request = {};
        uint16_t ssid[REACH_WIFI_SSID_CAPACITY] = {};
        int32_t radio_enabled = 0;
        uint32_t generation = 0;
        {
            std::unique_lock<std::mutex> lock(service->mutex);
            service->cv.wait(lock, [service]() {
                return service->stop ||
                       service->pending_command != REACH_WIFI_SERVICE_COMMAND_NONE;
            });
            if (service->stop)
            {
                break;
            }
            command = service->pending_command;
            request = service->pending_request;
            reach_copy_utf16(ssid, REACH_WIFI_SSID_CAPACITY, service->pending_ssid);
            radio_enabled = service->pending_radio_enabled;
            generation = service->pending_generation;
            service->pending_command = REACH_WIFI_SERVICE_COMMAND_NONE;
            service->in_flight = 1;
        }

        reach_wifi_snapshot snapshot = {};
        snapshot.completed_command = command;
        snapshot.command_succeeded = 1;

        switch (command)
        {
        case REACH_WIFI_SERVICE_COMMAND_SCAN:
            if (service->port.start_scan != nullptr &&
                service->port.start_scan(service->port.userdata) == REACH_OK)
            {
                snapshot.scan_result = reach_wifi_service_wait_for_scan(service);
            }
            else
            {
                snapshot.scan_result = REACH_WIFI_SCAN_RESULT_FAILED;
            }
            snapshot.command_succeeded = snapshot.scan_result == REACH_WIFI_SCAN_RESULT_SUCCEEDED;
            break;
        case REACH_WIFI_SERVICE_COMMAND_CONNECT:
            reach_copy_utf16(snapshot.connect_ssid, REACH_WIFI_SSID_CAPACITY, request.ssid);
            if (service->port.connect != nullptr)
            {
                (void)service->port.connect(service->port.userdata, &request,
                                            &snapshot.connect_result);
            }
            else
            {
                snapshot.connect_result = REACH_WIFI_CONNECT_RESULT_FAILED;
            }
            snapshot.command_succeeded =
                snapshot.connect_result == REACH_WIFI_CONNECT_RESULT_SUCCEEDED;
            break;
        case REACH_WIFI_SERVICE_COMMAND_DISCONNECT:
            snapshot.command_succeeded = service->port.disconnect != nullptr &&
                                         service->port.disconnect(service->port.userdata) ==
                                             REACH_OK;
            break;
        case REACH_WIFI_SERVICE_COMMAND_FORGET:
            snapshot.command_succeeded =
                service->port.forget != nullptr &&
                service->port.forget(service->port.userdata, ssid) == REACH_OK;
            break;
        case REACH_WIFI_SERVICE_COMMAND_SET_RADIO:
            snapshot.command_succeeded =
                service->port.set_radio_enabled != nullptr &&
                service->port.set_radio_enabled(service->port.userdata, radio_enabled) == REACH_OK;
            break;
        case REACH_WIFI_SERVICE_COMMAND_REFRESH:
        default:
            break;
        }

        reach_wifi_service_read_state(service, &snapshot);

        {
            std::lock_guard<std::mutex> lock(service->mutex);
            service->snapshot = snapshot;
            service->completed_generation = generation;
            service->completed = 1;
            service->in_flight = 0;
        }
        reach_wifi_service_notify(service);
    }

    if (service->port.thread_detach != nullptr)
    {
        service->port.thread_detach(service->port.userdata);
    }
}

static reach_result reach_wifi_service_start_thread(reach_wifi_service *service)
{
    if (service->thread_started)
    {
        return REACH_OK;
    }
    service->stop = 0;
    try
    {
        service->thread = std::thread(reach_wifi_service_thread_main, service);
    }
    catch (...)
    {
        return REACH_ERROR;
    }
    service->thread_started = 1;
    return REACH_OK;
}

static void reach_wifi_service_submit(reach_wifi_service *service,
                                      reach_wifi_service_command command)
{
    if (service == nullptr || reach_wifi_service_start_thread(service) != REACH_OK)
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        /* A refresh raised by an OS change must never displace a queued user action. */
        if (command == REACH_WIFI_SERVICE_COMMAND_REFRESH &&
            service->pending_command != REACH_WIFI_SERVICE_COMMAND_NONE)
        {
            return;
        }
        ++service->generation;
        service->pending_generation = service->generation;
        service->pending_command = command;
    }
    service->cv.notify_one();
}

reach_result reach_wifi_service_create(reach_wifi_port port, void (*notify)(void *user),
                                       void *notify_user, reach_wifi_service **out_service)
{
    if (out_service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_service = nullptr;

    reach_wifi_service *service = new (std::nothrow) reach_wifi_service();
    if (service == nullptr)
    {
        return REACH_ERROR;
    }
    service->port = port;
    service->notify = notify;
    service->notify_user = notify_user;

    if (service->port.start_watching != nullptr)
    {
        (void)service->port.start_watching(service->port.userdata, reach_wifi_service_on_change,
                                           service);
    }

    *out_service = service;
    return REACH_OK;
}

void reach_wifi_service_stop(reach_wifi_service *service)
{
    if (service == nullptr)
    {
        return;
    }
    if (service->port.stop_watching != nullptr)
    {
        service->port.stop_watching(service->port.userdata);
    }
    {
        std::lock_guard<std::mutex> lock(service->scan_mutex);
        service->scan_signalled = 1;
        service->scan_signal_result = REACH_WIFI_SCAN_RESULT_FAILED;
    }
    service->scan_cv.notify_all();

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
}

void reach_wifi_service_destroy(reach_wifi_service *service)
{
    if (service == nullptr)
    {
        return;
    }
    reach_wifi_service_stop(service);
    if (service->port.destroy != nullptr)
    {
        service->port.destroy(service->port.userdata);
    }
    delete service;
}

void reach_wifi_service_refresh(reach_wifi_service *service)
{
    reach_wifi_service_submit(service, REACH_WIFI_SERVICE_COMMAND_REFRESH);
}

void reach_wifi_service_scan(reach_wifi_service *service)
{
    reach_wifi_service_submit(service, REACH_WIFI_SERVICE_COMMAND_SCAN);
}

void reach_wifi_service_connect(reach_wifi_service *service,
                                const reach_wifi_connect_request *request)
{
    if (service == nullptr || request == nullptr ||
        reach_wifi_service_start_thread(service) != REACH_OK)
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        ++service->generation;
        service->pending_generation = service->generation;
        service->pending_command = REACH_WIFI_SERVICE_COMMAND_CONNECT;
        service->pending_request = *request;
    }
    service->cv.notify_one();
}

void reach_wifi_service_disconnect(reach_wifi_service *service)
{
    reach_wifi_service_submit(service, REACH_WIFI_SERVICE_COMMAND_DISCONNECT);
}

void reach_wifi_service_forget(reach_wifi_service *service, const uint16_t *ssid)
{
    if (service == nullptr || ssid == nullptr || ssid[0] == 0 ||
        reach_wifi_service_start_thread(service) != REACH_OK)
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        ++service->generation;
        service->pending_generation = service->generation;
        service->pending_command = REACH_WIFI_SERVICE_COMMAND_FORGET;
        reach_copy_utf16(service->pending_ssid, REACH_WIFI_SSID_CAPACITY, ssid);
    }
    service->cv.notify_one();
}

void reach_wifi_service_set_radio_enabled(reach_wifi_service *service, int32_t enabled)
{
    if (service == nullptr || reach_wifi_service_start_thread(service) != REACH_OK)
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        ++service->generation;
        service->pending_generation = service->generation;
        service->pending_command = REACH_WIFI_SERVICE_COMMAND_SET_RADIO;
        service->pending_radio_enabled = enabled;
    }
    service->cv.notify_one();
}

int32_t reach_wifi_service_take(reach_wifi_service *service, reach_wifi_snapshot *out_snapshot)
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

void reach_wifi_service_read(const reach_wifi_service *service, reach_wifi_snapshot *out_snapshot)
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
    reach_wifi_service *mutable_service = const_cast<reach_wifi_service *>(service);
    std::lock_guard<std::mutex> lock(mutable_service->mutex);
    *out_snapshot = mutable_service->snapshot;
}

int32_t reach_wifi_service_pending(const reach_wifi_service *service)
{
    if (service == nullptr)
    {
        return 0;
    }
    reach_wifi_service *mutable_service = const_cast<reach_wifi_service *>(service);
    std::lock_guard<std::mutex> lock(mutable_service->mutex);
    return mutable_service->pending_command != REACH_WIFI_SERVICE_COMMAND_NONE ||
           mutable_service->in_flight || mutable_service->completed;
}

int32_t reach_wifi_service_busy(const reach_wifi_service *service)
{
    if (service == nullptr)
    {
        return 0;
    }
    reach_wifi_service *mutable_service = const_cast<reach_wifi_service *>(service);
    std::lock_guard<std::mutex> lock(mutable_service->mutex);
    return mutable_service->pending_command != REACH_WIFI_SERVICE_COMMAND_NONE ||
           mutable_service->in_flight;
}
