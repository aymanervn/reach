#include "window_actions_win32.h"

static HWND reach_window_management_activation_target(HWND hwnd) {
  if (hwnd == nullptr || !IsWindow(hwnd)) {
    return nullptr;
  }

  HWND target = hwnd;
  HWND root_owner = GetAncestor(hwnd, GA_ROOTOWNER);
  if (root_owner != nullptr && IsWindow(root_owner)) {
    target = root_owner;
  }

  HWND popup = GetLastActivePopup(target);
  if (popup != nullptr && IsWindow(popup) && IsWindowVisible(popup)) {
    target = popup;
  }

  return target;
}

struct reach_thread_input_scope {
  DWORD current_thread;
  DWORD attached_thread;
  int32_t attached;

  reach_thread_input_scope(DWORD current, DWORD target)
      : current_thread(current), attached_thread(target), attached(0) {
    if (attached_thread != 0 && attached_thread != current_thread) {
      attached =
          AttachThreadInput(current_thread, attached_thread, TRUE) != FALSE;
    }
  }

  reach_thread_input_scope(const reach_thread_input_scope &) = delete;
  reach_thread_input_scope &
  operator=(const reach_thread_input_scope &) = delete;

  ~reach_thread_input_scope() {
    if (attached) {
      AttachThreadInput(current_thread, attached_thread, FALSE);
      attached = 0;
    }
  }
};

reach_result reach_window_management_activate(HWND hwnd) {
  if (hwnd == nullptr || !IsWindow(hwnd)) {
    return REACH_INVALID_ARGUMENT;
  }

  if (IsIconic(hwnd)) {
    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    const bool restore_to_maximized =
        GetWindowPlacement(hwnd, &placement) &&
        (placement.showCmd == SW_SHOWMAXIMIZED ||
         (placement.flags & WPF_RESTORETOMAXIMIZED) != 0);

    ShowWindow(hwnd, restore_to_maximized ? SW_SHOWMAXIMIZED : SW_RESTORE);
  } else if (!IsWindowVisible(hwnd)) {
    ShowWindow(hwnd, SW_SHOW);
  }

  HWND target = reach_window_management_activation_target(hwnd);
  if (target == nullptr) {
    return REACH_ERROR;
  }

  HWND foreground = GetForegroundWindow();
  DWORD foreground_thread =
      foreground != nullptr ? GetWindowThreadProcessId(foreground, nullptr) : 0;
  DWORD target_thread = GetWindowThreadProcessId(target, nullptr);
  DWORD current_thread = GetCurrentThreadId();

  reach_thread_input_scope foreground_attachment(current_thread,
                                                 foreground_thread);
  reach_thread_input_scope target_attachment(
      current_thread, target_thread != foreground_thread ? target_thread : 0);

  SetWindowPos(target, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  SetWindowPos(target, HWND_NOTOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  BringWindowToTop(target);

  BOOL foreground_ok = SetForegroundWindow(target);
  HWND actual_foreground = GetForegroundWindow();
  return foreground_ok || actual_foreground == target ? REACH_OK : REACH_ERROR;
}

reach_result reach_window_management_minimize(HWND hwnd) {
  if (hwnd == nullptr || !IsWindow(hwnd)) {
    return REACH_INVALID_ARGUMENT;
  }

  HWND target = reach_window_management_activation_target(hwnd);
  if (target == nullptr) {
    target = hwnd;
  }

  ShowWindowAsync(target, SW_FORCEMINIMIZE);
  PostMessageW(target, WM_SYSCOMMAND, SC_MINIMIZE, 0);
  CloseWindow(target);
  return REACH_OK;
}

reach_result reach_window_management_hide(HWND hwnd) {
  if (hwnd == nullptr || !IsWindow(hwnd)) {
    return REACH_INVALID_ARGUMENT;
  }
  ShowWindowAsync(hwnd, SW_HIDE);
  return REACH_OK;
}

reach_result reach_window_management_snap(HWND hwnd, reach_split_mode mode) {
  if (hwnd == nullptr || !IsWindow(hwnd)) {
    return REACH_INVALID_ARGUMENT;
  }

  HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  MONITORINFO info = {};
  info.cbSize = sizeof(info);
  if (!GetMonitorInfoW(monitor, &info)) {
    return REACH_ERROR;
  }

  reach_rect_i32 work_area = {info.rcWork.left, info.rcWork.top,
                              info.rcWork.right, info.rcWork.bottom};
  reach_rect_i32 target = {};
  reach_result result = reach_layout_compute_split(work_area, mode, &target);
  if (result != REACH_OK) {
    return result;
  }

  if (IsZoomed(hwnd)) {
    ShowWindow(hwnd, SW_RESTORE);
  }

  BOOL ok = SetWindowPos(hwnd, nullptr, target.left, target.top,
                         target.right - target.left, target.bottom - target.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
  return ok ? REACH_OK : REACH_ERROR;
}

reach_result reach_window_management_close(HWND hwnd) {
  if (hwnd == nullptr || !IsWindow(hwnd)) {
    return REACH_INVALID_ARGUMENT;
  }
  return PostMessageW(hwnd, WM_CLOSE, 0, 0) ? REACH_OK : REACH_ERROR;
}
