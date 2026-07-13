#include "reach/protocol/reach_service_protocol.h"

uint32_t reach_service_protocol_version(void)
{
    return 8;
}

int32_t reach_service_command_valid(uint32_t command)
{
    switch (command)
    {
    case REACH_SERVICE_COMMAND_PING:
    case REACH_SERVICE_COMMAND_ACTIVATE:
    case REACH_SERVICE_COMMAND_MINIMIZE:
    case REACH_SERVICE_COMMAND_SNAP:
    case REACH_SERVICE_COMMAND_CLOSE:
    case REACH_SERVICE_COMMAND_RESTORE:
        return 1;
    default:
        return 0;
    }
}

int32_t reach_service_request_valid(const reach_service_request *request)
{
    if (request == nullptr || request->version != reach_service_protocol_version() ||
        !reach_service_command_valid(request->command))
    {
        return 0;
    }

    if (request->command == REACH_SERVICE_COMMAND_PING)
    {
        return 1;
    }

    if (request->window == 0)
    {
        return 0;
    }

    if (request->command == REACH_SERVICE_COMMAND_SNAP)
    {
        return request->split_mode >= REACH_SPLIT_LEFT && request->split_mode <= REACH_SPLIT_FULL;
    }

    return 1;
}
