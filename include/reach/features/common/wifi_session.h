#ifndef REACH_FEATURES_COMMON_WIFI_SESSION_H
#define REACH_FEATURES_COMMON_WIFI_SESSION_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/wifi.h"
#include "reach/features/common/text_edit.h"
#include "reach/services/wifi.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_WIFI_SESSION_SELECTION_NONE (-1)

    typedef enum reach_wifi_session_status
    {
        REACH_WIFI_SESSION_STATUS_IDLE = 0,
        REACH_WIFI_SESSION_STATUS_SCANNING,
        REACH_WIFI_SESSION_STATUS_CONNECTING,
        REACH_WIFI_SESSION_STATUS_CONNECTED,
        REACH_WIFI_SESSION_STATUS_INVALID_KEY,
        REACH_WIFI_SESSION_STATUS_NOT_FOUND,
        REACH_WIFI_SESSION_STATUS_FAILED,
        REACH_WIFI_SESSION_STATUS_SCAN_FAILED,
        REACH_WIFI_SESSION_STATUS_FORGET_FAILED
    } reach_wifi_session_status;

    typedef struct reach_wifi_session
    {
        reach_wifi_radio_state radio;
        reach_wifi_network_list networks;
        int32_t loaded;
        reach_wifi_session_status status;
        uint16_t status_ssid[REACH_WIFI_SSID_CAPACITY];
        int32_t selected;
        reach_text_edit key_edit;
        int32_t show_key;
        int32_t connect_automatically;
    } reach_wifi_session;

    void reach_wifi_session_init(reach_wifi_session *session);

    void reach_wifi_session_apply_networks(reach_wifi_session *session,
                                           reach_wifi_radio_state radio,
                                           const reach_wifi_network_list *networks);
    int32_t reach_wifi_session_apply_snapshot(reach_wifi_session *session,
                                              const reach_wifi_snapshot *snapshot);

    void reach_wifi_session_set_status(reach_wifi_session *session,
                                       reach_wifi_session_status status, const uint16_t *ssid);
    const uint16_t *reach_wifi_session_status_message(reach_wifi_session_status status);
    int32_t reach_wifi_session_busy(const reach_wifi_session *session);

    int32_t reach_wifi_session_toggle_radio(reach_wifi_session *session);

    void reach_wifi_session_select(reach_wifi_session *session, int32_t selection);
    const reach_wifi_network *
    reach_wifi_session_selected_network(const reach_wifi_session *session);

    int32_t reach_wifi_session_connect_ready(const reach_wifi_session *session);
    int32_t reach_wifi_session_build_connect(const reach_wifi_session *session, size_t index,
                                             reach_wifi_connect_request *out_request);
    void reach_wifi_session_clear_secrets(reach_wifi_session *session);

    const uint16_t *reach_wifi_session_network_detail(const reach_wifi_network *network,
                                                      int32_t known_view);
    const uint16_t *reach_wifi_session_key_display(const reach_wifi_session *session,
                                                   const reach_text_edit *edit,
                                                   uint16_t out_masked[REACH_TEXT_EDIT_CAPACITY]);

#ifdef __cplusplus
}
#endif

#endif
