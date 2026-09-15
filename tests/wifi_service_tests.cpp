#include "reach/services/wifi.h"

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>

struct fake_wifi_port
{
    reach_wifi_change_callback callback = nullptr;
    void *callback_user = nullptr;
    std::mutex mutex;
    std::condition_variable cv;
    int notifications = 0;
};

static reach_result fake_get_radio_state(void *, reach_wifi_radio_state *out_state)
{
    *out_state = REACH_WIFI_RADIO_ON;
    return REACH_OK;
}

static reach_result fake_read_networks(void *, reach_wifi_network_list *out_networks)
{
    *out_networks = {};
    return REACH_OK;
}

static reach_result fake_connect(void *user, const reach_wifi_connect_request *,
                                 reach_wifi_connect_result *out_result)
{
    fake_wifi_port *fake = static_cast<fake_wifi_port *>(user);
    *out_result = REACH_WIFI_CONNECT_RESULT_SUCCEEDED;
    fake->callback(fake->callback_user, REACH_WIFI_CHANGE_CONNECTION);
    return REACH_OK;
}

static reach_result fake_start_watching(void *user, reach_wifi_change_callback callback,
                                        void *callback_user)
{
    fake_wifi_port *fake = static_cast<fake_wifi_port *>(user);
    fake->callback = callback;
    fake->callback_user = callback_user;
    return REACH_OK;
}

static void fake_notify(void *user)
{
    fake_wifi_port *fake = static_cast<fake_wifi_port *>(user);
    {
        std::lock_guard<std::mutex> lock(fake->mutex);
        ++fake->notifications;
    }
    fake->cv.notify_all();
}

int main()
{
    fake_wifi_port fake;
    reach_wifi_port port = {};
    port.userdata = &fake;
    port.get_radio_state = fake_get_radio_state;
    port.read_networks = fake_read_networks;
    port.connect = fake_connect;
    port.start_watching = fake_start_watching;

    reach_wifi_service *service = nullptr;
    if (reach_wifi_service_create(port, fake_notify, &fake, &service) != REACH_OK)
    {
        std::fprintf(stderr, "FAILED: could not create Wi-Fi service\n");
        return 1;
    }

    reach_wifi_connect_request request = {};
    request.ssid[0] = u'T';
    reach_wifi_service_connect(service, &request);

    bool completed = false;
    {
        std::unique_lock<std::mutex> lock(fake.mutex);
        completed = fake.cv.wait_for(lock, std::chrono::seconds(5),
                                     [&fake]() { return fake.notifications >= 2; });
    }

    reach_wifi_snapshot snapshot = {};
    bool result_retained = completed && reach_wifi_service_take(service, &snapshot) &&
                           snapshot.completed_command == REACH_WIFI_SERVICE_COMMAND_CONNECT &&
                           snapshot.connect_result == REACH_WIFI_CONNECT_RESULT_SUCCEEDED &&
                           snapshot.connect_ssid[0] == u'T';

    reach_wifi_service_destroy(service);
    if (!result_retained)
    {
        std::fprintf(stderr, "FAILED: connection result was lost after refresh\n");
        return 1;
    }
    return 0;
}
