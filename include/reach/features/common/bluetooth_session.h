#ifndef REACH_FEATURES_COMMON_BLUETOOTH_SESSION_H
#define REACH_FEATURES_COMMON_BLUETOOTH_SESSION_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/bluetooth.h"
#include "reach/core/render_commands.h"
#include "reach/ports/system_controls.h"
#include "reach/services/bluetooth.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_BLUETOOTH_SESSION_SELECTION_NONE (-1)

    typedef enum reach_bluetooth_session_status
    {
        REACH_BLUETOOTH_SESSION_STATUS_IDLE = 0,
        REACH_BLUETOOTH_SESSION_STATUS_SCANNING,
        REACH_BLUETOOTH_SESSION_STATUS_PAIRING,
        REACH_BLUETOOTH_SESSION_STATUS_CONFIRM_PIN,
        REACH_BLUETOOTH_SESSION_STATUS_PAIRED,
        REACH_BLUETOOTH_SESSION_STATUS_REJECTED,
        REACH_BLUETOOTH_SESSION_STATUS_FAILED
    } reach_bluetooth_session_status;

    typedef struct reach_bluetooth_session
    {
        reach_bluetooth_device_list devices;
        reach_bluetooth_pairing_request pairing;
        reach_bluetooth_state radio;
        int32_t scanning;
        int32_t loaded;
        reach_bluetooth_session_status status;
        uint16_t status_device[REACH_BLUETOOTH_DEVICE_ID_CAPACITY];
        int32_t selected;
    } reach_bluetooth_session;

    void reach_bluetooth_session_init(reach_bluetooth_session *session);

    void reach_bluetooth_session_apply_devices(reach_bluetooth_session *session,
                                               const reach_bluetooth_device_list *devices,
                                               const reach_bluetooth_pairing_request *pairing,
                                               int32_t scanning);
    void reach_bluetooth_session_apply_snapshot(reach_bluetooth_session *session,
                                                const reach_bluetooth_snapshot *snapshot);
    void reach_bluetooth_session_set_radio(reach_bluetooth_session *session,
                                           reach_bluetooth_state state);
    int32_t reach_bluetooth_session_toggle_radio(reach_bluetooth_session *session);

    void reach_bluetooth_session_set_status(reach_bluetooth_session *session,
                                            reach_bluetooth_session_status status,
                                            const uint16_t *device_id);
    const uint16_t *reach_bluetooth_session_status_message(reach_bluetooth_session_status status);

    void reach_bluetooth_session_select(reach_bluetooth_session *session, int32_t selection);
    const reach_bluetooth_device *
    reach_bluetooth_session_selected_device(const reach_bluetooth_session *session);
    int32_t reach_bluetooth_session_device_is_pairing(const reach_bluetooth_session *session,
                                                      size_t index);

    const uint16_t *reach_bluetooth_session_device_detail(const reach_bluetooth_device *device);
    reach_vector_icon_id reach_bluetooth_session_device_icon(reach_bluetooth_device_kind kind);

#ifdef __cplusplus
}
#endif

#endif
