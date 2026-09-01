#ifndef REACH_SERVICES_BLUETOOTH_H
#define REACH_SERVICES_BLUETOOTH_H

#include <stdint.h>

#include "reach/ports/bluetooth.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_bluetooth_service reach_bluetooth_service;

    typedef enum reach_bluetooth_service_command
    {
        REACH_BLUETOOTH_SERVICE_COMMAND_NONE = 0,
        REACH_BLUETOOTH_SERVICE_COMMAND_REFRESH,
        REACH_BLUETOOTH_SERVICE_COMMAND_SET_SCAN,
        REACH_BLUETOOTH_SERVICE_COMMAND_PAIR,
        REACH_BLUETOOTH_SERVICE_COMMAND_RESPOND_PAIRING,
        REACH_BLUETOOTH_SERVICE_COMMAND_UNPAIR
    } reach_bluetooth_service_command;

    typedef struct reach_bluetooth_snapshot
    {
        reach_bluetooth_device_list devices;
        reach_bluetooth_pairing_request pairing;
        reach_bluetooth_service_command completed_command;
        reach_bluetooth_pair_result pair_result;
        uint16_t pair_device_id[REACH_BLUETOOTH_DEVICE_ID_CAPACITY];
        int32_t scanning;
        int32_t command_succeeded;
    } reach_bluetooth_snapshot;

    reach_result reach_bluetooth_service_create(reach_bluetooth_port port,
                                                void (*notify)(void *user), void *notify_user,
                                                reach_bluetooth_service **out_service);
    void reach_bluetooth_service_destroy(reach_bluetooth_service *service);
    void reach_bluetooth_service_stop(reach_bluetooth_service *service);

    void reach_bluetooth_service_refresh(reach_bluetooth_service *service);
    void reach_bluetooth_service_set_scan_enabled(reach_bluetooth_service *service,
                                                  int32_t enabled);
    void reach_bluetooth_service_pair(reach_bluetooth_service *service, const uint16_t *device_id);
    void reach_bluetooth_service_respond_pairing(reach_bluetooth_service *service, int32_t accept);
    void reach_bluetooth_service_unpair(reach_bluetooth_service *service,
                                        const uint16_t *device_id);

    int32_t reach_bluetooth_service_take(reach_bluetooth_service *service,
                                         reach_bluetooth_snapshot *out_snapshot);
    void reach_bluetooth_service_read(const reach_bluetooth_service *service,
                                      reach_bluetooth_snapshot *out_snapshot);

    int32_t reach_bluetooth_service_pending(const reach_bluetooth_service *service);
    int32_t reach_bluetooth_service_busy(const reach_bluetooth_service *service);

#ifdef __cplusplus
}
#endif
#endif
