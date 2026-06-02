#include "elevation_helper_protocol.h"

uint32_t reach_elevation_helper_protocol_version(void)
{
    return 1;
}

int32_t reach_elevation_helper_command_valid(uint32_t command)
{
    switch (command)
    {
    case REACH_ELEVATION_HELPER_COMMAND_PING:
    case REACH_ELEVATION_HELPER_COMMAND_ACTIVATE:
    case REACH_ELEVATION_HELPER_COMMAND_MINIMIZE:
    case REACH_ELEVATION_HELPER_COMMAND_SNAP:
    case REACH_ELEVATION_HELPER_COMMAND_CLOSE:
    case REACH_ELEVATION_HELPER_COMMAND_HIDE:
    case REACH_ELEVATION_HELPER_COMMAND_SET_HOTKEY_FORWARDING:
        return 1;
    default:
        return 0;
    }
}

int32_t reach_elevation_helper_request_valid(const reach_elevation_helper_request *request)
{
    if (request == nullptr || request->version != reach_elevation_helper_protocol_version() ||
        !reach_elevation_helper_command_valid(request->command))
    {
        return 0;
    }

    if (request->command == REACH_ELEVATION_HELPER_COMMAND_SET_HOTKEY_FORWARDING)
    {
        const uint32_t valid_mask =
            REACH_ELEVATION_HELPER_HOTKEY_ALT_TAB | REACH_ELEVATION_HELPER_HOTKEY_WINDOWS_KEY;
        if ((request->hotkey_mask & ~valid_mask) != 0)
        {
            return 0;
        }
        if (request->flags != 0 && (request->hotkey_mask == 0 || request->event_pipe[0] == 0))
        {
            return 0;
        }
        return 1;
    }

    if (request->command == REACH_ELEVATION_HELPER_COMMAND_PING)
    {
        return 1;
    }

    if (request->window == 0)
    {
        return 0;
    }

    if (request->command == REACH_ELEVATION_HELPER_COMMAND_SNAP)
    {
        return request->split_mode >= REACH_SPLIT_LEFT && request->split_mode <= REACH_SPLIT_FULL;
    }

    return 1;
}
