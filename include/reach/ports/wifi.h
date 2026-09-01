#ifndef REACH_PORTS_WIFI_H
#define REACH_PORTS_WIFI_H

#include "reach/core/wifi.h"
#include "reach/support/util.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_WIFI_CHANGE_RADIO (1u << 0)
#define REACH_WIFI_CHANGE_NETWORKS (1u << 1)
#define REACH_WIFI_CHANGE_SCAN_COMPLETE (1u << 2)
#define REACH_WIFI_CHANGE_SCAN_FAILED (1u << 3)
#define REACH_WIFI_CHANGE_CONNECTION (1u << 4)

    typedef void (*reach_wifi_change_callback)(void *user, uint32_t change_flags);

    typedef struct reach_wifi_port
    {
        void *userdata;

        /* Optional per-thread setup for the worker thread that calls the ops below. */
        void (*thread_attach)(void *userdata);

        void (*thread_detach)(void *userdata);

        reach_result (*get_radio_state)(void *userdata, reach_wifi_radio_state *out_state);

        reach_result (*set_radio_enabled)(void *userdata, int32_t enabled);

        reach_result (*start_scan)(void *userdata);

        reach_result (*read_networks)(void *userdata, reach_wifi_network_list *out_networks);

        reach_result (*connect)(void *userdata, const reach_wifi_connect_request *request,
                                reach_wifi_connect_result *out_result);

        reach_result (*disconnect)(void *userdata);

        reach_result (*forget)(void *userdata, const uint16_t *ssid);

        reach_result (*start_watching)(void *userdata, reach_wifi_change_callback callback,
                                       void *callback_user);

        void (*stop_watching)(void *userdata);

        void (*destroy)(void *userdata);
    } reach_wifi_port;

#ifdef __cplusplus
}
#endif
#endif
