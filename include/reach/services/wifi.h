#ifndef REACH_SERVICES_WIFI_H
#define REACH_SERVICES_WIFI_H

#include <stdint.h>

#include "reach/ports/wifi.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_wifi_service reach_wifi_service;

    typedef enum reach_wifi_service_command
    {
        REACH_WIFI_SERVICE_COMMAND_NONE = 0,
        REACH_WIFI_SERVICE_COMMAND_REFRESH,
        REACH_WIFI_SERVICE_COMMAND_SCAN,
        REACH_WIFI_SERVICE_COMMAND_CONNECT,
        REACH_WIFI_SERVICE_COMMAND_DISCONNECT,
        REACH_WIFI_SERVICE_COMMAND_FORGET,
        REACH_WIFI_SERVICE_COMMAND_SET_RADIO
    } reach_wifi_service_command;

    typedef struct reach_wifi_snapshot
    {
        reach_wifi_radio_state radio;
        reach_wifi_network_list networks;
        reach_wifi_service_command completed_command;
        reach_wifi_scan_result scan_result;
        reach_wifi_connect_result connect_result;
        uint16_t connect_ssid[REACH_WIFI_SSID_CAPACITY];
        int32_t command_succeeded;
    } reach_wifi_snapshot;

    reach_result reach_wifi_service_create(reach_wifi_port port, void (*notify)(void *user),
                                           void *notify_user, reach_wifi_service **out_service);
    void reach_wifi_service_destroy(reach_wifi_service *service);
    void reach_wifi_service_stop(reach_wifi_service *service);

    void reach_wifi_service_refresh(reach_wifi_service *service);
    void reach_wifi_service_scan(reach_wifi_service *service);
    void reach_wifi_service_connect(reach_wifi_service *service,
                                    const reach_wifi_connect_request *request);
    void reach_wifi_service_disconnect(reach_wifi_service *service);
    void reach_wifi_service_forget(reach_wifi_service *service, const uint16_t *ssid);
    void reach_wifi_service_set_radio_enabled(reach_wifi_service *service, int32_t enabled);

    int32_t reach_wifi_service_take(reach_wifi_service *service, reach_wifi_snapshot *out_snapshot);
    void reach_wifi_service_read(const reach_wifi_service *service,
                                 reach_wifi_snapshot *out_snapshot);

    int32_t reach_wifi_service_pending(const reach_wifi_service *service);
    int32_t reach_wifi_service_busy(const reach_wifi_service *service);

#ifdef __cplusplus
}
#endif
#endif
