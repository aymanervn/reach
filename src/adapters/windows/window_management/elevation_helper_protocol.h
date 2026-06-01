#ifndef REACH_ELEVATION_HELPER_PROTOCOL_H
#define REACH_ELEVATION_HELPER_PROTOCOL_H

#include "../windows_adapters_internal.h"

#include <stdint.h>

static const wchar_t REACH_ELEVATION_HELPER_PIPE_NAME[] =
    L"\\\\.\\pipe\\ReachElevationHelper";

enum reach_elevation_helper_command : uint32_t {
  REACH_ELEVATION_HELPER_COMMAND_ACTIVATE = 1,
  REACH_ELEVATION_HELPER_COMMAND_MINIMIZE = 2,
  REACH_ELEVATION_HELPER_COMMAND_SNAP = 3,
  REACH_ELEVATION_HELPER_COMMAND_CLOSE = 4,
  REACH_ELEVATION_HELPER_COMMAND_HIDE = 5,
};

struct reach_elevation_helper_request {
  uint32_t version;
  uint32_t command;
  uint64_t window;
  int32_t split_mode;
  uint32_t reserved;
};

struct reach_elevation_helper_response {
  uint32_t version;
  int32_t result;
};

uint32_t reach_elevation_helper_protocol_version(void);
int32_t reach_elevation_helper_command_valid(uint32_t command);
int32_t reach_elevation_helper_request_valid(
    const reach_elevation_helper_request *request);

#endif
