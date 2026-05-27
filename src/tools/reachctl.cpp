#include <windows.h>
#include <shlwapi.h>
#include <tlhelp32.h>
#include <shellapi.h>

#include <cwchar>

#include "reach/platform/windows_adapters.h"
#include "reach/platform/shell_registration.h"
#include "reach/features/pin_config.h"
#include "reach/platform/windows_messages.h"

static void reachctl_print(const wchar_t *message)
{
    if (message == nullptr) {
        return;
    }

    DWORD written = 0;
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!WriteConsoleW(output, message, (DWORD)wcslen(message), &written, nullptr)) {
        char utf8[1024] = {};
        int bytes = WideCharToMultiByte(CP_UTF8, 0, message, -1, utf8, (int)sizeof(utf8) - 3, nullptr, nullptr);
        if (bytes > 0) {
            utf8[bytes - 1] = '\r';
            utf8[bytes] = '\n';
            WriteFile(output, utf8, (DWORD)(bytes + 1), &written, nullptr);
        }
        return;
    }
    WriteConsoleW(output, L"\r\n", 2, &written, nullptr);
}

static reach_result reachctl_target_exe(uint16_t *path, DWORD path_count)
{
    if (path == nullptr || path_count == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD length = GetModuleFileNameW(nullptr, reinterpret_cast<wchar_t *>(path), path_count);
    if (length == 0 || length >= path_count) {
        return REACH_ERROR;
    }

    wchar_t *path_w = reinterpret_cast<wchar_t *>(path);
    if (!PathRemoveFileSpecW(path_w)) {
        return REACH_ERROR;
    }
    if (!PathAppendW(path_w, L"reach.exe")) {
        return REACH_ERROR;
    }
    return REACH_OK;
}

static reach_result reachctl_current_exe(uint16_t *path, DWORD path_count)
{
    if (path == nullptr || path_count == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD length = GetModuleFileNameW(nullptr, reinterpret_cast<wchar_t *>(path), path_count);
    return length > 0 && length < path_count ? REACH_OK : REACH_ERROR;
}

static reach_result reachctl_open_config_store(reach_config_store_port *out_store)
{
    uint16_t config_path[260] = {};
    reach_result result = reach_windows_default_config_path(config_path, 260);
    if (result != REACH_OK) {
        return result;
    }
    return reach_windows_create_config_store(config_path, out_store);
}

static reach_result reachctl_write_string_value(HKEY root, const wchar_t *key, const wchar_t *name, const wchar_t *value)
{
    HKEY handle = nullptr;
    LONG status = RegCreateKeyExW(root, key, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &handle, nullptr);
    if (status != ERROR_SUCCESS) {
        return REACH_ERROR;
    }

    status = RegSetValueExW(
        handle,
        name,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE *>(value),
        (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));

    RegCloseKey(handle);
    return status == ERROR_SUCCESS ? REACH_OK : REACH_ERROR;
}

static reach_result reachctl_delete_tree(HKEY root, const wchar_t *key)
{
    LONG status = RegDeleteTreeW(root, key);
    return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND ? REACH_OK : REACH_ERROR;
}

static int32_t reachctl_is_process_elevated(void)
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return 0;
    }

    TOKEN_ELEVATION elevation = {};
    DWORD size = sizeof(elevation);
    BOOL ok = GetTokenInformation(
        token,
        TokenElevation,
        &elevation,
        sizeof(elevation),
        &size);

    CloseHandle(token);
    return ok && elevation.TokenIsElevated;
}

static reach_result reachctl_terminate_processes_by_name(const wchar_t *process_name)
{
    if (process_name == nullptr || process_name[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return REACH_ERROR;
    }

    DWORD current_process_id = GetCurrentProcessId();
    reach_result result = REACH_OK;

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

            HANDLE process = OpenProcess(
                PROCESS_TERMINATE | SYNCHRONIZE,
                FALSE,
                entry.th32ProcessID);

            if (process == nullptr) {
                result = REACH_ERROR;
                continue;
            }

            if (!TerminateProcess(process, 0)) {
                result = REACH_ERROR;
            } else {
                (void)WaitForSingleObject(process, 3000);
            }

            CloseHandle(process);
        } while (Process32NextW(snapshot, &entry));
    } else {
        result = REACH_ERROR;
    }

    CloseHandle(snapshot);
    return result;
}

static reach_result reachctl_start_process(
    const wchar_t *path,
    const wchar_t *arguments,
    const wchar_t *working_directory)
{
    if (path == nullptr || path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
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
    BOOL ok = CreateProcessW(
        nullptr,
        command_line,
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        working_directory,
        &startup,
        &process);

    if (!ok) {
        return REACH_ERROR;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return REACH_OK;
}

static reach_result reachctl_run_process_wait(
    const wchar_t *path,
    const wchar_t *arguments,
    const wchar_t *working_directory)
{
    if (path == nullptr || path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
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
    BOOL ok = CreateProcessW(
        nullptr,
        command_line,
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        working_directory,
        &startup,
        &process);

    if (!ok) {
        return REACH_ERROR;
    }

    CloseHandle(process.hThread);

    DWORD wait_result = WaitForSingleObject(process.hProcess, 15000);

    DWORD exit_code = 1;
    if (wait_result == WAIT_OBJECT_0) {
        (void)GetExitCodeProcess(process.hProcess, &exit_code);
    }

    CloseHandle(process.hProcess);

    return wait_result == WAIT_OBJECT_0 && exit_code == 0
        ? REACH_OK
        : REACH_ERROR;
}

static reach_result reachctl_watchdog_exe(uint16_t *path, DWORD path_count)
{
    if (path == nullptr || path_count == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD length = GetModuleFileNameW(nullptr, reinterpret_cast<wchar_t *>(path), path_count);
    if (length == 0 || length >= path_count) {
        return REACH_ERROR;
    }

    wchar_t *path_w = reinterpret_cast<wchar_t *>(path);
    if (!PathRemoveFileSpecW(path_w)) {
        return REACH_ERROR;
    }

    if (!PathAppendW(path_w, L"reach-watchdog.exe")) {
        return REACH_ERROR;
    }

    return REACH_OK;
}

static reach_result reachctl_register_watchdog_task(void)
{
    uint16_t watchdog_path_u16[260] = {};
    reach_result path_result = reachctl_watchdog_exe(watchdog_path_u16, 260);
    if (path_result != REACH_OK) {
        return path_result;
    }

    const wchar_t *watchdog_path =
        reinterpret_cast<const wchar_t *>(watchdog_path_u16);

    wchar_t arguments[1024] = {};
    swprintf_s(
        arguments,
        L"/Create /F /SC ONLOGON /TN \"ReachWatchdog\" /TR \"\\\"%ls\\\"\" /RL LIMITED",
        watchdog_path);

    return reachctl_run_process_wait(
        L"C:\\Windows\\System32\\schtasks.exe",
        arguments,
        nullptr);
}

static reach_result reachctl_unregister_watchdog_task(void)
{
    return reachctl_start_process(
        L"C:\\Windows\\System32\\schtasks.exe",
        L"/Delete /F /TN \"ReachWatchdog\"",
        nullptr);
}

static BOOL CALLBACK reachctl_reset_monitor_work_area_proc(
    HMONITOR monitor,
    HDC dc,
    LPRECT rect,
    LPARAM param)
{
    (void)dc;
    (void)rect;

    reach_result *result = reinterpret_cast<reach_result *>(param);
    if (result == nullptr) {
        return FALSE;
    }

    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        *result = REACH_ERROR;
        return TRUE;
    }

    RECT work_area = info.rcMonitor;
    if (!SystemParametersInfoW(
            SPI_SETWORKAREA,
            0,
            &work_area,
            SPIF_SENDCHANGE)) {
        *result = REACH_ERROR;
    }

    return TRUE;
}

static reach_result reachctl_reset_monitor_work_areas(void)
{
    reach_result result = REACH_OK;
    if (!EnumDisplayMonitors(
            nullptr,
            nullptr,
            reachctl_reset_monitor_work_area_proc,
            reinterpret_cast<LPARAM>(&result))) {
        return REACH_ERROR;
    }

    return result;
}

static BOOL CALLBACK reachctl_repair_maximized_window_proc(HWND hwnd, LPARAM param)
{
    (void)param;

    if (hwnd == nullptr || !IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
        return TRUE;
    }

    wchar_t class_name[64] = {};
    GetClassNameW(hwnd, class_name, 64);
    if (lstrcmpiW(class_name, L"Shell_TrayWnd") == 0 ||
        lstrcmpiW(class_name, L"Shell_SecondaryTrayWnd") == 0 ||
        lstrcmpiW(class_name, L"Progman") == 0 ||
        lstrcmpiW(class_name, L"WorkerW") == 0) {
        return TRUE;
    }

    if (!IsZoomed(hwnd)) {
        return TRUE;
    }

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (monitor == nullptr) {
        return TRUE;
    }

    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        return TRUE;
    }

    int width = info.rcMonitor.right - info.rcMonitor.left;
    int height = info.rcMonitor.bottom - info.rcMonitor.top;

    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    if (GetWindowPlacement(hwnd, &placement)) {
        placement.rcNormalPosition = info.rcMonitor;
        (void)SetWindowPlacement(hwnd, &placement);
    }

    (void)SetWindowPos(
        hwnd,
        nullptr,
        info.rcMonitor.left,
        info.rcMonitor.top,
        width,
        height,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    return TRUE;
}

static reach_result reachctl_repair_maximized_windows(void)
{
    return EnumWindows(reachctl_repair_maximized_window_proc, 0)
        ? REACH_OK
        : REACH_ERROR;
}

static reach_result reachctl_start_explorer_shell(void)
{
    wchar_t windows_dir[MAX_PATH] = {};
    UINT windows_length = GetWindowsDirectoryW(windows_dir, MAX_PATH);
    if (windows_length == 0 || windows_length >= MAX_PATH) {
        return REACH_ERROR;
    }

    wchar_t explorer_path[MAX_PATH] = {};
    wcscpy_s(explorer_path, windows_dir);
    if (!PathAppendW(explorer_path, L"explorer.exe")) {
        return REACH_ERROR;
    }

    return reachctl_start_process(explorer_path, nullptr, windows_dir);
}

static reach_result reachctl_start_userinit(void)
{
    wchar_t system_dir[MAX_PATH] = {};
    UINT system_length = GetSystemDirectoryW(system_dir, MAX_PATH);
    if (system_length == 0 || system_length >= MAX_PATH) {
        return REACH_ERROR;
    }

    wchar_t userinit_path[MAX_PATH] = {};
    wcscpy_s(userinit_path, system_dir);
    if (!PathAppendW(userinit_path, L"userinit.exe")) {
        return REACH_ERROR;
    }

    return reachctl_start_process(userinit_path, nullptr, system_dir);
}

static reach_result reachctl_run_watchdog_task(void)
{
    HINSTANCE result = ShellExecuteW(
        nullptr,
        L"open",
        L"C:\\Windows\\System32\\schtasks.exe",
        L"/Run /TN \"ReachWatchdog\"",
        nullptr,
        SW_HIDE);

    return reinterpret_cast<intptr_t>(result) > 32
        ? REACH_OK
        : REACH_ERROR;
}

static reach_result reachctl_start_reach_session(const uint16_t *reach_exe)
{
    (void)reach_exe;

    reach_result kill_reach_result =
        reachctl_terminate_processes_by_name(L"reach.exe");

    reach_result kill_explorer_result =
        reachctl_terminate_processes_by_name(L"explorer.exe");

    reach_result work_area_result = reachctl_reset_monitor_work_areas();
    reach_result repair_result = reachctl_repair_maximized_windows();

    reach_result start_result = reachctl_run_watchdog_task();

    if (kill_reach_result != REACH_OK ||
        kill_explorer_result != REACH_OK ||
        work_area_result != REACH_OK ||
        repair_result != REACH_OK ||
        start_result != REACH_OK) {
        return REACH_ERROR;
    }

    return REACH_OK;
}

static reach_result reachctl_absolute_path(const uint16_t *path, uint16_t *out_path, DWORD out_path_count)
{
    if (path == nullptr || path[0] == 0 || out_path == nullptr || out_path_count == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    wchar_t full_path[260] = {};
    DWORD length = GetFullPathNameW(reinterpret_cast<const wchar_t *>(path), 260, full_path, nullptr);
    if (length == 0 || length >= out_path_count) {
        return REACH_ERROR;
    }

    return reach_copy_utf16(out_path, out_path_count, reinterpret_cast<const uint16_t *>(full_path));
}

struct reachctl_notify_config_state {
    int32_t posted;
};

static BOOL CALLBACK reachctl_notify_config_window_proc(HWND hwnd, LPARAM param)
{
    reachctl_notify_config_state *state =
        reinterpret_cast<reachctl_notify_config_state *>(param);

    if (state == nullptr || hwnd == nullptr || !IsWindow(hwnd)) {
        return TRUE;
    }

    wchar_t class_name[64] = {};
    GetClassNameW(hwnd, class_name, 64);

    if (lstrcmpiW(class_name, L"ReachPlatformWindow") != 0) {
        return TRUE;
    }

    if (PostMessageW(hwnd, REACH_WM_CONFIG_CHANGED, 0, 0)) {
        state->posted = 1;
        return FALSE;
    }

    return TRUE;
}

static reach_result reachctl_notify_config_changed(void)
{
    reachctl_notify_config_state state = {};
    if (!EnumWindows(reachctl_notify_config_window_proc, reinterpret_cast<LPARAM>(&state))) {
        return state.posted ? REACH_OK : REACH_ERROR;
    }

    return state.posted ? REACH_OK : REACH_ERROR;
}

static int32_t reachctl_is_supported_pin_path(const uint16_t *path)
{
    if (path == nullptr || path[0] == 0) {
        return 0;
    }

    const wchar_t *path_w = reinterpret_cast<const wchar_t *>(path);
    const wchar_t *extension = PathFindExtensionW(path_w);

    return lstrcmpiW(extension, L".exe") == 0 ||
        lstrcmpiW(extension, L".lnk") == 0;
}

static int32_t reachctl_path_equals_ci(const uint16_t *a, const uint16_t *b)
{
    if (a == nullptr || b == nullptr) {
        return 0;
    }

    size_t index = 0;
    while (a[index] != 0 && b[index] != 0) {
        uint16_t ca = a[index];
        uint16_t cb = b[index];

        if (ca >= 'A' && ca <= 'Z') {
            ca = (uint16_t)(ca + ('a' - 'A'));
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (uint16_t)(cb + ('a' - 'A'));
        }

        if (ca != cb) {
            return 0;
        }

        ++index;
    }

    return a[index] == b[index];
}

static reach_result reachctl_path_is_already_pinned(
    reach_config_store_port *store,
    const uint16_t *path,
    int32_t *out_pinned)
{
    if (store == nullptr || store->ops.load == nullptr ||
        path == nullptr || path[0] == 0 ||
        out_pinned == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_pinned = 0;

    reach_config_snapshot snapshot = {};
    reach_result result = store->ops.load(store->store, &snapshot);
    if (result != REACH_OK) {
        return result;
    }

    for (size_t index = 0; index < snapshot.pinned_app_count; ++index) {
        if (reachctl_path_equals_ci(snapshot.pinned_apps[index].path, path)) {
            *out_pinned = 1;
            return REACH_OK;
        }
    }

    return REACH_OK;
}

static int reachctl_pin_command(const wchar_t *path)
{
    if (path == nullptr || path[0] == 0) {
        reachctl_print(L"--pin requires a path.");
        return 2;
    }

    uint16_t absolute_path[260] = {};
    reach_result path_result = reachctl_absolute_path(
        reinterpret_cast<const uint16_t *>(path),
        absolute_path,
        260);

    if (path_result != REACH_OK || absolute_path[0] == 0) {
        reachctl_print(L"Could not resolve path.");
        return 1;
    }

    if (!reachctl_is_supported_pin_path(absolute_path)) {
        reachctl_print(L"Only .exe and .lnk files can be pinned to Reach.");
        return 1;
    }

    reach_config_store_port store = {};
    reach_result store_result = reachctl_open_config_store(&store);
    if (store_result != REACH_OK) {
        reachctl_print(L"Could not open Reach config.");
        return 1;
    }
    int32_t already_pinned = 0;
    reach_result already_result = reachctl_path_is_already_pinned(
        &store,
        absolute_path,
        &already_pinned);

    if (already_result != REACH_OK) {
        if (store.ops.destroy != nullptr) {
            store.ops.destroy(store.store);
        }
        return 1;
    }

    if (already_pinned) {
        if (store.ops.destroy != nullptr) {
            store.ops.destroy(store.store);
        }
        return 0;
    }
    reach_result pin_result = reach_pin_config_pin_path(&store, absolute_path);

    if (store.ops.destroy != nullptr) {
        store.ops.destroy(store.store);
    }

    if (pin_result != REACH_OK) {
        reachctl_print(L"Could not pin app to Reach dock.");
        return 1;
    }

    reach_result notify_result = reachctl_notify_config_changed();

    reachctl_print(L"App pinned to Reach dock.");
    return 0;
}

static reach_result reachctl_install_pin_context_menu_for_class(
    const wchar_t *class_name,
    const wchar_t *reachctl_path,
    const wchar_t *icon_path)
{
    if (class_name == nullptr || reachctl_path == nullptr || icon_path == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    wchar_t verb_key[512] = {};
    swprintf_s(
        verb_key,
        L"Software\\Classes\\%ls\\shell\\ReachPin",
        class_name);

    wchar_t command_key[512] = {};
    swprintf_s(
        command_key,
        L"Software\\Classes\\%ls\\shell\\ReachPin\\command",
        class_name);

    wchar_t command[1024] = {};
    swprintf_s(
        command,
        L"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -Command \"Start-Process -WindowStyle Hidden -FilePath '%ls' -ArgumentList '--pin', '%%1'\"",
        reachctl_path);

    reach_result result = reachctl_write_string_value(
        HKEY_CURRENT_USER,
        verb_key,
        L"MUIVerb",
        L"Pin to Reach dock");

    if (result != REACH_OK) {
        return result;
    }

    result = reachctl_write_string_value(
        HKEY_CURRENT_USER,
        verb_key,
        L"Icon",
        icon_path);

    if (result != REACH_OK) {
        return result;
    }

    return reachctl_write_string_value(
        HKEY_CURRENT_USER,
        command_key,
        nullptr,
        command);
}

static reach_result reachctl_install_pin_context_menu(void)
{
    uint16_t reachctl_path_u16[260] = {};
    reach_result current_result = reachctl_current_exe(reachctl_path_u16, 260);
    if (current_result != REACH_OK) {
        return current_result;
    }

    uint16_t reach_exe_u16[260] = {};
    reach_result reach_result_value = reachctl_target_exe(reach_exe_u16, 260);
    if (reach_result_value != REACH_OK) {
        return reach_result_value;
    }

    const wchar_t *reachctl_path =
        reinterpret_cast<const wchar_t *>(reachctl_path_u16);
    const wchar_t *reach_exe =
        reinterpret_cast<const wchar_t *>(reach_exe_u16);

    reach_result exe_result = reachctl_install_pin_context_menu_for_class(
        L"exefile",
        reachctl_path,
        reach_exe);

    reach_result lnk_result = reachctl_install_pin_context_menu_for_class(
        L"lnkfile",
        reachctl_path,
        reach_exe);

    return exe_result == REACH_OK && lnk_result == REACH_OK
        ? REACH_OK
        : REACH_ERROR;
}

static reach_result reachctl_remove_pin_context_menu(void)
{
    reach_result exe_result = reachctl_delete_tree(
        HKEY_CURRENT_USER,
        L"Software\\Classes\\exefile\\shell\\ReachPin");

    reach_result lnk_result = reachctl_delete_tree(
        HKEY_CURRENT_USER,
        L"Software\\Classes\\lnkfile\\shell\\ReachPin");

    return exe_result == REACH_OK && lnk_result == REACH_OK
        ? REACH_OK
        : REACH_ERROR;
}

static reach_result reachctl_install_reach_shell_and_watchdog(const uint16_t *reach_exe)
{
    if (reach_exe == nullptr || reach_exe[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    if (!reachctl_is_process_elevated()) {
        return REACH_ERROR;
    }

    reach_result task_result = reachctl_register_watchdog_task();
    if (task_result != REACH_OK) {
        return task_result;
    }

    reach_result context_result = reachctl_install_pin_context_menu();
    if (context_result != REACH_OK) {
        return context_result;
    }
    return reach_windows_shell_install_current_user(reach_exe);
}

static reach_result reachctl_reset_to_windows_shell(void)
{
    reach_result restore_result = reach_windows_shell_restore_current_user();

    (void)reachctl_unregister_watchdog_task();

    (void)reachctl_terminate_processes_by_name(L"reach.exe");
    (void)reachctl_terminate_processes_by_name(L"reach-watchdog.exe");
    (void)reachctl_terminate_processes_by_name(L"explorer.exe");
    (void)reachctl_remove_pin_context_menu();

    reach_result start_result = reachctl_start_userinit();
    if (start_result != REACH_OK) {
        start_result = reachctl_start_explorer_shell();
    }

    if (restore_result != REACH_OK || start_result != REACH_OK) {
        return REACH_ERROR;
    }

    return REACH_OK;
}

struct reachctl_monitor_list_state {
    size_t index;
};

static BOOL CALLBACK reachctl_print_monitor_proc(HMONITOR monitor, HDC dc, LPRECT rect, LPARAM param)
{
    (void)monitor;
    (void)dc;

    reachctl_monitor_list_state *state = reinterpret_cast<reachctl_monitor_list_state *>(param);
    if (state == nullptr || rect == nullptr) {
        return TRUE;
    }

    wchar_t line[256] = {};
    swprintf_s(
        line,
        L"%u: x=%ld y=%ld width=%ld height=%ld",
        (unsigned)(state->index + 1),
        rect->left,
        rect->top,
        rect->right - rect->left,
        rect->bottom - rect->top);

    reachctl_print(line);
    state->index += 1;
    return TRUE;
}

static reach_result reachctl_list_monitors(void)
{
    reachctl_monitor_list_state state = {};
    BOOL ok = EnumDisplayMonitors(nullptr, nullptr, reachctl_print_monitor_proc, reinterpret_cast<LPARAM>(&state));
    return ok && state.index > 0 ? REACH_OK : REACH_ERROR;
}

int wmain(int argc, wchar_t **argv)
{
    uint16_t reach_exe[260] = {};
    if (reachctl_target_exe(reach_exe, 260) != REACH_OK) {
        reachctl_print(L"Could not locate sibling reach.exe.");
        return 1;
    }

    for (int index = 1; index < argc; ++index) {
        if (lstrcmpiW(argv[index], L"--install") == 0) {
            if (!reachctl_is_process_elevated()) {
                reachctl_print(
                    L"Reach install requires Administrator privileges to create the watchdog scheduled task. "
                    L"Open PowerShell as Administrator and run reachctl --install again.");
                return 1;
            }

            int ok = reachctl_install_reach_shell_and_watchdog(reach_exe) == REACH_OK;
            reachctl_print(ok
                ? L"Reach installed. Shell registry and watchdog task configured."
                : L"Reach install failed.");
            return ok ? 0 : 1;
        }

        if (lstrcmpiW(argv[index], L"--start") == 0) {
            int ok = reachctl_start_reach_session(reach_exe) == REACH_OK;
            reachctl_print(ok
                ? L"Reach started for current session."
                : L"Reach start failed.");
            return ok ? 0 : 1;
        }

        if (lstrcmpiW(argv[index], L"--reset") == 0) {
            if (!reachctl_is_process_elevated()) {
                reachctl_print(
                    L"Reach reset requires Administrator privileges to remove the watchdog scheduled task. "
                    L"Open PowerShell as Administrator and run reachctl --reset again.");
                return 1;
            }
            int ok = reachctl_reset_to_windows_shell() == REACH_OK;
            reachctl_print(ok
                ? L"Reach reset complete. Windows shell restored."
                : L"Reach reset attempted, but one or more steps failed.");
            return ok ? 0 : 1;
        }

        if (lstrcmpiW(argv[index], L"--list-monitors") == 0) {
            int ok = reachctl_list_monitors() == REACH_OK;
            if (!ok) {
                reachctl_print(L"Monitor query failed.");
            }
            return ok ? 0 : 1;
        }

        if (lstrcmpiW(argv[index], L"--pin") == 0) {
            if (index + 1 >= argc) {
                reachctl_print(L"--pin requires a path.");
                return 2;
            }

            return reachctl_pin_command(argv[index + 1]);
        }
    }

    reachctl_print(
        L"Usage: reachctl.exe\n"
        L"  --install\n"
        L"  --start\n"
        L"  --reset\n"
        L"  --list-monitors\n"
        L"  --pin <path>\n");
    return 2;
}
