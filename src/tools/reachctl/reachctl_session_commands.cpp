#include "reachctl_session_commands.h"

#include "reachctl_common.h"
#include "reachctl_context_menu.h"
#include "reachctl_elevation_helper.h"

#include "reach/platform/shell_registration.h"

#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <cwchar>

struct reachctl_monitor_list_state
{
    size_t index;
};

static reach_result reachctl_first_start_repair_marker_path(wchar_t *path, DWORD path_count)
{
    if (path == nullptr || path_count == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    path[0] = 0;
    PWSTR local_app_data = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr,
                                      &local_app_data);
    if (FAILED(hr) || local_app_data == nullptr)
    {
        return REACH_ERROR;
    }

    wcscpy_s(path, path_count, local_app_data);
    CoTaskMemFree(local_app_data);

    if (!PathAppendW(path, L"Reach"))
    {
        return REACH_ERROR;
    }

    (void)CreateDirectoryW(path, nullptr);

    if (!PathAppendW(path, L"first-start-repair.pending"))
    {
        return REACH_ERROR;
    }

    return REACH_OK;
}

static reach_result reachctl_mark_first_start_repair_pending(void)
{
    wchar_t marker_path[MAX_PATH] = {};
    if (reachctl_first_start_repair_marker_path(marker_path, MAX_PATH) != REACH_OK)
    {
        return REACH_ERROR;
    }

    HANDLE file = CreateFileW(marker_path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return REACH_ERROR;
    }

    CloseHandle(file);
    return REACH_OK;
}

static int32_t reachctl_consume_first_start_repair_pending(void)
{
    wchar_t marker_path[MAX_PATH] = {};
    if (reachctl_first_start_repair_marker_path(marker_path, MAX_PATH) != REACH_OK)
    {
        return 0;
    }

    DWORD attributes = GetFileAttributesW(marker_path);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        return 0;
    }

    (void)DeleteFileW(marker_path);
    return 1;
}

reach_result reachctl_start_process(const wchar_t *path, const wchar_t *arguments,
                                    const wchar_t *working_directory)
{
    if (path == nullptr || path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    wchar_t command_line[1024] = {};
    if (arguments != nullptr && arguments[0] != 0)
    {
        swprintf_s(command_line, L"\"%ls\" %ls", path, arguments);
    }
    else
    {
        swprintf_s(command_line, L"\"%ls\"", path);
    }

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);

    PROCESS_INFORMATION process = {};
    BOOL ok = CreateProcessW(nullptr, command_line, nullptr, nullptr, FALSE, 0, nullptr,
                             working_directory, &startup, &process);

    if (!ok)
    {
        return REACH_ERROR;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return REACH_OK;
}

static reach_result reachctl_terminate_processes_by_name(const wchar_t *process_name)
{
    if (process_name == nullptr || process_name[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return REACH_ERROR;
    }

    DWORD current_process_id = GetCurrentProcessId();
    reach_result result = REACH_OK;

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (entry.th32ProcessID == current_process_id)
            {
                continue;
            }

            if (lstrcmpiW(entry.szExeFile, process_name) != 0)
            {
                continue;
            }

            HANDLE process =
                OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, entry.th32ProcessID);

            if (process == nullptr)
            {
                result = REACH_ERROR;
                continue;
            }

            if (!TerminateProcess(process, 0))
            {
                result = REACH_ERROR;
            }
            else
            {
                (void)WaitForSingleObject(process, 3000);
            }

            CloseHandle(process);
        } while (Process32NextW(snapshot, &entry));
    }
    else
    {
        result = REACH_ERROR;
    }

    CloseHandle(snapshot);
    return result;
}

static reach_result reachctl_watchdog_exe(uint16_t *path, DWORD path_count)
{
    if (path == nullptr || path_count == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD length = GetModuleFileNameW(nullptr, reinterpret_cast<wchar_t *>(path), path_count);
    if (length == 0 || length >= path_count)
    {
        return REACH_ERROR;
    }

    wchar_t *path_w = reinterpret_cast<wchar_t *>(path);
    if (!PathRemoveFileSpecW(path_w))
    {
        return REACH_ERROR;
    }

    if (!PathAppendW(path_w, L"reach-watchdog.exe"))
    {
        return REACH_ERROR;
    }

    return REACH_OK;
}

static reach_result reachctl_register_watchdog_task(void)
{
    uint16_t watchdog_path_u16[260] = {};
    reach_result path_result = reachctl_watchdog_exe(watchdog_path_u16, 260);
    if (path_result != REACH_OK)
    {
        return path_result;
    }

    const wchar_t *watchdog_path = reinterpret_cast<const wchar_t *>(watchdog_path_u16);

    wchar_t arguments[1024] = {};
    swprintf_s(arguments,
               L"/Create /F /SC ONLOGON /TN \"ReachWatchdog\" /TR \"\\\"%ls\\\"\" /RL LIMITED",
               watchdog_path);

    return reachctl_run_process_wait(L"C:\\Windows\\System32\\schtasks.exe", arguments, nullptr);
}

static reach_result reachctl_unregister_watchdog_task(void)
{
    return reachctl_run_process_wait(L"C:\\Windows\\System32\\schtasks.exe",
                                     L"/Delete /F /TN \"ReachWatchdog\"", nullptr);
}

static int32_t reachctl_watchdog_task_is_registered(void)
{
    return reachctl_run_process_wait(L"C:\\Windows\\System32\\schtasks.exe",
                                     L"/Query /TN \"ReachWatchdog\"", nullptr) == REACH_OK;
}

static BOOL CALLBACK reachctl_reset_monitor_work_area_proc(HMONITOR monitor, HDC dc, LPRECT rect,
                                                           LPARAM param)
{
    (void)dc;
    (void)rect;

    reach_result *result = reinterpret_cast<reach_result *>(param);
    if (result == nullptr)
    {
        return FALSE;
    }

    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info))
    {
        *result = REACH_ERROR;
        return TRUE;
    }

    RECT work_area = info.rcMonitor;
    if (!SystemParametersInfoW(SPI_SETWORKAREA, 0, &work_area, SPIF_SENDCHANGE))
    {
        *result = REACH_ERROR;
    }

    return TRUE;
}

static reach_result reachctl_reset_monitor_work_areas(void)
{
    reach_result result = REACH_OK;
    if (!EnumDisplayMonitors(nullptr, nullptr, reachctl_reset_monitor_work_area_proc,
                             reinterpret_cast<LPARAM>(&result)))
    {
        return REACH_ERROR;
    }

    return result;
}

static BOOL CALLBACK reachctl_repair_maximized_window_proc(HWND hwnd, LPARAM param)
{
    (void)param;

    if (hwnd == nullptr || !IsWindow(hwnd) || !IsWindowVisible(hwnd))
    {
        return TRUE;
    }

    wchar_t class_name[64] = {};
    GetClassNameW(hwnd, class_name, 64);
    if (lstrcmpiW(class_name, L"Shell_TrayWnd") == 0 ||
        lstrcmpiW(class_name, L"Shell_SecondaryTrayWnd") == 0 ||
        lstrcmpiW(class_name, L"Progman") == 0 || lstrcmpiW(class_name, L"WorkerW") == 0)
    {
        return TRUE;
    }

    if (!IsZoomed(hwnd))
    {
        return TRUE;
    }

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (monitor == nullptr)
    {
        return TRUE;
    }

    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info))
    {
        return TRUE;
    }

    int width = info.rcMonitor.right - info.rcMonitor.left;
    int height = info.rcMonitor.bottom - info.rcMonitor.top;

    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    if (GetWindowPlacement(hwnd, &placement))
    {
        placement.rcNormalPosition = info.rcMonitor;
        (void)SetWindowPlacement(hwnd, &placement);
    }

    (void)SetWindowPos(hwnd, nullptr, info.rcMonitor.left, info.rcMonitor.top, width, height,
                       SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    return TRUE;
}

static reach_result reachctl_repair_maximized_windows(void)
{
    return EnumWindows(reachctl_repair_maximized_window_proc, 0) ? REACH_OK : REACH_ERROR;
}

static reach_result reachctl_start_explorer_shell(void)
{
    wchar_t windows_dir[MAX_PATH] = {};
    UINT windows_length = GetWindowsDirectoryW(windows_dir, MAX_PATH);
    if (windows_length == 0 || windows_length >= MAX_PATH)
    {
        return REACH_ERROR;
    }

    wchar_t explorer_path[MAX_PATH] = {};
    wcscpy_s(explorer_path, windows_dir);
    if (!PathAppendW(explorer_path, L"explorer.exe"))
    {
        return REACH_ERROR;
    }

    return reachctl_start_process(explorer_path, nullptr, windows_dir);
}

static reach_result reachctl_start_userinit(void)
{
    wchar_t system_dir[MAX_PATH] = {};
    UINT system_length = GetSystemDirectoryW(system_dir, MAX_PATH);
    if (system_length == 0 || system_length >= MAX_PATH)
    {
        return REACH_ERROR;
    }

    wchar_t userinit_path[MAX_PATH] = {};
    wcscpy_s(userinit_path, system_dir);
    if (!PathAppendW(userinit_path, L"userinit.exe"))
    {
        return REACH_ERROR;
    }

    return reachctl_start_process(userinit_path, nullptr, system_dir);
}

static reach_result reachctl_run_watchdog_task(void)
{
    HINSTANCE result = ShellExecuteW(nullptr, L"open", L"C:\\Windows\\System32\\schtasks.exe",
                                     L"/Run /TN \"ReachWatchdog\"", nullptr, SW_HIDE);

    return reinterpret_cast<intptr_t>(result) > 32 ? REACH_OK : REACH_ERROR;
}

reach_result reachctl_start_reach_session(const uint16_t *reach_exe)
{
    (void)reach_exe;

    reach_result helper_result = reachctl_start_elevation_helper();
    if (helper_result != REACH_OK)
    {
        return helper_result;
    }

    reach_result kill_reach_result = reachctl_terminate_processes_by_name(L"reach.exe");

    reach_result kill_explorer_result = reachctl_terminate_processes_by_name(L"explorer.exe");

    reach_result work_area_result = REACH_OK;
    reach_result repair_result = REACH_OK;
    if (reachctl_consume_first_start_repair_pending())
    {
        work_area_result = reachctl_reset_monitor_work_areas();
        repair_result = reachctl_repair_maximized_windows();
    }

    reach_result start_result = reachctl_run_watchdog_task();

    if (kill_reach_result != REACH_OK || kill_explorer_result != REACH_OK ||
        work_area_result != REACH_OK || repair_result != REACH_OK || start_result != REACH_OK)
    {
        return REACH_ERROR;
    }

    return REACH_OK;
}

static BOOL CALLBACK reachctl_print_monitor_proc(HMONITOR monitor, HDC dc, LPRECT rect,
                                                 LPARAM param)
{
    (void)monitor;
    (void)dc;

    reachctl_monitor_list_state *state = reinterpret_cast<reachctl_monitor_list_state *>(param);
    if (state == nullptr || rect == nullptr)
    {
        return TRUE;
    }

    wchar_t line[256] = {};
    swprintf_s(line, L"%u: x=%ld y=%ld width=%ld height=%ld", (unsigned)(state->index + 1),
               rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top);

    reachctl_print(line);
    state->index += 1;
    return TRUE;
}

reach_result reachctl_list_monitors(void)
{
    reachctl_monitor_list_state state = {};
    BOOL ok = EnumDisplayMonitors(nullptr, nullptr, reachctl_print_monitor_proc,
                                  reinterpret_cast<LPARAM>(&state));
    return ok && state.index > 0 ? REACH_OK : REACH_ERROR;
}

int32_t reachctl_is_process_elevated(void)
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        return 0;
    }

    TOKEN_ELEVATION elevation = {};
    DWORD size = sizeof(elevation);
    BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);

    CloseHandle(token);
    return ok && elevation.TokenIsElevated;
}

int32_t reachctl_is_reach_installed(void)
{
    uint16_t reach_exe[260] = {};
    if (reachctl_target_exe(reach_exe, 260) != REACH_OK)
    {
        return 0;
    }

    const wchar_t *reach_exe_w = reinterpret_cast<const wchar_t *>(reach_exe);
    DWORD attributes = GetFileAttributesW(reach_exe_w);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        return 0;
    }

    uint16_t watchdog_exe[260] = {};
    if (reachctl_watchdog_exe(watchdog_exe, 260) != REACH_OK)
    {
        return 0;
    }

    const wchar_t *watchdog_exe_w = reinterpret_cast<const wchar_t *>(watchdog_exe);
    attributes = GetFileAttributesW(watchdog_exe_w);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        return 0;
    }

    reach_shell_registration_status shell_status = {};
    if (reach_windows_shell_query_current_user(reach_exe, &shell_status) != REACH_OK ||
        !shell_status.reach_is_shell)
    {
        return 0;
    }

    return reachctl_watchdog_task_is_registered();
}

reach_result reachctl_reset_to_windows_shell(void)
{
    reach_result restore_result = reach_windows_shell_restore_current_user();

    (void)reachctl_unregister_watchdog_task();
    (void)reachctl_unregister_elevation_helper();
    (void)reachctl_remove_context_menus();

    (void)reachctl_terminate_processes_by_name(L"reach.exe");
    (void)reachctl_terminate_processes_by_name(L"reach-watchdog.exe");
    (void)reachctl_terminate_processes_by_name(L"reach_elevation_helper.exe");
    (void)reachctl_terminate_processes_by_name(L"explorer.exe");

    reach_result start_result = reachctl_start_userinit();
    if (start_result != REACH_OK)
    {
        start_result = reachctl_start_explorer_shell();
    }

    if (restore_result != REACH_OK || start_result != REACH_OK)
    {
        return REACH_ERROR;
    }

    return REACH_OK;
}

reach_result reachctl_install_reach_shell_and_watchdog(const uint16_t *reach_exe)
{
    if (reach_exe == nullptr || reach_exe[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (!reachctl_is_process_elevated())
    {
        return REACH_ERROR;
    }

    reach_result task_result = reachctl_register_watchdog_task();
    if (task_result != REACH_OK)
    {
        return task_result;
    }

    uint16_t reachctl_exe[260] = {};
    reach_result current_result = reachctl_current_exe(reachctl_exe, 260);
    if (current_result != REACH_OK)
    {
        return current_result;
    }

    reach_result context_result = reachctl_install_context_menus(reachctl_exe, reach_exe);

    if (context_result != REACH_OK)
    {
        return context_result;
    }

    reach_result shell_result = reach_windows_shell_install_current_user(reach_exe);
    if (shell_result != REACH_OK)
    {
        return shell_result;
    }

    return reachctl_mark_first_start_repair_pending();
}
