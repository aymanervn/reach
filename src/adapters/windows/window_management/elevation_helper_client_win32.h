#ifndef REACH_ELEVATION_HELPER_CLIENT_WIN32_H
#define REACH_ELEVATION_HELPER_CLIENT_WIN32_H

#include "elevation_helper_protocol.h"

reach_result reach_elevation_helper_send(reach_elevation_helper_command command,
                                         uintptr_t window_id, reach_split_mode mode);
reach_result reach_elevation_helper_send_request(const reach_elevation_helper_request *request,
                                                 reach_elevation_helper_response *out_response);

#endif
