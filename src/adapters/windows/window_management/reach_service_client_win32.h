#ifndef REACH_SERVICE_CLIENT_WIN32_H
#define REACH_SERVICE_CLIENT_WIN32_H

#include "reach/protocol/reach_service_protocol.h"

reach_result reach_service_send(reach_service_command command,
                                         uintptr_t window_id, reach_split_mode mode);
reach_result reach_service_send_request(const reach_service_request *request,
                                                 reach_service_response *out_response);

#endif
