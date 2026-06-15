#include "../windows_adapters_internal.h"

#include "elevation_helper_client_win32.h"
#include "elevation_helper_shared_state_win32.h"

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>

#include <new>
#include <vector>

struct reach_window_manager
{
    std::vector<reach_elevation_helper_window_snapshot> helper_windows;
    int32_t dirty;
    CRITICAL_SECTION lock;
    int32_t lock_initialized;
    int32_t game_mode_active;
    LONG helper_prompt_active;
    LONG helper_start_active;
    DWORD helper_retry_suppressed_until;
};

static const DWORD REACH_HELPER_RESTART_DECLINED_COOLDOWN_MS = 10000;
static const DWORD REACH_HELPER_RESTART_FAILED_COOLDOWN_MS = 5000;
static const int REACH_SETTINGS_ICON_RESOURCE_ID = 102;

static void reach_window_manager_icon_ref_for_helper(
    const reach_elevation_helper_window_snapshot &helper, uint16_t *out_icon_ref,
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

    if (lstrcmpiW(helper.class_name, L"ReachSettingsWindow") == 0)
    {
        swprintf_s(reinterpret_cast<wchar_t *>(out_icon_ref), out_icon_ref_count, L"%ls,-%d",
                   helper.process_path, REACH_SETTINGS_ICON_RESOURCE_ID);
        return;
    }

    (void)reach_copy_utf16(out_icon_ref, out_icon_ref_count,
                           reinterpret_cast<const uint16_t *>(helper.process_path));
}

static const reach_elevation_helper_window_snapshot *
reach_window_manager_find_helper_window(const reach_window_manager *manager, uintptr_t window_id)
{
    if (manager == nullptr || window_id == 0)
    {
        return nullptr;
    }

    for (const reach_elevation_helper_window_snapshot &snapshot : manager->helper_windows)
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

static void reach_window_manager_fill_request_identity(
    const reach_window_manager *manager, uintptr_t window_id,
    reach_elevation_helper_request *request)
{
    if (request == nullptr)
    {
        return;
    }

    request->window = static_cast<uint64_t>(window_id);
    reach_window_manager_lock(manager);
    const reach_elevation_helper_window_snapshot *snapshot =
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
                                                     reach_elevation_helper_command command,
                                                     uintptr_t window_id,
                                                     reach_split_mode mode)
{
    if (!reach_elevation_helper_shared_reader_connected())
    {
        return REACH_ERROR;
    }

    reach_elevation_helper_request request = {};
    request.version = reach_elevation_helper_protocol_version();
    request.command = command;
    request.split_mode = static_cast<int32_t>(mode);
    reach_window_manager_fill_request_identity(manager, window_id, &request);

    return reach_elevation_helper_send_request(&request, nullptr);
}

static int32_t reach_window_manager_privileged_control_available(
    const reach_window_manager *manager)
{
    if (manager != nullptr && reach_elevation_helper_shared_reader_connected())
    {
        const_cast<reach_window_manager *>(manager)->helper_retry_suppressed_until = 0;
        return 1;
    }
    return reach_elevation_helper_shared_reader_connected();
}

static int32_t reach_window_manager_retry_suppressed(const reach_window_manager *manager)
{
    if (manager == nullptr || manager->helper_retry_suppressed_until == 0)
    {
        return 0;
    }

    DWORD now = GetTickCount();
    return static_cast<LONG>(now - manager->helper_retry_suppressed_until) < 0;
}

static void reach_window_manager_suppress_helper_retry(reach_window_manager *manager,
                                                       DWORD cooldown_ms)
{
    if (manager != nullptr)
    {
        manager->helper_retry_suppressed_until = GetTickCount() + cooldown_ms;
    }
}

static int32_t reach_window_manager_confirm_privileged_control_restart(
    reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return 0;
    }

    if (reach_window_manager_privileged_control_available(manager))
    {
        return 1;
    }

    if (reach_window_manager_retry_suppressed(manager) ||
        InterlockedCompareExchange(&manager->helper_start_active, 0, 0) != 0)
    {
        return 0;
    }

    if (InterlockedCompareExchange(&manager->helper_prompt_active, 1, 0) != 0)
    {
        return 0;
    }

    int response = MessageBoxW(nullptr, L"Reach elevation helper is not running. Restart it?",
                               L"Reach",
                               MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON1 | MB_SETFOREGROUND |
                                   MB_TOPMOST | MB_TASKMODAL);
    InterlockedExchange(&manager->helper_prompt_active, 0);

    if (response != IDYES)
    {
        reach_window_manager_suppress_helper_retry(
            manager, REACH_HELPER_RESTART_DECLINED_COOLDOWN_MS);
        return 0;
    }

    return 1;
}

static reach_result reach_window_manager_helper_path(wchar_t *path, size_t path_count)
{
    if (path == nullptr || path_count == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD length = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(path_count));
    if (length == 0 || length >= path_count)
    {
        return REACH_ERROR;
    }
    if (!PathRemoveFileSpecW(path) || !PathAppendW(path, L"reach_elevation_helper.exe"))
    {
        return REACH_ERROR;
    }
    return REACH_OK;
}

static reach_result reach_window_manager_start_privileged_control(reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (reach_window_manager_privileged_control_available(manager))
    {
        return REACH_OK;
    }

    if (reach_window_manager_retry_suppressed(manager))
    {
        return REACH_ERROR;
    }

    if (InterlockedCompareExchange(&manager->helper_start_active, 1, 0) != 0)
    {
        return REACH_ERROR;
    }

    wchar_t helper_path[MAX_PATH] = {};
    if (reach_window_manager_helper_path(helper_path, MAX_PATH) != REACH_OK)
    {
        InterlockedExchange(&manager->helper_start_active, 0);
        reach_window_manager_suppress_helper_retry(manager,
                                                   REACH_HELPER_RESTART_FAILED_COOLDOWN_MS);
        return REACH_ERROR;
    }

    SHELLEXECUTEINFOW execute = {};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_NOCLOSEPROCESS;
    execute.lpVerb = L"runas";
    execute.lpFile = helper_path;
    execute.nShow = SW_HIDE;
    if (!ShellExecuteExW(&execute))
    {
        InterlockedExchange(&manager->helper_start_active, 0);
        reach_window_manager_suppress_helper_retry(manager,
                                                   REACH_HELPER_RESTART_FAILED_COOLDOWN_MS);
        return REACH_ERROR;
    }
    if (execute.hProcess != nullptr)
    {
        CloseHandle(execute.hProcess);
    }

    for (int attempt = 0; attempt < 40; ++attempt)
    {
        if (reach_elevation_helper_shared_reader_connected())
        {
            InterlockedExchange(&manager->helper_start_active, 0);
            return REACH_OK;
        }
        Sleep(50);
    }

    InterlockedExchange(&manager->helper_start_active, 0);
    reach_window_manager_suppress_helper_retry(manager, REACH_HELPER_RESTART_FAILED_COOLDOWN_MS);
    return REACH_ERROR;
}

static void reach_window_manager_copy_shared_windows(reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return;
    }

    reach_elevation_helper_window_snapshot windows[REACH_ELEVATION_HELPER_MAX_WINDOWS] = {};
    uint32_t window_count = 0;
    if (reach_elevation_helper_shared_copy_windows(windows, REACH_ELEVATION_HELPER_MAX_WINDOWS,
                                                   &window_count) != REACH_OK)
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
    (void)reach_elevation_helper_shared_copy_game_mode(&active);

    reach_window_manager_lock(manager);
    manager->game_mode_active = active ? 1 : 0;
    manager->dirty = 1;
    reach_window_manager_unlock(manager);
}

static void reach_window_manager_shared_callback(void *user,
                                                 reach_elevation_helper_shared_reader_event event)
{
    reach_window_manager *manager = static_cast<reach_window_manager *>(user);
    if (manager == nullptr)
    {
        return;
    }

    if (event == REACH_ELEVATION_HELPER_SHARED_EVENT_CONNECTED)
    {
        reach_window_manager_copy_shared_windows(manager);
        reach_window_manager_copy_shared_game_mode(manager);
        return;
    }

    if (event == REACH_ELEVATION_HELPER_SHARED_EVENT_WINDOWS_CHANGED)
    {
        reach_window_manager_copy_shared_windows(manager);
        return;
    }

    if (event == REACH_ELEVATION_HELPER_SHARED_EVENT_GAME_MODE_CHANGED)
    {
        reach_window_manager_copy_shared_game_mode(manager);
        return;
    }

    if (event == REACH_ELEVATION_HELPER_SHARED_EVENT_DISCONNECTED)
    {
        reach_window_manager_lock(manager);
        manager->helper_windows.clear();
        manager->game_mode_active = 0;
        manager->dirty = 1;
        reach_window_manager_unlock(manager);
    }
}

static reach_result reach_window_manager_start(reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    manager->dirty = 1;
    (void)reach_elevation_helper_shared_reader_subscribe(reach_window_manager_shared_callback,
                                                         manager);
    reach_window_manager_copy_shared_windows(manager);
    reach_window_manager_copy_shared_game_mode(manager);
    (void)reach_window_manager_start_privileged_control(manager);
    return REACH_OK;
}

static reach_result reach_window_manager_stop(reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_window_manager_lock(manager);
    manager->helper_windows.clear();
    manager->game_mode_active = 0;
    manager->dirty = 0;
    reach_window_manager_unlock(manager);
    reach_elevation_helper_shared_reader_unsubscribe(manager);
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
        reach_window_manager_send_helper(manager, REACH_ELEVATION_HELPER_COMMAND_SNAP, window_id,
                                         mode);
    return result;
}

static uintptr_t reach_window_manager_foreground(const reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return 0;
    }

    reach_window_manager_lock(manager);
    uintptr_t foreground = 0;
    for (const reach_elevation_helper_window_snapshot &snapshot : manager->helper_windows)
    {
        if (snapshot.focused)
        {
            foreground = static_cast<uintptr_t>(snapshot.window);
            break;
        }
    }
    reach_window_manager_unlock(manager);
    return foreground;
}

static int32_t reach_window_manager_foreground_is_maximized(const reach_window_manager *manager)
{
    reach_window_manager_lock(manager);
    int32_t maximized = 0;
    for (const reach_elevation_helper_window_snapshot &snapshot : manager->helper_windows)
    {
        if (snapshot.focused)
        {
            maximized = snapshot.maximized;
            break;
        }
    }
    reach_window_manager_unlock(manager);
    return maximized;
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

    const reach_elevation_helper_window_snapshot &helper = manager->helper_windows[index];
    reach_window_snapshot snapshot = {};
    snapshot.id = static_cast<uintptr_t>(helper.window);
    (void)reach_copy_utf16(snapshot.title, 260,
                           reinterpret_cast<const uint16_t *>(helper.title));
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
    const reach_elevation_helper_window_snapshot *helper =
        reach_window_manager_find_helper_window(manager, window_id);
    if (helper == nullptr || helper->process_path[0] == 0)
    {
        reach_window_manager_unlock(manager);
        return REACH_ERROR;
    }

    *out_app = {};
    (void)reach_copy_utf16(out_app->path, 260,
                           reinterpret_cast<const uint16_t *>(helper->process_path));
    (void)reach_copy_utf16(out_app->title, 128,
                           reinterpret_cast<const uint16_t *>(helper->title));
    reach_window_manager_icon_ref_for_helper(*helper, out_app->icon_ref, 260);
    (void)reach_copy_utf16(out_app->app_user_model_id, 260,
                           reinterpret_cast<const uint16_t *>(helper->app_user_model_id));
    reach_window_manager_unlock(manager);
    return REACH_OK;
}

static reach_result reach_window_manager_activate(reach_window_manager *manager,
                                                  uintptr_t window_id)
{
    reach_result result = reach_window_manager_send_helper(
        manager, REACH_ELEVATION_HELPER_COMMAND_ACTIVATE, window_id, REACH_SPLIT_LEFT);
    return result;
}

static reach_result reach_window_manager_minimize(reach_window_manager *manager,
                                                  uintptr_t window_id)
{
    reach_result result = reach_window_manager_send_helper(
        manager, REACH_ELEVATION_HELPER_COMMAND_MINIMIZE, window_id, REACH_SPLIT_LEFT);
    return result;
}

static reach_result reach_window_manager_close(reach_window_manager *manager, uintptr_t window_id)
{
    reach_result result = reach_window_manager_send_helper(
        manager, REACH_ELEVATION_HELPER_COMMAND_CLOSE, window_id, REACH_SPLIT_LEFT);
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
    out_port->ops.foreground = reach_window_manager_foreground;
    out_port->ops.foreground_is_maximized = reach_window_manager_foreground_is_maximized;
    out_port->ops.game_mode_active = reach_window_manager_game_mode_active;
    out_port->ops.needs_refresh = reach_window_manager_needs_refresh;
    out_port->ops.window_count = reach_window_manager_window_count;
    out_port->ops.window_at = reach_window_manager_window_at;
    out_port->ops.pin_app_for_window = reach_window_manager_pin_app_for_window;
    out_port->ops.privileged_control_available =
        reach_window_manager_privileged_control_available;
    out_port->ops.confirm_privileged_control_restart =
        reach_window_manager_confirm_privileged_control_restart;
    out_port->ops.start_privileged_control = reach_window_manager_start_privileged_control;
    out_port->ops.activate = reach_window_manager_activate;
    out_port->ops.minimize = reach_window_manager_minimize;
    out_port->ops.close = reach_window_manager_close;
    out_port->ops.destroy = reach_window_manager_destroy;
    return REACH_OK;
}
