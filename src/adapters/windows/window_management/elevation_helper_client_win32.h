#ifndef REACH_ELEVATION_HELPER_CLIENT_WIN32_H
#define REACH_ELEVATION_HELPER_CLIENT_WIN32_H

#include "elevation_helper_protocol.h"

reach_result reach_elevation_helper_send(reach_elevation_helper_command command,
                                         uintptr_t window_id, reach_split_mode mode);
reach_result reach_elevation_helper_set_event_channel(int32_t enabled, const wchar_t *event_pipe);
reach_result reach_elevation_helper_set_hotkey_forwarding(int32_t enabled, uint32_t hotkey_mask,
                                                          const wchar_t *event_pipe);

#endif
