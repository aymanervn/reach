#ifndef REACH_PORTS_BLUETOOTH_H
#define REACH_PORTS_BLUETOOTH_H

#include "reach/core/bluetooth.h"
#include "reach/support/util.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_BLUETOOTH_CHANGE_DEVICES (1u << 0)
#define REACH_BLUETOOTH_CHANGE_SCAN_COMPLETE (1u << 1)
#define REACH_BLUETOOTH_CHANGE_PAIRING (1u << 2)

    typedef void (*reach_bluetooth_change_callback)(void *user, uint32_t change_flags);

    typedef struct reach_bluetooth_port
    {
        void *userdata;

        /* Optional per-thread setup for the worker thread that calls the ops below. */
        void (*thread_attach)(void *userdata);

        void (*thread_detach)(void *userdata);

        reach_result (*set_scan_enabled)(void *userdata, int32_t enabled);

        reach_result (*read_devices)(void *userdata, reach_bluetooth_device_list *out_devices);

        reach_result (*read_pairing_request)(void *userdata,
                                             reach_bluetooth_pairing_request *out_request);

        reach_result (*pair)(void *userdata, const uint16_t *device_id);

        reach_result (*respond_pairing)(void *userdata, int32_t accept);

        reach_result (*take_pair_result)(void *userdata, uint16_t *out_device_id,
                                         size_t device_id_capacity,
                                         reach_bluetooth_pair_result *out_result);

        reach_result (*unpair)(void *userdata, const uint16_t *device_id);

        reach_result (*start_watching)(void *userdata, reach_bluetooth_change_callback callback,
                                       void *callback_user);

        void (*stop_watching)(void *userdata);

        void (*destroy)(void *userdata);
    } reach_bluetooth_port;

#ifdef __cplusplus
}
#endif
#endif
