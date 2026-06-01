#include "../../adapters/windows/window_management/elevation_helper_protocol.h"
#include "../../adapters/windows/window_management/window_actions_win32.h"

#include <windows.h>
#include <sddl.h>

#include <new>
#include <vector>

static reach_result reach_helper_query_token_user(HANDLE token,
                                                  std::vector<BYTE> *out_user) {
  if (token == nullptr || out_user == nullptr) {
    return REACH_INVALID_ARGUMENT;
  }

  DWORD needed = 0;
  (void)GetTokenInformation(token, TokenUser, nullptr, 0, &needed);
  if (needed == 0) {
    return REACH_ERROR;
  }

  out_user->resize(needed);
  if (!GetTokenInformation(token, TokenUser, out_user->data(), needed,
                           &needed)) {
    out_user->clear();
    return REACH_ERROR;
  }

  return REACH_OK;
}

static int32_t reach_helper_same_user_client(HANDLE pipe) {
  HANDLE process_token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token)) {
    return 0;
  }

  std::vector<BYTE> process_user;
  reach_result process_result =
      reach_helper_query_token_user(process_token, &process_user);
  CloseHandle(process_token);
  if (process_result != REACH_OK) {
    return 0;
  }

  if (!ImpersonateNamedPipeClient(pipe)) {
    return 0;
  }

  HANDLE client_token = nullptr;
  BOOL opened_client =
      OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &client_token);
  RevertToSelf();
  if (!opened_client) {
    return 0;
  }

  std::vector<BYTE> client_user;
  reach_result client_result =
      reach_helper_query_token_user(client_token, &client_user);
  CloseHandle(client_token);
  if (client_result != REACH_OK) {
    return 0;
  }

  TOKEN_USER *process_token_user =
      reinterpret_cast<TOKEN_USER *>(process_user.data());
  TOKEN_USER *client_token_user =
      reinterpret_cast<TOKEN_USER *>(client_user.data());
  if (process_token_user == nullptr || client_token_user == nullptr ||
      process_token_user->User.Sid == nullptr ||
      client_token_user->User.Sid == nullptr) {
    return 0;
  }

  return EqualSid(process_token_user->User.Sid, client_token_user->User.Sid);
}

static reach_result reach_helper_execute(
    const reach_elevation_helper_request *request) {
  if (!reach_elevation_helper_request_valid(request)) {
    return REACH_INVALID_ARGUMENT;
  }

  HWND hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(request->window));
  if (hwnd == nullptr || !IsWindow(hwnd)) {
    return REACH_INVALID_ARGUMENT;
  }

  switch (request->command) {
  case REACH_ELEVATION_HELPER_COMMAND_ACTIVATE:
    return reach_window_management_activate(hwnd);
  case REACH_ELEVATION_HELPER_COMMAND_MINIMIZE:
    return reach_window_management_minimize(hwnd);
  case REACH_ELEVATION_HELPER_COMMAND_SNAP:
    return reach_window_management_snap(
        hwnd, static_cast<reach_split_mode>(request->split_mode));
  case REACH_ELEVATION_HELPER_COMMAND_CLOSE:
    return reach_window_management_close(hwnd);
  case REACH_ELEVATION_HELPER_COMMAND_HIDE:
    return reach_window_management_hide(hwnd);
  default:
    return REACH_INVALID_ARGUMENT;
  }
}

static SECURITY_ATTRIBUTES reach_helper_pipe_security(PSECURITY_DESCRIPTOR *sd) {
  *sd = nullptr;
  SECURITY_ATTRIBUTES attributes = {};
  attributes.nLength = sizeof(attributes);
  attributes.bInheritHandle = FALSE;

  const wchar_t *sddl =
      L"D:P(A;;GA;;;IU)(A;;GA;;;SY)(A;;GA;;;BA)";
  if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
          sddl, SDDL_REVISION_1, sd, nullptr)) {
    attributes.lpSecurityDescriptor = *sd;
  }

  return attributes;
}

static HANDLE reach_helper_create_pipe(void) {
  PSECURITY_DESCRIPTOR descriptor = nullptr;
  SECURITY_ATTRIBUTES security = reach_helper_pipe_security(&descriptor);
  HANDLE pipe = CreateNamedPipeW(
      REACH_ELEVATION_HELPER_PIPE_NAME,
      PIPE_ACCESS_DUPLEX,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
      PIPE_UNLIMITED_INSTANCES,
      sizeof(reach_elevation_helper_response),
      sizeof(reach_elevation_helper_request),
      1000,
      security.lpSecurityDescriptor != nullptr ? &security : nullptr);

  if (descriptor != nullptr) {
    LocalFree(descriptor);
  }

  return pipe;
}

static void reach_helper_serve_client(HANDLE pipe) {
  reach_elevation_helper_request request = {};
  DWORD read = 0;
  reach_result result = REACH_ERROR;

  if (ReadFile(pipe, &request, sizeof(request), &read, nullptr) &&
      read == sizeof(request)) {
    if (reach_helper_same_user_client(pipe)) {
      result = reach_helper_execute(&request);
    } else {
      result = REACH_ERROR;
    }
  }

  reach_elevation_helper_response response = {};
  response.version = reach_elevation_helper_protocol_version();
  response.result = static_cast<int32_t>(result);

  DWORD written = 0;
  (void)WriteFile(pipe, &response, sizeof(response), &written, nullptr);
  FlushFileBuffers(pipe);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line,
                    int show_command) {
  (void)instance;
  (void)previous;
  (void)command_line;
  (void)show_command;

  for (;;) {
    HANDLE pipe = reach_helper_create_pipe();
    if (pipe == INVALID_HANDLE_VALUE) {
      Sleep(1000);
      continue;
    }

    BOOL connected = ConnectNamedPipe(pipe, nullptr)
                         ? TRUE
                         : GetLastError() == ERROR_PIPE_CONNECTED;
    if (connected) {
      reach_helper_serve_client(pipe);
      DisconnectNamedPipe(pipe);
    }

    CloseHandle(pipe);
  }
}
