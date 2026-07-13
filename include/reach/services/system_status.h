#ifndef REACH_SERVICES_SYSTEM_STATUS_H
#define REACH_SERVICES_SYSTEM_STATUS_H

#include <stdint.h>

#include "reach/ports/audio_volume.h"
#include "reach/ports/system_controls.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_system_status reach_system_status;

    typedef struct reach_system_status_audio_snapshot
    {
        reach_audio_volume_state state;
        reach_audio_volume_session_list sessions;
        reach_audio_output_device_list output_devices;
        int32_t state_valid;
        int32_t sessions_valid;
        int32_t output_devices_valid;
    } reach_system_status_audio_snapshot;

    typedef struct reach_system_status_system_snapshot
    {
        reach_network_state network;
        reach_bluetooth_state bluetooth;
        reach_power_state power;
        reach_brightness_state brightness;
        int32_t network_valid;
        int32_t bluetooth_valid;
        int32_t power_valid;
        int32_t brightness_valid;
        uint32_t change_flags;
    } reach_system_status_system_snapshot;

    reach_result reach_system_status_create(reach_audio_volume_port audio_volume,
                                            reach_system_controls_port system_controls,
                                            void (*notify)(void *user), void *notify_user,
                                            reach_system_status **out_service);
    void reach_system_status_destroy(reach_system_status *service);

    void reach_system_status_stop(reach_system_status *service);

    void reach_system_status_refresh_audio(reach_system_status *service);

    void reach_system_status_refresh_system(reach_system_status *service, uint32_t change_flags);

    int32_t reach_system_status_take_audio(reach_system_status *service,
                                           reach_system_status_audio_snapshot *out_snapshot);
    int32_t reach_system_status_take_system(reach_system_status *service,
                                            reach_system_status_system_snapshot *out_snapshot);

    int32_t reach_system_status_audio_pending(const reach_system_status *service);
    int32_t reach_system_status_system_pending(const reach_system_status *service);

#ifdef __cplusplus
}
#endif

#endif
