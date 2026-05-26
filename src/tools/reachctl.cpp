#include <windows.h>
#include <shlwapi.h>
#include <tlhelp32.h>

#include <cwchar>

#include "reach/features/pin_config.h"
#include "reach/platform/windows_adapters.h"
#include "reach/platform/windows_messages.h"
#include "reach/platform/shell_registration.h"

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

static reach_result reachctl_start_reach_shell(const uint16_t *reach_exe)
{
    if (reach_exe == nullptr || reach_exe[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    wchar_t path[MAX_PATH] = {};
    if (reach_copy_utf16(reinterpret_cast<uint16_t *>(path), MAX_PATH, reach_exe) != REACH_OK) {
        return REACH_ERROR;
    }

    wchar_t working_directory[MAX_PATH] = {};
    wcscpy_s(working_directory, path);
    if (!PathRemoveFileSpecW(working_directory)) {
        working_directory[0] = 0;
    }

    return reachctl_start_process(
        path,
        L"--launch",
        working_directory[0] != 0 ? working_directory : nullptr);
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

static reach_result reachctl_install_reach_shell_and_switch(const uint16_t *reach_exe)
{
    reach_result install_result = reach_windows_shell_install_current_user(reach_exe);

    reach_result kill_reach_result =
        reachctl_terminate_processes_by_name(L"reach.exe");

    reach_result kill_explorer_result =
        reachctl_terminate_processes_by_name(L"explorer.exe");
    reach_result work_area_result = reachctl_reset_monitor_work_areas();
    reach_result repair_result = reachctl_repair_maximized_windows();
    reach_result start_reach_result = reachctl_start_reach_shell(reach_exe);

    if (install_result != REACH_OK ||
        kill_reach_result != REACH_OK ||
        kill_explorer_result != REACH_OK ||
        work_area_result != REACH_OK ||
        repair_result != REACH_OK ||
        start_reach_result != REACH_OK) {
        return REACH_ERROR;
    }

    return REACH_OK;
}

static reach_result reachctl_reset_to_windows_shell(void)
{
    reach_result restore_result = reach_windows_shell_restore_current_user();

    reach_result kill_reach_result =
        reachctl_terminate_processes_by_name(L"reach.exe");

    reach_result kill_explorer_result =
        reachctl_terminate_processes_by_name(L"explorer.exe");

    Sleep(750);

    reach_result start_result = reachctl_start_userinit();
    if (start_result != REACH_OK) {
        start_result = reachctl_start_explorer_shell();
    }

    if (restore_result != REACH_OK ||
        kill_reach_result != REACH_OK ||
        kill_explorer_result != REACH_OK ||
        start_result != REACH_OK) {
        return REACH_ERROR;
    }

    return REACH_OK;
}

static reach_result reachctl_install_context_menu(void)
{
    uint16_t ctl_path[260] = {};
    if (reachctl_current_exe(ctl_path, 260) != REACH_OK) {
        return REACH_ERROR;
    }

    wchar_t command[640] = {};
    swprintf_s(command, L"\"%ls\" --pin-path \"%%1\"", reinterpret_cast<const wchar_t *>(ctl_path));

    const wchar_t *targets[] = {
        L"Software\\Classes\\*\\shell\\Reach.PinToDock",
        L"Software\\Classes\\Directory\\shell\\Reach.PinToDock",
        L"Software\\Classes\\exefile\\shell\\Reach.PinToDock"
    };

    for (size_t index = 0; index < sizeof(targets) / sizeof(targets[0]); ++index) {
        wchar_t command_key[260] = {};
        swprintf_s(command_key, L"%ls\\command", targets[index]);

        if (reachctl_write_string_value(HKEY_CURRENT_USER, targets[index], nullptr, L"Pin app to dock") != REACH_OK ||
            reachctl_write_string_value(HKEY_CURRENT_USER, targets[index], L"Icon", reinterpret_cast<const wchar_t *>(ctl_path)) != REACH_OK ||
            reachctl_write_string_value(HKEY_CURRENT_USER, command_key, nullptr, command) != REACH_OK) {
            return REACH_ERROR;
        }
    }

    return REACH_OK;
}

static reach_result reachctl_remove_context_menu(void)
{
    const wchar_t *targets[] = {
        L"Software\\Classes\\*\\shell\\Reach.PinToDock",
        L"Software\\Classes\\Directory\\shell\\Reach.PinToDock",
        L"Software\\Classes\\exefile\\shell\\Reach.PinToDock"
    };

    reach_result result = REACH_OK;
    for (size_t index = 0; index < sizeof(targets) / sizeof(targets[0]); ++index) {
        if (reachctl_delete_tree(HKEY_CURRENT_USER, targets[index]) != REACH_OK) {
            result = REACH_ERROR;
        }
    }

    return result;
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

static void reachctl_notify_wallpaper_changed(void)
{
    HWND hwnd = nullptr;
    while ((hwnd = FindWindowExW(nullptr, hwnd, L"ReachPlatformWindow", nullptr)) != nullptr) {
        PostMessageW(hwnd, REACH_WM_WALLPAPER_CHANGED, 0, 0);
    }
}

static void reachctl_notify_config_changed(void)
{
    HWND hwnd = nullptr;
    while ((hwnd = FindWindowExW(nullptr, hwnd, L"ReachPlatformWindow", nullptr)) != nullptr) {
        PostMessageW(hwnd, REACH_WM_CONFIG_CHANGED, 0, 0);
    }
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

static reach_result reachctl_set_wallpaper(const uint16_t *path)
{
    if (path == nullptr || path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    uint16_t full_path[260] = {};
    reach_result result = reachctl_absolute_path(path, full_path, 260);
    if (result != REACH_OK) {
        return result;
    }

    DWORD attributes = GetFileAttributesW(reinterpret_cast<const wchar_t *>(full_path));
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_config_store_port store = {};
    result = reachctl_open_config_store(&store);

    reach_config_snapshot snapshot = {};
    if (result == REACH_OK) {
        result = store.ops.load(store.store, &snapshot);
    }
    if (result == REACH_OK) {
        result = reach_copy_utf16(snapshot.wallpaper_path, 260, full_path);
    }
    if (result == REACH_OK) {
        result = store.ops.save(store.store, &snapshot);
    }

    reach_wallpaper_service_port wallpaper = {};
    if (result == REACH_OK) {
        result = reach_windows_create_wallpaper_service(&wallpaper);
    }
    if (result == REACH_OK) {
        result = wallpaper.ops.set_wallpaper(wallpaper.service, full_path);
    }

    if (wallpaper.ops.destroy != nullptr) {
        wallpaper.ops.destroy(wallpaper.service);
    }
    if (store.ops.destroy != nullptr) {
        store.ops.destroy(store.store);
    }

    if (result == REACH_OK) {
        reachctl_notify_wallpaper_changed();
    }

    return result;
}

static reach_result reachctl_set_monitor_wallpaper(size_t monitor_index, const uint16_t *path)
{
    if (monitor_index == 0 || monitor_index > REACH_MAX_WALLPAPER_MONITORS || path == nullptr || path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    size_t zero_based_index = monitor_index - 1;

    uint16_t full_path[260] = {};
    reach_result result = reachctl_absolute_path(path, full_path, 260);
    if (result != REACH_OK) {
        return result;
    }

    DWORD attributes = GetFileAttributesW(reinterpret_cast<const wchar_t *>(full_path));
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_config_store_port store = {};
    result = reachctl_open_config_store(&store);

    reach_config_snapshot snapshot = {};
    if (result == REACH_OK) {
        result = store.ops.load(store.store, &snapshot);
    }
    if (result == REACH_OK) {
        result = reach_copy_utf16(snapshot.monitor_wallpaper_paths[zero_based_index], 260, full_path);
    }
    if (result == REACH_OK) {
        result = store.ops.save(store.store, &snapshot);
    }

    reach_wallpaper_service_port wallpaper = {};
    if (result == REACH_OK) {
        result = reach_windows_create_wallpaper_service(&wallpaper);
    }
    if (result == REACH_OK && wallpaper.ops.set_monitor_wallpaper != nullptr) {
        (void)wallpaper.ops.set_monitor_wallpaper(wallpaper.service, zero_based_index, full_path);
    }

    if (wallpaper.ops.destroy != nullptr) {
        wallpaper.ops.destroy(wallpaper.service);
    }
    if (store.ops.destroy != nullptr) {
        store.ops.destroy(store.store);
    }

    if (result == REACH_OK) {
        reachctl_notify_wallpaper_changed();
    }

    return result;
}

static reach_result reachctl_clear_wallpaper(void)
{
    reach_config_store_port store = {};
    reach_result result = reachctl_open_config_store(&store);

    reach_config_snapshot snapshot = {};
    if (result == REACH_OK) {
        result = store.ops.load(store.store, &snapshot);
    }
    if (result == REACH_OK) {
        snapshot.wallpaper_path[0] = 0;
        result = store.ops.save(store.store, &snapshot);
    }

    reach_wallpaper_service_port wallpaper = {};
    if (result == REACH_OK) {
        result = reach_windows_create_wallpaper_service(&wallpaper);
    }
    if (result == REACH_OK) {
        result = wallpaper.ops.clear_wallpaper(wallpaper.service);
    }

    if (wallpaper.ops.destroy != nullptr) {
        wallpaper.ops.destroy(wallpaper.service);
    }
    if (store.ops.destroy != nullptr) {
        store.ops.destroy(store.store);
    }

    if (result == REACH_OK) {
        reachctl_notify_wallpaper_changed();
    }

    return result;
}

static reach_result reachctl_clear_monitor_wallpaper(size_t monitor_index)
{
    if (monitor_index == 0 || monitor_index > REACH_MAX_WALLPAPER_MONITORS) {
        return REACH_INVALID_ARGUMENT;
    }

    size_t zero_based_index = monitor_index - 1;

    reach_config_store_port store = {};
    reach_result result = reachctl_open_config_store(&store);

    reach_config_snapshot snapshot = {};
    if (result == REACH_OK) {
        result = store.ops.load(store.store, &snapshot);
    }
    if (result == REACH_OK) {
        snapshot.monitor_wallpaper_paths[zero_based_index][0] = 0;
        result = store.ops.save(store.store, &snapshot);
    }

    if (store.ops.destroy != nullptr) {
        store.ops.destroy(store.store);
    }

    if (result == REACH_OK) {
        reachctl_notify_wallpaper_changed();
    }

    return result;
}

static int reachctl_pin_path_command(const wchar_t *path)
{
    reach_config_store_port store = {};
    int ok = reachctl_open_config_store(&store) == REACH_OK &&
        reach_pin_config_pin_path(&store, reinterpret_cast<const uint16_t *>(path)) == REACH_OK;

    if (store.ops.destroy != nullptr) {
        store.ops.destroy(store.store);
    }
    if (ok) {
        reachctl_notify_config_changed();
    }

    reachctl_print(ok ? L"Pinned to Reach dock." : L"Pin to Reach dock failed.");
    return ok ? 0 : 1;
}

static int reachctl_set_pin_appid_command(const wchar_t *path, const wchar_t *appid)
{
    reach_config_store_port store = {};
    int ok = reachctl_open_config_store(&store) == REACH_OK &&
        reach_pin_config_set_app_user_model_id(
            &store,
            reinterpret_cast<const uint16_t *>(path),
            reinterpret_cast<const uint16_t *>(appid)) == REACH_OK;

    if (store.ops.destroy != nullptr) {
        store.ops.destroy(store.store);
    }
    if (ok) {
        reachctl_notify_config_changed();
    }

    reachctl_print(ok ? L"Reach pin AppUserModelID set (diagnostic override)." : L"Reach pin AppUserModelID set failed.");
    return ok ? 0 : 1;
}

static int reachctl_unpin_path_command(const wchar_t *path)
{
    reach_config_store_port store = {};
    int ok = reachctl_open_config_store(&store) == REACH_OK &&
        reach_pin_config_unpin_path(&store, reinterpret_cast<const uint16_t *>(path)) == REACH_OK;

    if (store.ops.destroy != nullptr) {
        store.ops.destroy(store.store);
    }
    if (ok) {
        reachctl_notify_config_changed();
    }

    reachctl_print(ok ? L"Unpinned from Reach dock." : L"Unpin from Reach dock failed.");
    return ok ? 0 : 1;
}

static int reachctl_unpin_id_command(uint32_t id)
{
    reach_config_store_port store = {};
    int ok = reachctl_open_config_store(&store) == REACH_OK &&
        reach_pin_config_unpin_id(&store, id) == REACH_OK;

    if (store.ops.destroy != nullptr) {
        store.ops.destroy(store.store);
    }
    if (ok) {
        reachctl_notify_config_changed();
    }

    reachctl_print(ok ? L"Unpinned from Reach dock." : L"Unpin from Reach dock failed.");
    return ok ? 0 : 1;
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
            int ok = reachctl_install_reach_shell_and_switch(reach_exe) == REACH_OK;
            reachctl_print(ok
                ? L"Reach installed and started for current user."
                : L"Reach install/start failed.");
            return ok ? 0 : 1;
        }

        if (lstrcmpiW(argv[index], L"--reset") == 0) {
            int ok = reachctl_reset_to_windows_shell() == REACH_OK;
            reachctl_print(ok
                ? L"Reach reset complete. Windows shell restored. If the desktop/taskbar does not appear, sign out and sign back in."
                : L"Reach reset attempted, but one or more steps failed.");
            return ok ? 0 : 1;
        }

        if (lstrcmpiW(argv[index], L"--print-shell-status") == 0) {
            reach_shell_registration_status status = {};
            if (reach_windows_shell_query_current_user(reach_exe, &status) != REACH_OK) {
                reachctl_print(L"Reach shell status query failed.");
                return 1;
            }

            wchar_t line[640] = {};
            swprintf_s(
                line,
                L"CurrentShell=%ls StartupAttemptCount=%u",
                reinterpret_cast<const wchar_t *>(status.current_shell),
                status.startup_attempt_count);
            reachctl_print(line);
            return 0;
        }

        if (lstrcmpiW(argv[index], L"--list-monitors") == 0) {
            int ok = reachctl_list_monitors() == REACH_OK;
            if (!ok) {
                reachctl_print(L"Monitor query failed.");
            }
            return ok ? 0 : 1;
        }

        if (lstrcmpiW(argv[index], L"--pin-path") == 0) {
            if (index + 1 >= argc) {
                reachctl_print(L"--pin-path requires a path.");
                return 2;
            }
            return reachctl_pin_path_command(argv[index + 1]);
        }

        if (lstrcmpiW(argv[index], L"--set-pin-appid") == 0) {
            if (index + 2 >= argc) {
                reachctl_print(L"--set-pin-appid requires a pinned path and AppUserModelID.");
                return 2;
            }
            return reachctl_set_pin_appid_command(argv[index + 1], argv[index + 2]);
        }

        if (lstrcmpiW(argv[index], L"--unpin-path") == 0) {
            if (index + 1 >= argc) {
                reachctl_print(L"--unpin-path requires a path.");
                return 2;
            }
            return reachctl_unpin_path_command(argv[index + 1]);
        }

        if (lstrcmpiW(argv[index], L"--unpin-id") == 0) {
            if (index + 1 >= argc) {
                reachctl_print(L"--unpin-id requires an id.");
                return 2;
            }
            uint32_t id = (uint32_t)wcstoul(argv[index + 1], nullptr, 10);
            return reachctl_unpin_id_command(id);
        }

        if (lstrcmpiW(argv[index], L"--install-context-menu") == 0) {
            int ok = reachctl_install_context_menu() == REACH_OK;
            reachctl_print(ok ? L"Reach Explorer context menu installed." : L"Reach Explorer context menu install failed.");
            return ok ? 0 : 1;
        }

        if (lstrcmpiW(argv[index], L"--remove-context-menu") == 0) {
            int ok = reachctl_remove_context_menu() == REACH_OK;
            reachctl_print(ok ? L"Reach Explorer context menu removed." : L"Reach Explorer context menu removal failed.");
            return ok ? 0 : 1;
        }

        if (lstrcmpiW(argv[index], L"--set-wallpaper") == 0) {
            if (index + 1 >= argc) {
                reachctl_print(L"--set-wallpaper requires a path.");
                return 2;
            }
            int ok = reachctl_set_wallpaper(reinterpret_cast<const uint16_t *>(argv[index + 1])) == REACH_OK;
            reachctl_print(ok ? L"Reach wallpaper set." : L"Reach wallpaper set failed.");
            return ok ? 0 : 1;
        }

        if (lstrcmpiW(argv[index], L"--set-wallpaper-monitor") == 0) {
            if (index + 2 >= argc) {
                reachctl_print(L"--set-wallpaper-monitor requires a 1-based monitor index and path.");
                return 2;
            }
            size_t monitor_index = (size_t)wcstoul(argv[index + 1], nullptr, 10);
            int ok = reachctl_set_monitor_wallpaper(monitor_index, reinterpret_cast<const uint16_t *>(argv[index + 2])) == REACH_OK;
            reachctl_print(ok ? L"Reach monitor wallpaper set." : L"Reach monitor wallpaper set failed.");
            return ok ? 0 : 1;
        }

        if (lstrcmpiW(argv[index], L"--clear-wallpaper") == 0) {
            int ok = reachctl_clear_wallpaper() == REACH_OK;
            reachctl_print(ok ? L"Reach wallpaper cleared." : L"Reach wallpaper clear failed.");
            return ok ? 0 : 1;
        }

        if (lstrcmpiW(argv[index], L"--clear-wallpaper-monitor") == 0) {
            if (index + 1 >= argc) {
                reachctl_print(L"--clear-wallpaper-monitor requires a 1-based monitor index.");
                return 2;
            }
            size_t monitor_index = (size_t)wcstoul(argv[index + 1], nullptr, 10);
            int ok = reachctl_clear_monitor_wallpaper(monitor_index) == REACH_OK;
            reachctl_print(ok ? L"Reach monitor wallpaper cleared." : L"Reach monitor wallpaper clear failed.");
            return ok ? 0 : 1;
        }
    }

    reachctl_print(
        L"Usage: reachctl.exe "
        L"--install | "
        L"--reset | "
        L"--print-shell-status | "
        L"--list-monitors | "
        L"--pin-path <path> | "
        L"--set-pin-appid <path> <appid> (diagnostic override) | "
        L"--unpin-path <path> | "
        L"--unpin-id <id> | "
        L"--install-context-menu | "
        L"--remove-context-menu | "
        L"--set-wallpaper <path> | "
        L"--set-wallpaper-monitor <index> <path> | "
        L"--clear-wallpaper | "
        L"--clear-wallpaper-monitor <index>");
    return 2;
}
