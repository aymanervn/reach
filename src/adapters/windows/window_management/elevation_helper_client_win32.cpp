#include "elevation_helper_client_win32.h"

#include <windows.h>

static reach_result reach_elevation_helper_send_request(
    const reach_elevation_helper_request *request) {
  if (!reach_elevation_helper_request_valid(request)) {
    return REACH_INVALID_ARGUMENT;
  }
  if (!WaitNamedPipeW(REACH_ELEVATION_HELPER_PIPE_NAME, 50)) {
    return REACH_ERROR;
  }

  HANDLE pipe = CreateFileW(REACH_ELEVATION_HELPER_PIPE_NAME,
                            GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                            OPEN_EXISTING, 0, nullptr);
  if (pipe == INVALID_HANDLE_VALUE) {
    return REACH_ERROR;
  }

  DWORD mode_bytes = PIPE_READMODE_MESSAGE;
  (void)SetNamedPipeHandleState(pipe, &mode_bytes, nullptr, nullptr);

  DWORD written = 0;
  BOOL ok = WriteFile(pipe, request, sizeof(*request), &written, nullptr);
  if (!ok || written != sizeof(*request)) {
    CloseHandle(pipe);
    return REACH_ERROR;
  }

  reach_elevation_helper_response response = {};
  DWORD read = 0;
  ok = ReadFile(pipe, &response, sizeof(response), &read, nullptr);
  CloseHandle(pipe);

  if (!ok || read != sizeof(response) ||
      response.version != reach_elevation_helper_protocol_version()) {
    return REACH_ERROR;
  }

  return static_cast<reach_result>(response.result);
}

reach_result reach_elevation_helper_send(
    reach_elevation_helper_command command,
    uintptr_t window_id,
    reach_split_mode mode) {
  reach_elevation_helper_request request = {};
  request.version = reach_elevation_helper_protocol_version();
  request.command = command;
  request.window = static_cast<uint64_t>(window_id);
  request.split_mode = static_cast<int32_t>(mode);
  return reach_elevation_helper_send_request(&request);
}

reach_result reach_elevation_helper_set_hotkey_forwarding(
    int32_t enabled,
    uint32_t hotkey_mask,
    const wchar_t *event_pipe) {
  reach_elevation_helper_request request = {};
  request.version = reach_elevation_helper_protocol_version();
  request.command = REACH_ELEVATION_HELPER_COMMAND_SET_HOTKEY_FORWARDING;
  request.flags = enabled ? 1u : 0u;
  request.hotkey_mask = enabled ? hotkey_mask : 0u;
  if (enabled && event_pipe != nullptr) {
    size_t index = 0;
    while (index + 1 < 128 && event_pipe[index] != 0) {
      request.event_pipe[index] = event_pipe[index];
      ++index;
    }
  }
  return reach_elevation_helper_send_request(&request);
}
