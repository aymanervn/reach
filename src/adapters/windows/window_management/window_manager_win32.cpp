#include "../windows_adapters_internal.h"

#include "reach_service_client_win32.h"
#include "reach_service_shared_state_win32.h"
#include "reach_service_task_win32.h"

#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlwapi.h>

#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

struct reach_window_manager
{
    std::vector<reach_service_window_snapshot> helper_windows;
    int32_t dirty;
    CRITICAL_SECTION lock;
    int32_t lock_initialized;
    int32_t game_mode_active;
    LONG helper_prompt_active;
    LONG helper_start_active;
    LONG helper_retry_suppressed_until;
    std::thread helper_start_thread;
    std::mutex helper_start_mutex;
    std::condition_variable helper_start_cv;
    int32_t helper_start_stop;
    int32_t helper_start_requested;
};

static const DWORD REACH_HELPER_RESTART_FAILED_COOLDOWN_MS = 5000;
static void reach_window_manager_icon_ref_for_helper(const reach_service_window_snapshot &helper,
                                                     uint16_t *out_icon_ref,
                                                     size_t out_icon_ref_count)
{
    if (out_icon_ref == nullptr || out_icon_ref_count == 0)
    {
        return;
    }

    out_icon_ref[0] = 0;
    if (helper.process_path[0] == 0)
    {
        return;
    }

    (void)reach_copy_utf16(out_icon_ref, out_icon_ref_count,
                           reinterpret_cast<const uint16_t *>(helper.process_path));
}

static const reach_service_window_snapshot *
reach_window_manager_find_helper_window(const reach_window_manager *manager, uintptr_t window_id)
{
    if (manager == nullptr || window_id == 0)
    {
        return nullptr;
    }

    for (const reach_service_window_snapshot &snapshot : manager->helper_windows)
    {
        if (snapshot.window == static_cast<uint64_t>(window_id))
        {
            return &snapshot;
        }
    }
    return nullptr;
}

static void reach_window_manager_lock(const reach_window_manager *manager)
{
    if (manager != nullptr && manager->lock_initialized)
    {
        EnterCriticalSection(&const_cast<reach_window_manager *>(manager)->lock);
    }
}

static void reach_window_manager_unlock(const reach_window_manager *manager)
{
    if (manager != nullptr && manager->lock_initialized)
    {
        LeaveCriticalSection(&const_cast<reach_window_manager *>(manager)->lock);
    }
}

static void reach_window_manager_fill_request_identity(const reach_window_manager *manager,
                                                       uintptr_t window_id,
                                                       reach_service_request *request)
{
    if (request == nullptr)
    {
        return;
    }

    request->window = static_cast<uint64_t>(window_id);
    reach_window_manager_lock(manager);
    const reach_service_window_snapshot *snapshot =
        reach_window_manager_find_helper_window(manager, window_id);
    if (snapshot != nullptr)
    {
        request->process_id = snapshot->process_id;
        request->thread_id = snapshot->thread_id;
        lstrcpynW(request->class_name, snapshot->class_name, 128);
    }
    reach_window_manager_unlock(manager);
}

static reach_result reach_window_manager_send_helper(reach_window_manager *manager,
                                                     reach_service_command command,
                                                     uintptr_t window_id, reach_split_mode mode)
{
    if (!reach_service_shared_reader_connected())
    {
        return REACH_ERROR;
    }

    reach_service_request request = {};
    request.version = reach_service_protocol_version();
    request.command = command;
    request.split_mode = static_cast<int32_t>(mode);
    reach_window_manager_fill_request_identity(manager, window_id, &request);

    return reach_service_send_request(&request, nullptr);
}

static int32_t
reach_window_manager_privileged_control_available(const reach_window_manager *manager)
{
    if (manager != nullptr && reach_service_shared_reader_connected())
    {
        InterlockedExchange(
            &const_cast<reach_window_manager *>(manager)->helper_retry_suppressed_until, 0);
        return 1;
    }
    return reach_service_shared_reader_connected();
}

static int32_t reach_window_manager_retry_suppressed(const reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return 0;
    }

    LONG suppressed_until = InterlockedCompareExchange(
        &const_cast<reach_window_manager *>(manager)->helper_retry_suppressed_until, 0, 0);
    if (suppressed_until == 0)
    {
        return 0;
    }
    DWORD now = GetTickCount();
    return static_cast<LONG>(now - static_cast<DWORD>(suppressed_until)) < 0;
}

static void reach_window_manager_suppress_helper_retry(reach_window_manager *manager,
                                                       DWORD cooldown_ms)
{
    if (manager != nullptr)
    {
        InterlockedExchange(&manager->helper_retry_suppressed_until,
                            static_cast<LONG>(GetTickCount() + cooldown_ms));
    }
}

static reach_result reach_window_manager_sibling_path(const wchar_t *filename, wchar_t *path,
                                                      size_t path_count)
{
    if (filename == nullptr || filename[0] == 0 || path == nullptr || path_count == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD length = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(path_count));
    if (length == 0 || length >= path_count)
    {
        return REACH_ERROR;
    }
    if (!PathRemoveFileSpecW(path) || !PathAppendW(path, filename))
    {
        return REACH_ERROR;
    }
    return REACH_OK;
}

static reach_result reach_window_manager_repair_privileged_control(void)
{
    wchar_t reachctl_path[MAX_PATH] = {};
    if (reach_window_manager_sibling_path(L"reachctl.exe", reachctl_path, MAX_PATH) != REACH_OK)
    {
        return REACH_ERROR;
    }

    wchar_t user_id[192] = {};
    if (reach_service_current_user_id(user_id, 192) != REACH_OK)
    {
        return REACH_ERROR;
    }
    wchar_t arguments[256] = {};
    swprintf_s(arguments, L"--install-service --user-id %ls", user_id);

    SHELLEXECUTEINFOW execute = {};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_NOCLOSEPROCESS;
    execute.lpVerb = L"runas";
    execute.lpFile = reachctl_path;
    execute.lpParameters = arguments;
    execute.nShow = SW_HIDE;
    if (!ShellExecuteExW(&execute) || execute.hProcess == nullptr)
    {
        return REACH_ERROR;
    }

    DWORD wait = WaitForSingleObject(execute.hProcess, INFINITE);
    DWORD exit_code = 1;
    if (wait == WAIT_OBJECT_0)
    {
        (void)GetExitCodeProcess(execute.hProcess, &exit_code);
    }
    CloseHandle(execute.hProcess);
    return wait == WAIT_OBJECT_0 && exit_code == 0 ? REACH_OK : REACH_ERROR;
}

struct reach_window_manager_prompt_owner_state
{
    DWORD process_id;
    HWND window;
};

static BOOL CALLBACK reach_window_manager_find_prompt_owner(HWND window, LPARAM parameter)
{
    reach_window_manager_prompt_owner_state *state =
        reinterpret_cast<reach_window_manager_prompt_owner_state *>(parameter);
    if (state == nullptr || window == nullptr || !IsWindowVisible(window))
    {
        return TRUE;
    }

    DWORD process_id = 0;
    (void)GetWindowThreadProcessId(window, &process_id);
    if (process_id != state->process_id)
    {
        return TRUE;
    }

    wchar_t class_name[64] = {};
    GetClassNameW(window, class_name, 64);
    if (lstrcmpW(class_name, L"ReachPlatformWindow") == 0)
    {
        state->window = window;
        return FALSE;
    }
    return TRUE;
}

static HWND reach_window_manager_prompt_owner(void)
{
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        reach_window_manager_prompt_owner_state state = {};
        state.process_id = GetCurrentProcessId();
        (void)EnumWindows(reach_window_manager_find_prompt_owner, reinterpret_cast<LPARAM>(&state));
        if (state.window != nullptr)
        {
            return state.window;
        }
        Sleep(20);
    }
    return nullptr;
}

static void reach_window_manager_prepare_prompt_owner(HWND owner)
{
    if (owner == nullptr)
    {
        return;
    }

    HWND foreground = GetForegroundWindow();
    DWORD foreground_thread =
        foreground != nullptr ? GetWindowThreadProcessId(foreground, nullptr) : 0;
    DWORD current_thread = GetCurrentThreadId();
    int32_t attached = foreground_thread != 0 && foreground_thread != current_thread &&
                       AttachThreadInput(current_thread, foreground_thread, TRUE);

    (void)BringWindowToTop(owner);
    (void)SetForegroundWindow(owner);

    if (attached)
    {
        (void)AttachThreadInput(current_thread, foreground_thread, FALSE);
    }
}

static int32_t reach_window_manager_confirm_privileged_control_repair(reach_window_manager *manager)
{
    if (manager == nullptr || InterlockedCompareExchange(&manager->helper_prompt_active, 1, 0) != 0)
    {
        return 0;
    }

    HWND owner = reach_window_manager_prompt_owner();
    reach_window_manager_prepare_prompt_owner(owner);
    int response = MessageBoxW(
        owner,
        L"Reach's privileged helper task is missing or points to a different installation. "
        L"Repairing it requires administrator approval.",
        L"Reach",
        MB_ICONWARNING | MB_OKCANCEL | MB_DEFBUTTON1 | MB_SETFOREGROUND | MB_TOPMOST |
            MB_TASKMODAL);
    InterlockedExchange(&manager->helper_prompt_active, 0);
    return response == IDOK;
}

static reach_result reach_window_manager_start_privileged_control(reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (reach_window_manager_retry_suppressed(manager) ||
        InterlockedCompareExchange(&manager->helper_start_active, 1, 0) != 0)
    {
        return REACH_ERROR;
    }

    wchar_t helper_path[MAX_PATH] = {};
    reach_result result = REACH_ERROR;

    int32_t helper_path_ok =
        reach_window_manager_sibling_path(L"reachService.exe", helper_path, MAX_PATH) == REACH_OK;

    int32_t task_valid = helper_path_ok && reach_service_task_valid(helper_path);

    if (!task_valid)
    {
        if (helper_path_ok && reach_window_manager_confirm_privileged_control_repair(manager))
        {
            result = reach_window_manager_repair_privileged_control();

            task_valid = result == REACH_OK && reach_service_task_valid(helper_path);
        }
    }

    if (task_valid)
    {
        result = reach_service_task_run();
    }

    InterlockedExchange(&manager->helper_start_active, 0);

    if (result != REACH_OK)
    {
        reach_window_manager_suppress_helper_retry(manager,
                                                   REACH_HELPER_RESTART_FAILED_COOLDOWN_MS);
    }

    return result;
}

static void reach_window_manager_request_privileged_control_start(reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(manager->helper_start_mutex);
        if (manager->helper_start_stop)
        {
            return;
        }
        manager->helper_start_requested = 1;
    }
    manager->helper_start_cv.notify_one();
}

static void reach_window_manager_privileged_control_thread(reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return;
    }

    for (;;)
    {
        {
            std::unique_lock<std::mutex> lock(manager->helper_start_mutex);
            manager->helper_start_cv.wait(
                lock, [manager]()
                { return manager->helper_start_stop || manager->helper_start_requested; });
            if (manager->helper_start_stop)
            {
                return;
            }
            manager->helper_start_requested = 0;
        }
        (void)reach_window_manager_start_privileged_control(manager);
    }
}

static void reach_window_manager_copy_shared_windows(reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return;
    }

    reach_service_window_snapshot windows[REACH_SERVICE_MAX_WINDOWS] = {};
    uint32_t window_count = 0;
    if (reach_service_shared_copy_windows(windows, REACH_SERVICE_MAX_WINDOWS, &window_count) !=
        REACH_OK)
    {
        window_count = 0;
    }

    reach_window_manager_lock(manager);
    manager->helper_windows.clear();
    for (uint32_t index = 0; index < window_count; ++index)
    {
        if (windows[index].include_in_switcher)
        {
            manager->helper_windows.push_back(windows[index]);
        }
    }
    manager->dirty = 1;
    reach_window_manager_unlock(manager);
}

static void reach_window_manager_copy_shared_game_mode(reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return;
    }

    int32_t active = 0;
    (void)reach_service_shared_copy_game_mode(&active);

    reach_window_manager_lock(manager);
    manager->game_mode_active = active ? 1 : 0;
    manager->dirty = 1;
    reach_window_manager_unlock(manager);
}

static void reach_window_manager_shared_callback(void *user,
                                                 reach_service_shared_reader_event event)
{
    reach_window_manager *manager = static_cast<reach_window_manager *>(user);
    if (manager == nullptr)
    {
        return;
    }

    if (event == REACH_SERVICE_SHARED_EVENT_CONNECTED)
    {
        reach_window_manager_copy_shared_windows(manager);
        reach_window_manager_copy_shared_game_mode(manager);
        return;
    }

    if (event == REACH_SERVICE_SHARED_EVENT_WINDOWS_CHANGED)
    {
        reach_window_manager_copy_shared_windows(manager);
        return;
    }

    if (event == REACH_SERVICE_SHARED_EVENT_GAME_MODE_CHANGED)
    {
        reach_window_manager_copy_shared_game_mode(manager);
        return;
    }

    if (event == REACH_SERVICE_SHARED_EVENT_DISCONNECTED)
    {
        reach_window_manager_lock(manager);
        manager->helper_windows.clear();
        manager->game_mode_active = 0;
        manager->dirty = 1;
        reach_window_manager_unlock(manager);
        reach_window_manager_request_privileged_control_start(manager);
    }
}

static reach_result reach_window_manager_start(reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    manager->dirty = 1;
    (void)reach_service_shared_reader_subscribe(reach_window_manager_shared_callback, manager);
    reach_window_manager_copy_shared_windows(manager);
    reach_window_manager_copy_shared_game_mode(manager);
    try
    {
        manager->helper_start_thread =
            std::thread(reach_window_manager_privileged_control_thread, manager);
        reach_window_manager_request_privileged_control_start(manager);
    }
    catch (...)
    {
        reach_window_manager_suppress_helper_retry(manager,
                                                   REACH_HELPER_RESTART_FAILED_COOLDOWN_MS);
    }
    return REACH_OK;
}

static reach_result reach_window_manager_stop(reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    {
        std::lock_guard<std::mutex> lock(manager->helper_start_mutex);
        manager->helper_start_stop = 1;
        manager->helper_start_requested = 0;
    }
    manager->helper_start_cv.notify_one();
    if (manager->helper_start_thread.joinable())
    {
        manager->helper_start_thread.join();
    }

    reach_window_manager_lock(manager);
    manager->helper_windows.clear();
    manager->game_mode_active = 0;
    manager->dirty = 0;
    reach_window_manager_unlock(manager);
    reach_service_shared_reader_unsubscribe(manager);
    return REACH_OK;
}

static reach_result reach_window_manager_refresh(reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_window_manager_lock(manager);
    manager->dirty = 0;
    reach_window_manager_unlock(manager);
    return REACH_OK;
}

static reach_result reach_window_manager_snap(reach_window_manager *manager, uintptr_t window_id,
                                              reach_split_mode mode)
{
    reach_result result =
        reach_window_manager_send_helper(manager, REACH_SERVICE_COMMAND_SNAP, window_id, mode);
    return result;
}

static int32_t reach_window_manager_window_on_primary_monitor(uint64_t window)
{
    HWND hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(window));
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (monitor == nullptr || !GetMonitorInfoW(monitor, &info))
    {
        return 0;
    }
    return (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
}

static int32_t
reach_window_manager_window_is_maximized_on_primary(const reach_window_manager *manager,
                                                    reach_window_id window_id)
{
    if (window_id == 0)
    {
        return 0;
    }

    reach_window_manager_lock(manager);
    int32_t maximized = 0;
    for (const reach_service_window_snapshot &snapshot : manager->helper_windows)
    {
        if (snapshot.window == static_cast<uint64_t>(window_id))
        {
            maximized = snapshot.maximized &&
                        reach_window_manager_window_on_primary_monitor(snapshot.window);
            break;
        }
    }
    reach_window_manager_unlock(manager);
    return maximized;
}

static int32_t
reach_window_manager_window_is_snapped_on_primary(const reach_window_manager *manager,
                                                  reach_window_id window_id)
{
    if (window_id == 0)
    {
        return 0;
    }

    reach_window_manager_lock(manager);
    uint64_t window = 0;
    for (const reach_service_window_snapshot &snapshot : manager->helper_windows)
    {
        if (snapshot.window == static_cast<uint64_t>(window_id))
        {
            if (!snapshot.maximized && !snapshot.iconic)
            {
                window = snapshot.window;
            }
            break;
        }
    }
    reach_window_manager_unlock(manager);
    if (window == 0)
    {
        return 0;
    }

    HWND hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(window));
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (monitor == nullptr || !GetMonitorInfoW(monitor, &info) ||
        (info.dwFlags & MONITORINFOF_PRIMARY) == 0)
    {
        return 0;
    }

    RECT frame = {};
    if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof(frame))) &&
        !GetWindowRect(hwnd, &frame))
    {
        return 0;
    }

    reach_rect_i32 work_area = {info.rcWork.left, info.rcWork.top, info.rcWork.right,
                                info.rcWork.bottom};
    reach_rect_i32 rect = {frame.left, frame.top, frame.right, frame.bottom};
    const int32_t tolerance = 8;

    return reach_layout_rect_matches_split(work_area, rect, REACH_SPLIT_LEFT, tolerance) ||
           reach_layout_rect_matches_split(work_area, rect, REACH_SPLIT_RIGHT, tolerance) ||
           reach_layout_rect_matches_split(work_area, rect, REACH_SPLIT_BOTTOM, tolerance);
}

static int32_t reach_window_manager_game_mode_active(const reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return 0;
    }

    reach_window_manager_lock(manager);
    int32_t active = manager->game_mode_active;
    reach_window_manager_unlock(manager);
    return active;
}

static int32_t reach_window_manager_needs_refresh(const reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return 0;
    }

    reach_window_manager_lock(manager);
    int32_t dirty = manager->dirty;
    reach_window_manager_unlock(manager);
    return dirty;
}

static size_t reach_window_manager_window_count(const reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return 0;
    }
    reach_window_manager_lock(manager);
    size_t count = manager->helper_windows.size();
    reach_window_manager_unlock(manager);
    return count;
}

static reach_result reach_window_manager_window_at(const reach_window_manager *manager,
                                                   size_t index, reach_window_snapshot *out_window)
{
    if (manager == nullptr || out_window == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_window_manager_lock(manager);
    if (index >= manager->helper_windows.size())
    {
        reach_window_manager_unlock(manager);
        return REACH_INVALID_ARGUMENT;
    }

    const reach_service_window_snapshot &helper = manager->helper_windows[index];
    reach_window_snapshot snapshot = {};
    snapshot.id = static_cast<uintptr_t>(helper.window);
    (void)reach_copy_utf16(snapshot.title, 260, reinterpret_cast<const uint16_t *>(helper.title));
    (void)reach_copy_utf16(snapshot.path, 260,
                           reinterpret_cast<const uint16_t *>(helper.process_path));
    reach_window_manager_icon_ref_for_helper(helper, snapshot.icon_ref, 260);
    snapshot.visible = helper.visible;
    snapshot.maximized = helper.maximized;
    snapshot.minimized = helper.iconic;
    (void)reach_copy_utf16(snapshot.app_user_model_id, 260,
                           reinterpret_cast<const uint16_t *>(helper.app_user_model_id));
    *out_window = snapshot;
    reach_window_manager_unlock(manager);
    return REACH_OK;
}

static reach_result reach_window_manager_pin_app_for_window(reach_window_manager *manager,
                                                            uintptr_t window_id,
                                                            const reach_window_snapshot *snapshot,
                                                            reach_pinned_app_model *out_app)
{
    (void)snapshot;
    if (manager == nullptr || window_id == 0 || out_app == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_window_manager_lock(manager);
    const reach_service_window_snapshot *helper =
        reach_window_manager_find_helper_window(manager, window_id);
    if (helper == nullptr || helper->process_path[0] == 0)
    {
        reach_window_manager_unlock(manager);
        return REACH_ERROR;
    }

    *out_app = {};
    (void)reach_copy_utf16(out_app->path, 260,
                           reinterpret_cast<const uint16_t *>(helper->process_path));
    (void)reach_copy_utf16(out_app->title, 128, reinterpret_cast<const uint16_t *>(helper->title));
    reach_window_manager_icon_ref_for_helper(*helper, out_app->icon_ref, 260);
    (void)reach_copy_utf16(out_app->app_user_model_id, 260,
                           reinterpret_cast<const uint16_t *>(helper->app_user_model_id));
    reach_window_manager_unlock(manager);
    return REACH_OK;
}

static reach_result reach_window_manager_activate(reach_window_manager *manager,
                                                  uintptr_t window_id)
{
    reach_result result = reach_window_manager_send_helper(manager, REACH_SERVICE_COMMAND_ACTIVATE,
                                                           window_id, REACH_SPLIT_LEFT);
    return result;
}

static reach_result reach_window_manager_minimize(reach_window_manager *manager,
                                                  uintptr_t window_id)
{
    reach_result result = reach_window_manager_send_helper(manager, REACH_SERVICE_COMMAND_MINIMIZE,
                                                           window_id, REACH_SPLIT_LEFT);
    return result;
}

static reach_result reach_window_manager_close(reach_window_manager *manager, uintptr_t window_id)
{
    reach_result result = reach_window_manager_send_helper(manager, REACH_SERVICE_COMMAND_CLOSE,
                                                           window_id, REACH_SPLIT_LEFT);
    return result;
}

static void reach_window_manager_destroy(reach_window_manager *manager)
{
    if (manager != nullptr)
    {
        (void)reach_window_manager_stop(manager);
        if (manager->lock_initialized)
        {
            DeleteCriticalSection(&manager->lock);
            manager->lock_initialized = 0;
        }
    }
    delete manager;
}

reach_result reach_windows_create_window_manager(reach_window_manager_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_window_manager *manager = new (std::nothrow) reach_window_manager();
    if (manager == nullptr)
    {
        return REACH_ERROR;
    }

    InitializeCriticalSection(&manager->lock);
    manager->lock_initialized = 1;
    manager->dirty = 1;
    out_port->manager = manager;
    out_port->ops.start = reach_window_manager_start;
    out_port->ops.stop = reach_window_manager_stop;
    out_port->ops.refresh = reach_window_manager_refresh;
    out_port->ops.snap = reach_window_manager_snap;
    out_port->ops.window_is_maximized_on_primary =
        reach_window_manager_window_is_maximized_on_primary;
    out_port->ops.window_is_snapped_on_primary = reach_window_manager_window_is_snapped_on_primary;
    out_port->ops.game_mode_active = reach_window_manager_game_mode_active;
    out_port->ops.needs_refresh = reach_window_manager_needs_refresh;
    out_port->ops.window_count = reach_window_manager_window_count;
    out_port->ops.window_at = reach_window_manager_window_at;
    out_port->ops.pin_app_for_window = reach_window_manager_pin_app_for_window;
    out_port->ops.privileged_control_available = reach_window_manager_privileged_control_available;
    out_port->ops.start_privileged_control = reach_window_manager_start_privileged_control;
    out_port->ops.activate = reach_window_manager_activate;
    out_port->ops.minimize = reach_window_manager_minimize;
    out_port->ops.close = reach_window_manager_close;
    out_port->ops.destroy = reach_window_manager_destroy;
    return REACH_OK;
}
