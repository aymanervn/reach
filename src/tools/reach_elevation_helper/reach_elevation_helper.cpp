#include "../../adapters/windows/window_management/elevation_helper_protocol.h"
#include "../../adapters/windows/window_management/window_actions_win32.h"

#include "reach/core/ui_events.h"

#include <windows.h>
#include <sddl.h>

#include <new>
#include <vector>

struct reach_helper_hotkey_event {
  uint32_t version;
  uint32_t event_count;
  uint32_t event_types[2];
};

struct reach_helper_hotkey_state {
  HHOOK hook;
  HANDLE thread;
  DWORD thread_id;
  uint32_t hotkey_mask;
  wchar_t event_pipe[128];
  HANDLE event_pipe_handle;
  int32_t alt_down;
  int32_t shift_down;
  int32_t alt_tab_active;
  int32_t windows_key_down;
  DWORD windows_key_vk;
};

static reach_helper_hotkey_state g_hotkeys;

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

static void reach_helper_close_event_pipe(void) {
  if (g_hotkeys.event_pipe_handle != nullptr) {
    CloseHandle(g_hotkeys.event_pipe_handle);
    g_hotkeys.event_pipe_handle = nullptr;
  }
}

static int32_t reach_helper_connect_event_pipe(void) {
  if (g_hotkeys.event_pipe[0] == 0) {
    return 0;
  }

  if (g_hotkeys.event_pipe_handle != nullptr) {
    return 1;
  }

  for (int attempt = 0; attempt < 20; ++attempt) {
    HANDLE pipe = CreateFileW(g_hotkeys.event_pipe, GENERIC_WRITE, 0, nullptr,
                              OPEN_EXISTING, 0, nullptr);
    if (pipe != INVALID_HANDLE_VALUE) {
      g_hotkeys.event_pipe_handle = pipe;
      return 1;
    }
    if (GetLastError() == ERROR_PIPE_BUSY) {
      (void)WaitNamedPipeW(g_hotkeys.event_pipe, 50);
    } else {
      Sleep(10);
    }
  }

  return 0;
}

static int32_t reach_helper_hotkeys_armed(void) {
  return g_hotkeys.hotkey_mask != 0 && g_hotkeys.event_pipe_handle != nullptr;
}

static void reach_helper_reset_windows_key_state(void) {
  g_hotkeys.windows_key_down = 0;
  g_hotkeys.windows_key_vk = 0;
}

static void reach_helper_disarm_hotkeys_after_channel_failure(void) {
  reach_helper_close_event_pipe();
  g_hotkeys.hotkey_mask = 0;
  g_hotkeys.alt_down = 0;
  g_hotkeys.shift_down = 0;
  g_hotkeys.alt_tab_active = 0;
  reach_helper_reset_windows_key_state();
}

static int32_t reach_helper_send_events(const reach_ui_event_type *event_types,
                                        uint32_t event_count) {
  if (event_types == nullptr || event_count == 0 || event_count > 2) {
    return 0;
  }

  HANDLE pipe = g_hotkeys.event_pipe_handle;
  if (pipe == nullptr || pipe == INVALID_HANDLE_VALUE) {
    return 0;
  }

  reach_helper_hotkey_event event = {};
  event.version = reach_elevation_helper_protocol_version();
  event.event_count = event_count;
  for (uint32_t index = 0; index < event_count; ++index) {
    event.event_types[index] = static_cast<uint32_t>(event_types[index]);
  }

  DWORD written = 0;
  if (!WriteFile(pipe, &event, sizeof(event), &written, nullptr) ||
      written != sizeof(event)) {
    reach_helper_disarm_hotkeys_after_channel_failure();
    return 0;
  }

  return 1;
}

static int32_t reach_helper_send_event(reach_ui_event_type event_type) {
  return reach_helper_send_events(&event_type, 1);
}

static void reach_helper_send_windows_key(DWORD vk, DWORD flags) {
  INPUT input = {};
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = static_cast<WORD>(vk == VK_RWIN ? VK_RWIN : VK_LWIN);
  input.ki.dwFlags = flags | KEYEVENTF_EXTENDEDKEY;
  (void)SendInput(1, &input, sizeof(INPUT));
}

static void reach_helper_release_windows_key_state(void) {
  if (g_hotkeys.windows_key_down && g_hotkeys.windows_key_vk != 0) {
    reach_helper_send_windows_key(g_hotkeys.windows_key_vk, KEYEVENTF_KEYUP);
  }
  reach_helper_reset_windows_key_state();
}

static LRESULT CALLBACK reach_helper_keyboard_proc(int code, WPARAM wparam,
                                                   LPARAM lparam) {
  if (code == HC_ACTION) {
    const KBDLLHOOKSTRUCT *keyboard =
        reinterpret_cast<const KBDLLHOOKSTRUCT *>(lparam);
    if (keyboard != nullptr && (keyboard->flags & LLKHF_INJECTED) == 0 &&
        reach_helper_hotkeys_armed()) {
      const bool key_down = wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN;
      const bool key_up = wparam == WM_KEYUP || wparam == WM_SYSKEYUP;

      if (keyboard->vkCode == VK_SHIFT || keyboard->vkCode == VK_LSHIFT ||
          keyboard->vkCode == VK_RSHIFT) {
        g_hotkeys.shift_down = key_down ? 1 : 0;
      }

      if ((g_hotkeys.hotkey_mask & REACH_ELEVATION_HELPER_HOTKEY_ALT_TAB) !=
          0) {
        if (keyboard->vkCode == VK_MENU || keyboard->vkCode == VK_LMENU ||
            keyboard->vkCode == VK_RMENU) {
          if (key_down) {
            g_hotkeys.alt_down = 1;
          } else if (key_up) {
            g_hotkeys.alt_down = 0;
            if (g_hotkeys.alt_tab_active) {
              g_hotkeys.alt_tab_active = 0;
              (void)reach_helper_send_event(REACH_UI_EVENT_ALT_TAB_COMMIT);
            }
          }
        }

        if (keyboard->vkCode == VK_TAB && key_down &&
            (g_hotkeys.alt_down || (keyboard->flags & LLKHF_ALTDOWN) != 0)) {
          reach_ui_event_type direction = g_hotkeys.shift_down
                                              ? REACH_UI_EVENT_ALT_TAB_PREVIOUS
                                              : REACH_UI_EVENT_ALT_TAB_NEXT;
          int32_t sent = 0;
          if (!g_hotkeys.alt_tab_active) {
            reach_ui_event_type events[2] = {
                REACH_UI_EVENT_ALT_TAB_BEGIN,
                direction,
            };
            sent = reach_helper_send_events(events, 2);
          } else {
            sent = reach_helper_send_event(direction);
          }
          if (sent) {
            g_hotkeys.alt_tab_active = 1;
            return 1;
          }
          g_hotkeys.alt_tab_active = 0;
          reach_helper_disarm_hotkeys_after_channel_failure();
          return CallNextHookEx(g_hotkeys.hook, code, wparam, lparam);
        }

        if (keyboard->vkCode == VK_ESCAPE && g_hotkeys.alt_tab_active &&
            key_down) {
          if (reach_helper_send_event(REACH_UI_EVENT_ALT_TAB_CANCEL)) {
            g_hotkeys.alt_tab_active = 0;
            return 1;
          }
          return CallNextHookEx(g_hotkeys.hook, code, wparam, lparam);
        }
      }

      if ((g_hotkeys.hotkey_mask &
           REACH_ELEVATION_HELPER_HOTKEY_WINDOWS_KEY) != 0) {
        if (keyboard->vkCode == VK_LWIN || keyboard->vkCode == VK_RWIN) {
          if (key_down) {
            if (!g_hotkeys.windows_key_down) {
              g_hotkeys.windows_key_down = 1;
              g_hotkeys.windows_key_vk = keyboard->vkCode;
              reach_helper_send_windows_key(g_hotkeys.windows_key_vk,
                                            KEYEVENTF_KEYUP);
              if (!reach_helper_send_event(REACH_UI_EVENT_WINDOWS_KEY)) {
                reach_helper_disarm_hotkeys_after_channel_failure();
              }
            }
            return 1;
          }
          if (key_up) {
            if (!g_hotkeys.windows_key_down) {
              return CallNextHookEx(g_hotkeys.hook, code, wparam, lparam);
            }
            reach_helper_reset_windows_key_state();
            return 1;
          }
        }
      }
    }
  }

  return CallNextHookEx(g_hotkeys.hook, code, wparam, lparam);
}

static DWORD WINAPI reach_helper_hotkey_thread(void *param) {
  (void)param;
  MSG message = {};
  PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
  g_hotkeys.thread_id = GetCurrentThreadId();
  g_hotkeys.hook = SetWindowsHookExW(WH_KEYBOARD_LL,
                                     reach_helper_keyboard_proc,
                                     GetModuleHandleW(nullptr),
                                     0);
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
  }
  if (g_hotkeys.hook != nullptr) {
    UnhookWindowsHookEx(g_hotkeys.hook);
    g_hotkeys.hook = nullptr;
  }
  return 0;
}

static void reach_helper_stop_hotkeys(void) {
  reach_helper_release_windows_key_state();
  reach_helper_close_event_pipe();
  if (g_hotkeys.thread != nullptr) {
    if (g_hotkeys.thread_id != 0) {
      PostThreadMessageW(g_hotkeys.thread_id, WM_QUIT, 0, 0);
    }
    WaitForSingleObject(g_hotkeys.thread, 1000);
    CloseHandle(g_hotkeys.thread);
    g_hotkeys.thread = nullptr;
  }

  g_hotkeys = {};
}

static reach_result reach_helper_set_hotkey_forwarding(
    const reach_elevation_helper_request *request) {
  if (request == nullptr) {
    return REACH_INVALID_ARGUMENT;
  }

  reach_helper_stop_hotkeys();
  if (request->flags == 0) {
    return REACH_OK;
  }

  g_hotkeys.hotkey_mask = request->hotkey_mask;
  for (size_t index = 0; index < 128; ++index) {
    g_hotkeys.event_pipe[index] = request->event_pipe[index];
    if (request->event_pipe[index] == 0) {
      break;
    }
  }

  if (!reach_helper_connect_event_pipe()) {
    reach_helper_stop_hotkeys();
    return REACH_ERROR;
  }

  g_hotkeys.thread = CreateThread(nullptr, 0, reach_helper_hotkey_thread,
                                  nullptr, 0, nullptr);
  for (int attempt = 0;
       g_hotkeys.thread != nullptr &&
           (g_hotkeys.thread_id == 0 || g_hotkeys.hook == nullptr) &&
           attempt < 20;
       ++attempt) {
    Sleep(10);
  }

  if (g_hotkeys.thread == nullptr || g_hotkeys.hook == nullptr) {
    reach_helper_stop_hotkeys();
    return REACH_ERROR;
  }

  return REACH_OK;
}

static reach_result reach_helper_execute(
    const reach_elevation_helper_request *request) {
  if (!reach_elevation_helper_request_valid(request)) {
    return REACH_INVALID_ARGUMENT;
  }

  if (request->command == REACH_ELEVATION_HELPER_COMMAND_SET_HOTKEY_FORWARDING) {
    return reach_helper_set_hotkey_forwarding(request);
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
