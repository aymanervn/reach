#include <windows.h>
#include <shlwapi.h>
#include <tlhelp32.h>

#include <cwchar>

static const wchar_t *REACH_PROCESS_NAME = L"reach.exe";
static const int REACH_MAX_FAILED_STARTS = 3;
static const DWORD REACH_STARTUP_GRACE_MS = 5000;
static const DWORD REACH_RESTART_DELAY_MS = 250;

static int reach_watchdog_sibling_path(const wchar_t *file_name,
                                       wchar_t *out_path, DWORD out_count) {
  if (file_name == nullptr || file_name[0] == 0 || out_path == nullptr ||
      out_count == 0) {
    return 0;
  }

  DWORD length = GetModuleFileNameW(nullptr, out_path, out_count);
  if (length == 0 || length >= out_count) {
    return 0;
  }

  if (!PathRemoveFileSpecW(out_path)) {
    return 0;
  }

  if (!PathAppendW(out_path, file_name)) {
    return 0;
  }

  return 1;
}

static int reach_watchdog_working_directory(const wchar_t *path,
                                            wchar_t *out_directory,
                                            DWORD out_count) {
  if (path == nullptr || path[0] == 0 || out_directory == nullptr ||
      out_count == 0) {
    return 0;
  }

  wcscpy_s(out_directory, out_count, path);
  if (!PathRemoveFileSpecW(out_directory)) {
    out_directory[0] = 0;
    return 0;
  }

  return 1;
}

static int reach_watchdog_start_process(const wchar_t *path,
                                        const wchar_t *arguments,
                                        const wchar_t *working_directory) {
  if (path == nullptr || path[0] == 0) {
    return 0;
  }

  wchar_t command_line[1024] = {};
  if (arguments != nullptr && arguments[0] != 0) {
    swprintf_s(command_line, L"\"%ls\" %ls", path, arguments);
  } else {
    swprintf_s(command_line, L"\"%ls\"", path);
  }

  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);

  PROCESS_INFORMATION process = {};
  BOOL ok = CreateProcessW(nullptr, command_line, nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW, nullptr, working_directory,
                           &startup, &process);

  if (!ok) {
    return 0;
  }

  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return 1;
}

static int reach_watchdog_start_process_with_handle(
    const wchar_t *path, const wchar_t *arguments,
    const wchar_t *working_directory, HANDLE *out_process) {
  if (out_process != nullptr) {
    *out_process = nullptr;
  }

  if (path == nullptr || path[0] == 0 || out_process == nullptr) {
    return 0;
  }

  wchar_t command_line[1024] = {};
  if (arguments != nullptr && arguments[0] != 0) {
    swprintf_s(command_line, L"\"%ls\" %ls", path, arguments);
  } else {
    swprintf_s(command_line, L"\"%ls\"", path);
  }

  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);

  PROCESS_INFORMATION process = {};
  BOOL ok = CreateProcessW(nullptr, command_line, nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW, nullptr, working_directory,
                           &startup, &process);

  if (!ok) {
    return 0;
  }

  CloseHandle(process.hThread);
  *out_process = process.hProcess;
  return 1;
}

static int reach_watchdog_open_process_by_name(const wchar_t *process_name,
                                               HANDLE *out_process) {
  if (out_process != nullptr) {
    *out_process = nullptr;
  }

  if (process_name == nullptr || process_name[0] == 0 ||
      out_process == nullptr) {
    return 0;
  }

  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return 0;
  }

  DWORD current_process_id = GetCurrentProcessId();

  PROCESSENTRY32W entry = {};
  entry.dwSize = sizeof(entry);

  int found = 0;
  if (Process32FirstW(snapshot, &entry)) {
    do {
      if (entry.th32ProcessID == current_process_id) {
        continue;
      }

      if (lstrcmpiW(entry.szExeFile, process_name) != 0) {
        continue;
      }

      HANDLE process =
          OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                      entry.th32ProcessID);

      if (process != nullptr) {
        *out_process = process;
        found = 1;
        break;
      }
    } while (Process32NextW(snapshot, &entry));
  }

  CloseHandle(snapshot);
  return found;
}

static void
reach_watchdog_terminate_processes_by_name(const wchar_t *process_name) {
  if (process_name == nullptr || process_name[0] == 0) {
    return;
  }

  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return;
  }

  DWORD current_process_id = GetCurrentProcessId();

  PROCESSENTRY32W entry = {};
  entry.dwSize = sizeof(entry);

  if (Process32FirstW(snapshot, &entry)) {
    do {
      if (entry.th32ProcessID == current_process_id) {
        continue;
      }

      if (lstrcmpiW(entry.szExeFile, process_name) != 0) {
        continue;
      }

      HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE,
                                   entry.th32ProcessID);

      if (process == nullptr) {
        continue;
      }

      if (TerminateProcess(process, 1)) {
        (void)WaitForSingleObject(process, 3000);
      }

      CloseHandle(process);
    } while (Process32NextW(snapshot, &entry));
  }

  CloseHandle(snapshot);
}

static void reach_watchdog_reset_direct_fallback(void) {
  HKEY key = nullptr;
  LONG status = RegCreateKeyExW(
      HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon", 0, nullptr,
      0, KEY_SET_VALUE, nullptr, &key, nullptr);

  if (status == ERROR_SUCCESS) {
    const wchar_t *explorer = L"explorer.exe";
    (void)RegSetValueExW(key, L"Shell", 0, REG_SZ,
                         reinterpret_cast<const BYTE *>(explorer),
                         (DWORD)((wcslen(explorer) + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
  }

  reach_watchdog_terminate_processes_by_name(REACH_PROCESS_NAME);

  wchar_t windows_dir[MAX_PATH] = {};
  UINT windows_length = GetWindowsDirectoryW(windows_dir, MAX_PATH);
  if (windows_length > 0 && windows_length < MAX_PATH) {
    wchar_t explorer_path[MAX_PATH] = {};
    wcscpy_s(explorer_path, windows_dir);
    if (PathAppendW(explorer_path, L"explorer.exe")) {
      (void)reach_watchdog_start_process(explorer_path, nullptr, windows_dir);
    }
  }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  wchar_t reach_path[MAX_PATH] = {};
  if (!reach_watchdog_sibling_path(REACH_PROCESS_NAME, reach_path, MAX_PATH)) {
    reach_watchdog_reset_direct_fallback();
    return 1;
  }

  wchar_t working_directory[MAX_PATH] = {};
  if (!reach_watchdog_working_directory(reach_path, working_directory,
                                        MAX_PATH)) {
    working_directory[0] = 0;
  }

  int failed_starts = 0;

  for (;;) {
    HANDLE reach_process = nullptr;
    if (!reach_watchdog_open_process_by_name(REACH_PROCESS_NAME,
                                             &reach_process)) {
      if (!reach_watchdog_start_process_with_handle(
              reach_path, L"--launch",
              working_directory[0] != 0 ? working_directory : nullptr,
              &reach_process)) {
        ++failed_starts;
        if (failed_starts >= REACH_MAX_FAILED_STARTS) {
          reach_watchdog_reset_direct_fallback();
          return 1;
        }

        Sleep(REACH_RESTART_DELAY_MS);
        continue;
      }
    }

    DWORD startup_wait =
        WaitForSingleObject(reach_process, REACH_STARTUP_GRACE_MS);

    if (startup_wait == WAIT_TIMEOUT) {
      failed_starts = 0;
      DWORD runtime_wait = WaitForSingleObject(reach_process, INFINITE);

      if (runtime_wait == WAIT_OBJECT_0) {
        DWORD exit_code = 1;
        (void)GetExitCodeProcess(reach_process, &exit_code);
        CloseHandle(reach_process);

        if (exit_code == 0) {
          return 0;
        }

        int choice = MessageBoxW(nullptr,
                                 L"Reach exited unexpectedly.\n\n"
                                 L"Yes: restart reach\n"
                                 L"No: restart explorer",
                                 L"Reach Watchdog",
                                 MB_ICONWARNING | MB_YESNO | MB_SETFOREGROUND |
                                     MB_TOPMOST);

        if (choice == IDYES) {
          Sleep(REACH_RESTART_DELAY_MS);
          continue;
        }

        reach_watchdog_reset_direct_fallback();
        return 1;
      }

      CloseHandle(reach_process);
      reach_watchdog_reset_direct_fallback();
      return 1;
    }

    ++failed_starts;
    CloseHandle(reach_process);

    if (failed_starts >= REACH_MAX_FAILED_STARTS) {
      reach_watchdog_terminate_processes_by_name(REACH_PROCESS_NAME);
      reach_watchdog_reset_direct_fallback();
      return 1;
    }

    Sleep(REACH_RESTART_DELAY_MS);
  }
}
