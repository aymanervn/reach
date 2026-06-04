#ifndef REACH_ELEVATION_HELPER_SESSION_WIN32_H
#define REACH_ELEVATION_HELPER_SESSION_WIN32_H

#include "elevation_helper_protocol.h"

enum reach_elevation_helper_session_state : int32_t
{
    REACH_ELEVATION_HELPER_SESSION_UNKNOWN = 0,
    REACH_ELEVATION_HELPER_SESSION_CONNECTED = 1,
    REACH_ELEVATION_HELPER_SESSION_DISCONNECTED = 2,
};

typedef void (*reach_elevation_helper_session_event_callback)(
    void *user, const reach_elevation_helper_event *event);
typedef void (*reach_elevation_helper_session_state_callback)(
    void *user, reach_elevation_helper_session_state state);

reach_result reach_elevation_helper_session_start(
    reach_elevation_helper_session_event_callback event_callback,
    reach_elevation_helper_session_state_callback state_callback, void *user);
void reach_elevation_helper_session_stop(void);
reach_result reach_elevation_helper_session_reconnect(void);
reach_elevation_helper_session_state reach_elevation_helper_session_get_state(void);
reach_result reach_elevation_helper_session_wait_connected(uint32_t timeout_ms);

reach_result reach_elevation_helper_session_send(reach_elevation_helper_command command,
                                                 uintptr_t window_id, reach_split_mode mode);
reach_result reach_elevation_helper_session_set_hotkey_forwarding(int32_t enabled,
                                                                  uint32_t hotkey_mask);

#endif
