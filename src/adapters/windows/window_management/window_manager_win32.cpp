#include "../windows_adapters_internal.h"

#include "elevation_helper_session_win32.h"
#include "window_display_state_win32.h"
#include "window_filter_win32.h"
#include "window_query_win32.h"

#include <windows.h>
#include <shobjidl.h>
#include <propkey.h>
#include <shellapi.h>
#include <shlwapi.h>

#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

struct reach_window_metadata_cache_entry
{
    HWND hwnd;
    DWORD process_id;
    int32_t app_user_model_id_queried;
    uint16_t app_user_model_id[260];
    int32_t process_path_queried;
    uint16_t process_path[260];
    int32_t title_queried;
    uint16_t title[260];
};

struct reach_window_metadata_job
{
    HWND hwnd;
    DWORD process_id;
    uint32_t generation;
};

struct reach_window_metadata_result
{
    HWND hwnd;
    DWORD process_id;
    uint32_t generation;
    int32_t has_app_user_model_id;
    uint16_t app_user_model_id[260];
    int32_t has_process_path;
    uint16_t process_path[260];
};

struct reach_window_manager
{
    HWINEVENTHOOK create_hook;
    HWINEVENTHOOK destroy_hook;
    HWINEVENTHOOK foreground_hook;
    HWINEVENTHOOK show_hook;
    HWINEVENTHOOK hide_hook;
    HWINEVENTHOOK minimize_start_hook;
    HWINEVENTHOOK minimize_end_hook;
    HWINEVENTHOOK location_hook;
    HWINEVENTHOOK name_change_hook;
    HWND foreground;
    std::vector<reach_window_snapshot> windows;
    std::vector<HWND> window_order;
    std::vector<reach_window_snapshot> pending_windows;
    std::vector<reach_window_metadata_cache_entry> metadata_cache;
    std::thread metadata_thread;
    std::mutex metadata_mutex;
    std::condition_variable metadata_cv;
    std::vector<reach_window_metadata_job> metadata_jobs;
    std::vector<reach_window_metadata_result> metadata_results;
    uint32_t metadata_generation;
    int32_t metadata_thread_started;
    int32_t metadata_stop_requested;
    int32_t dirty;
    int32_t pending_location_change;
    LONG helper_prompt_active;
    LONG helper_start_active;
    DWORD helper_retry_suppressed_until;
};

static reach_window_manager *g_window_manager;
static const DWORD REACH_HELPER_RESTART_DECLINED_COOLDOWN_MS = 10000;
static const DWORD REACH_HELPER_RESTART_FAILED_COOLDOWN_MS = 5000;

static void reach_window_manager_mark_seen(reach_window_manager *manager, HWND hwnd);

static void reach_window_manager_invalidate_title_cache(reach_window_manager *manager, HWND hwnd);
static reach_window_metadata_cache_entry *
reach_window_manager_find_metadata_cache(reach_window_manager *manager, HWND hwnd,
                                         DWORD process_id);
static int32_t reach_window_manager_update_snapshot_placement(reach_window_manager *manager,
                                                              HWND hwnd,
                                                              reach_window_snapshot *snapshot);
static reach_result reach_window_manager_start_metadata_worker(reach_window_manager *manager);
static void reach_window_manager_stop_metadata_worker(reach_window_manager *manager);

static void reach_window_manager_metadata_thread_main(reach_window_manager *manager)
{
    for (;;)
    {
        reach_window_metadata_job job = {};
        {
            std::unique_lock<std::mutex> lock(manager->metadata_mutex);
            manager->metadata_cv.wait(
                lock, [manager]()
                { return manager->metadata_stop_requested || !manager->metadata_jobs.empty(); });

            if (manager->metadata_stop_requested)
            {
                return;
            }

            job = manager->metadata_jobs.front();
            manager->metadata_jobs.erase(manager->metadata_jobs.begin());
        }

        reach_window_metadata_result result = {};
        result.hwnd = job.hwnd;
        result.process_id = job.process_id;
        result.generation = job.generation;

        if (job.hwnd != nullptr && IsWindow(job.hwnd))
        {
            result.has_app_user_model_id =
                reach_window_app_user_model_id(job.hwnd, result.app_user_model_id, 260);
        }

        if (!result.has_app_user_model_id)
        {
            result.has_app_user_model_id = reach_window_process_app_user_model_id_for_process(
                job.process_id, result.app_user_model_id, 260);
        }

        result.has_process_path =
            reach_window_query_process_path_for_process(job.process_id, result.process_path, 260);

        {
            std::lock_guard<std::mutex> lock(manager->metadata_mutex);
            if (!manager->metadata_stop_requested)
            {
                manager->metadata_results.push_back(result);
            }
        }
    }
}

static reach_result reach_window_manager_start_metadata_worker(reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (manager->metadata_thread_started)
    {
        return REACH_OK;
    }

    manager->metadata_stop_requested = 0;

    try
    {
        manager->metadata_thread = std::thread(reach_window_manager_metadata_thread_main, manager);
    }
    catch (...)
    {
        return REACH_ERROR;
    }

    manager->metadata_thread_started = 1;
    return REACH_OK;
}

static void reach_window_manager_stop_metadata_worker(reach_window_manager *manager)
{
    if (manager == nullptr || !manager->metadata_thread_started)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(manager->metadata_mutex);
        manager->metadata_stop_requested = 1;
        manager->metadata_jobs.clear();
        manager->metadata_results.clear();
    }

    manager->metadata_cv.notify_one();

    if (manager->metadata_thread.joinable())
    {
        manager->metadata_thread.join();
    }

    manager->metadata_thread_started = 0;
    manager->metadata_stop_requested = 0;
}

static void reach_window_manager_schedule_metadata_job(reach_window_manager *manager, HWND hwnd,
                                                       DWORD process_id)
{
    if (manager == nullptr || hwnd == nullptr || process_id == 0)
    {
        return;
    }

    if (reach_window_manager_start_metadata_worker(manager) != REACH_OK)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(manager->metadata_mutex);

        for (const reach_window_metadata_job &job : manager->metadata_jobs)
        {
            if (job.hwnd == hwnd && job.process_id == process_id)
            {
                return;
            }
        }

        reach_window_metadata_job job = {};
        job.hwnd = hwnd;
        job.process_id = process_id;
        job.generation = manager->metadata_generation;
        manager->metadata_jobs.push_back(job);
    }

    manager->metadata_cv.notify_one();
}

static int32_t reach_window_manager_apply_metadata_results(reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return 0;
    }

    std::vector<reach_window_metadata_result> results;
    {
        std::lock_guard<std::mutex> lock(manager->metadata_mutex);
        results.swap(manager->metadata_results);
    }

    int32_t changed = 0;

    for (const reach_window_metadata_result &result : results)
    {
        if (result.generation != manager->metadata_generation)
        {
            continue;
        }

        if (result.hwnd == nullptr || !IsWindow(result.hwnd) || result.process_id == 0)
        {
            continue;
        }

        reach_window_metadata_cache_entry *cache =
            reach_window_manager_find_metadata_cache(manager, result.hwnd, result.process_id);
        if (cache == nullptr)
        {
            continue;
        }

        if (result.has_app_user_model_id)
        {
            reach_copy_utf16(cache->app_user_model_id, 260, result.app_user_model_id);
        }
        cache->app_user_model_id_queried = 1;

        if (result.has_process_path)
        {
            reach_copy_utf16(cache->process_path, 260, result.process_path);
        }
        cache->process_path_queried = 1;

        changed = 1;
    }

    return changed;
}

static reach_result reach_window_manager_send_helper(reach_elevation_helper_command command,
                                                     uintptr_t window_id, reach_split_mode mode)
{
    if (reach_elevation_helper_session_get_state() == REACH_ELEVATION_HELPER_SESSION_CONNECTED)
    {
        reach_result helper_result =
            reach_elevation_helper_session_send(command, window_id, mode);
        if (helper_result == REACH_OK)
        {
            return REACH_OK;
        }

        if (reach_elevation_helper_session_get_state() ==
            REACH_ELEVATION_HELPER_SESSION_CONNECTED)
        {
            return helper_result;
        }
    }

    return REACH_ERROR;
}

static int32_t reach_window_manager_privileged_control_available(
    const reach_window_manager *manager)
{
    if (manager != nullptr &&
        reach_elevation_helper_session_get_state() == REACH_ELEVATION_HELPER_SESSION_CONNECTED)
    {
        const_cast<reach_window_manager *>(manager)->helper_retry_suppressed_until = 0;
        return 1;
    }
    return reach_elevation_helper_session_get_state() == REACH_ELEVATION_HELPER_SESSION_CONNECTED;
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

    if (reach_elevation_helper_session_reconnect() == REACH_OK)
    {
        reach_result wait_result = reach_elevation_helper_session_wait_connected(1000);
        InterlockedExchange(&manager->helper_start_active, 0);
        if (wait_result == REACH_OK)
        {
            manager->helper_retry_suppressed_until = 0;
        }
        else
        {
            reach_window_manager_suppress_helper_retry(
                manager, REACH_HELPER_RESTART_FAILED_COOLDOWN_MS);
        }
        return wait_result;
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
        if (reach_elevation_helper_session_reconnect() == REACH_OK)
        {
            reach_result wait_result = reach_elevation_helper_session_wait_connected(1000);
            InterlockedExchange(&manager->helper_start_active, 0);
            if (wait_result == REACH_OK)
            {
                manager->helper_retry_suppressed_until = 0;
            }
            else
            {
                reach_window_manager_suppress_helper_retry(
                    manager, REACH_HELPER_RESTART_FAILED_COOLDOWN_MS);
            }
            return wait_result;
        }
        Sleep(50);
    }

    InterlockedExchange(&manager->helper_start_active, 0);
    reach_window_manager_suppress_helper_retry(manager, REACH_HELPER_RESTART_FAILED_COOLDOWN_MS);
    return REACH_ERROR;
}

static void CALLBACK reach_window_manager_event_proc(HWINEVENTHOOK hook, DWORD event, HWND window,
                                                     LONG object_id, LONG child_id,
                                                     DWORD event_thread, DWORD event_time)
{
    (void)hook;
    (void)child_id;
    (void)event_thread;
    (void)event_time;

    if (g_window_manager == nullptr)
    {
        return;
    }

    const bool is_relevant_event =
        event == EVENT_SYSTEM_FOREGROUND || event == EVENT_SYSTEM_MINIMIZESTART ||
        event == EVENT_SYSTEM_MINIMIZEEND || event == EVENT_OBJECT_CREATE ||
        event == EVENT_OBJECT_SHOW || event == EVENT_OBJECT_HIDE || event == EVENT_OBJECT_DESTROY ||
        event == EVENT_OBJECT_LOCATIONCHANGE || event == EVENT_OBJECT_NAMECHANGE;

    if (!is_relevant_event)
    {
        return;
    }

    if (event != EVENT_SYSTEM_FOREGROUND)
    {
        if (object_id != OBJID_WINDOW)
        {
            return;
        }

        if (event != EVENT_OBJECT_DESTROY)
        {
            if (window == nullptr || !IsWindow(window))
            {
                return;
            }

            if (GetAncestor(window, GA_ROOT) != window)
            {
                return;
            }
        }
    }

    if (event == EVENT_OBJECT_LOCATIONCHANGE && window != GetForegroundWindow())
    {
        return;
    }

    if (event == EVENT_OBJECT_LOCATIONCHANGE && g_window_manager->dirty &&
        g_window_manager->pending_location_change)
    {
        return;
    }

    if (event == EVENT_SYSTEM_FOREGROUND || !g_window_manager->dirty)
    {
        g_window_manager->foreground = GetForegroundWindow();
    }

    if (event == EVENT_SYSTEM_FOREGROUND && reach_window_is_app(g_window_manager->foreground))
    {
        reach_window_manager_mark_seen(g_window_manager, g_window_manager->foreground);
    }

    if (event == EVENT_OBJECT_NAMECHANGE)
    {
        reach_window_manager_invalidate_title_cache(g_window_manager, window);
    }

    g_window_manager->pending_location_change = event == EVENT_OBJECT_LOCATIONCHANGE ? 1 : 0;
    g_window_manager->dirty = 1;
}

static void reach_window_manager_unhook(HWINEVENTHOOK *hook)
{
    if (hook != nullptr && *hook != nullptr)
    {
        UnhookWinEvent(*hook);
        *hook = nullptr;
    }
}

static reach_result reach_window_manager_stop(reach_window_manager *manager);

static reach_result reach_window_manager_start(reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    if (manager == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    (void)reach_window_manager_stop(manager);
    g_window_manager = manager;
    if (reach_window_manager_start_metadata_worker(manager) != REACH_OK)
    {
        g_window_manager = nullptr;
        return REACH_ERROR;
    }
    manager->dirty = 1;
    manager->create_hook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_CREATE, nullptr,
                                           reach_window_manager_event_proc, 0, 0,
                                           WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    manager->destroy_hook = SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, nullptr,
                                            reach_window_manager_event_proc, 0, 0,
                                            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    manager->foreground_hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                                               nullptr, reach_window_manager_event_proc, 0, 0,
                                               WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    manager->show_hook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, nullptr,
                                         reach_window_manager_event_proc, 0, 0,
                                         WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    manager->hide_hook = SetWinEventHook(EVENT_OBJECT_HIDE, EVENT_OBJECT_HIDE, nullptr,
                                         reach_window_manager_event_proc, 0, 0,
                                         WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    manager->minimize_start_hook = SetWinEventHook(
        EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZESTART, nullptr,
        reach_window_manager_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    manager->minimize_end_hook = SetWinEventHook(EVENT_SYSTEM_MINIMIZEEND, EVENT_SYSTEM_MINIMIZEEND,
                                                 nullptr, reach_window_manager_event_proc, 0, 0,
                                                 WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    manager->location_hook = SetWinEventHook(
        EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr,
        reach_window_manager_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    manager->name_change_hook = SetWinEventHook(EVENT_OBJECT_NAMECHANGE, EVENT_OBJECT_NAMECHANGE,
                                                nullptr, reach_window_manager_event_proc, 0, 0,
                                                WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    return manager->create_hook != nullptr && manager->destroy_hook != nullptr &&
                   manager->foreground_hook != nullptr && manager->show_hook != nullptr &&
                   manager->hide_hook != nullptr && manager->minimize_start_hook != nullptr &&
                   manager->minimize_end_hook != nullptr && manager->location_hook != nullptr &&
                   manager->name_change_hook != nullptr
               ? REACH_OK
               : REACH_ERROR;
}

static reach_result reach_window_manager_stop(reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    if (manager == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_window_manager_stop_metadata_worker(manager);
    reach_window_manager_unhook(&manager->create_hook);
    reach_window_manager_unhook(&manager->destroy_hook);
    reach_window_manager_unhook(&manager->foreground_hook);
    reach_window_manager_unhook(&manager->show_hook);
    reach_window_manager_unhook(&manager->hide_hook);
    reach_window_manager_unhook(&manager->minimize_start_hook);
    reach_window_manager_unhook(&manager->minimize_end_hook);
    reach_window_manager_unhook(&manager->location_hook);
    reach_window_manager_unhook(&manager->name_change_hook);
    if (g_window_manager == manager)
    {
        g_window_manager = nullptr;
    }
    return REACH_OK;
}

static int32_t reach_window_manager_contains(const std::vector<HWND> &windows, HWND hwnd)
{
    for (HWND existing : windows)
    {
        if (existing == hwnd)
        {
            return 1;
        }
    }

    return 0;
}

static void reach_window_manager_remove_ordered_window(std::vector<HWND> &windows, HWND hwnd)
{
    for (size_t index = 0; index < windows.size();)
    {
        if (windows[index] == hwnd)
        {
            windows.erase(windows.begin() + index);
        }
        else
        {
            ++index;
        }
    }
}

static void reach_window_manager_mark_seen(reach_window_manager *manager, HWND hwnd)
{
    if (manager == nullptr || hwnd == nullptr)
    {
        return;
    }

    reach_window_manager_remove_ordered_window(manager->window_order, hwnd);
    manager->window_order.insert(manager->window_order.begin(), hwnd);
}

static reach_window_metadata_cache_entry *
reach_window_manager_find_metadata_cache(reach_window_manager *manager, HWND hwnd, DWORD process_id)
{
    if (manager == nullptr || hwnd == nullptr || process_id == 0)
    {
        return nullptr;
    }

    for (reach_window_metadata_cache_entry &entry : manager->metadata_cache)
    {
        if (entry.hwnd == hwnd && entry.process_id == process_id)
        {
            return &entry;
        }
    }

    reach_window_metadata_cache_entry entry = {};
    entry.hwnd = hwnd;
    entry.process_id = process_id;
    manager->metadata_cache.push_back(entry);
    return &manager->metadata_cache.back();
}

static void reach_window_manager_invalidate_title_cache(reach_window_manager *manager, HWND hwnd)
{
    if (manager == nullptr || hwnd == nullptr)
    {
        return;
    }

    for (reach_window_metadata_cache_entry &entry : manager->metadata_cache)
    {
        if (entry.hwnd == hwnd)
        {
            entry.title_queried = 0;
            entry.title[0] = 0;
            return;
        }
    }
}

static int32_t
reach_window_manager_window_snapshot_contains(const std::vector<reach_window_snapshot> &windows,
                                              HWND hwnd)
{
    uintptr_t id = reinterpret_cast<uintptr_t>(hwnd);
    for (const reach_window_snapshot &snapshot : windows)
    {
        if (snapshot.id == id)
        {
            return 1;
        }
    }
    return 0;
}

static void reach_window_manager_trim_metadata_cache(reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < manager->metadata_cache.size();)
    {
        HWND hwnd = manager->metadata_cache[index].hwnd;
        if (hwnd == nullptr || !IsWindow(hwnd) ||
            !reach_window_manager_window_snapshot_contains(manager->windows, hwnd))
        {
            manager->metadata_cache.erase(manager->metadata_cache.begin() + index);
        }
        else
        {
            ++index;
        }
    }
}

static int32_t reach_window_manager_apply_cached_metadata(reach_window_manager *manager, HWND hwnd,
                                                          DWORD process_id,
                                                          reach_window_snapshot *snapshot)
{
    if (manager == nullptr || hwnd == nullptr || process_id == 0 || snapshot == nullptr)
    {
        return 0;
    }

    reach_window_metadata_cache_entry *cache =
        reach_window_manager_find_metadata_cache(manager, hwnd, process_id);
    if (cache == nullptr)
    {
        reach_window_manager_schedule_metadata_job(manager, hwnd, process_id);
        return 0;
    }

    int32_t needs_metadata = 0;

    if (cache->app_user_model_id_queried)
    {
        reach_copy_utf16(snapshot->app_user_model_id, 260, cache->app_user_model_id);
    }
    else
    {
        snapshot->app_user_model_id[0] = 0;
        needs_metadata = 1;
    }

    if (cache->title_queried)
    {
        reach_copy_utf16(snapshot->title, 260, cache->title);
    }
    else
    {
        wchar_t title[260] = {};
        GetWindowTextW(hwnd, title, 260);
        (void)reach_copy_utf16(cache->title, 260, reinterpret_cast<const uint16_t *>(title));
        cache->title_queried = 1;
        reach_copy_utf16(snapshot->title, 260, cache->title);
    }

    if (cache->process_path_queried)
    {
        reach_copy_utf16(snapshot->path, 260, cache->process_path);
    }
    else
    {
        snapshot->path[0] = 0;
        needs_metadata = 1;
    }

    if (needs_metadata)
    {
        reach_window_manager_schedule_metadata_job(manager, hwnd, process_id);
    }

    return snapshot->path[0] != 0;
}

static int32_t reach_window_manager_build_snapshot(reach_window_manager *manager, HWND hwnd,
                                                   reach_window_snapshot *out_snapshot)
{
    if (manager == nullptr || out_snapshot == nullptr || hwnd == nullptr || !IsWindow(hwnd))
    {
        return 0;
    }

    DWORD process_id = 0;
    GetWindowThreadProcessId(hwnd, &process_id);

    reach_window_snapshot snapshot = {};
    snapshot.id = reinterpret_cast<uintptr_t>(hwnd);

    if (!reach_window_manager_update_snapshot_placement(manager, hwnd, &snapshot))
    {
        return 0;
    }

    if (!reach_window_manager_apply_cached_metadata(manager, hwnd, process_id, &snapshot))
    {
        return 0;
    }

    *out_snapshot = snapshot;
    return 1;
}

static BOOL CALLBACK reach_window_manager_enum_windows_proc(HWND hwnd, LPARAM param)
{
    reach_window_manager *manager = reinterpret_cast<reach_window_manager *>(param);
    if (manager == nullptr || !reach_window_is_app(hwnd))
    {
        return TRUE;
    }

    reach_window_snapshot snapshot = {};
    if (reach_window_manager_build_snapshot(manager, hwnd, &snapshot))
    {
        if (snapshot.minimized)
        {
            snapshot.visible = 0;
        }
        manager->pending_windows.push_back(snapshot);
    }
    return TRUE;
}

static const reach_window_snapshot *
reach_window_manager_find_pending(const std::vector<reach_window_snapshot> &windows, HWND hwnd)
{
    uintptr_t id = reinterpret_cast<uintptr_t>(hwnd);
    for (const reach_window_snapshot &snapshot : windows)
    {
        if (snapshot.id == id)
        {
            return &snapshot;
        }
    }
    return nullptr;
}

static reach_window_snapshot *
reach_window_manager_find_window_snapshot(std::vector<reach_window_snapshot> &windows, HWND hwnd)
{
    uintptr_t id = reinterpret_cast<uintptr_t>(hwnd);
    for (reach_window_snapshot &snapshot : windows)
    {
        if (snapshot.id == id)
        {
            return &snapshot;
        }
    }
    return nullptr;
}

static int32_t reach_window_manager_update_snapshot_placement(reach_window_manager *manager,
                                                              HWND hwnd,
                                                              reach_window_snapshot *snapshot)
{
    if (manager == nullptr || hwnd == nullptr || snapshot == nullptr || !IsWindow(hwnd))
    {
        return 0;
    }

    snapshot->visible = IsWindowVisible(hwnd) ? 1 : 0;
    snapshot->maximized = IsZoomed(hwnd) ? 1 : 0;
    snapshot->minimized = IsIconic(hwnd) ? 1 : 0;

    RECT rect = {};
    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);

    if (snapshot->minimized && GetWindowPlacement(hwnd, &placement))
    {
        rect = placement.rcNormalPosition;
    }
    else
    {
        (void)GetWindowRect(hwnd, &rect);
    }

    snapshot->bounds = {rect.left, rect.top, rect.right, rect.bottom};

    if (snapshot->minimized)
    {
        snapshot->visible = 0;
    }

    return 1;
}

static int32_t reach_window_manager_refresh_location_change(reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return 0;
    }

    HWND foreground = GetForegroundWindow();
    if (foreground == nullptr || !IsWindow(foreground))
    {
        return 0;
    }

    reach_window_snapshot *snapshot =
        reach_window_manager_find_window_snapshot(manager->windows, foreground);
    if (snapshot == nullptr)
    {
        return 0;
    }

    if (!reach_window_manager_update_snapshot_placement(manager, foreground, snapshot))
    {
        return 0;
    }

    manager->foreground = foreground;
    reach_window_manager_mark_seen(manager, foreground);
    return 1;
}

static void reach_window_manager_refresh_windows(reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return;
    }

    manager->pending_windows.clear();
    EnumWindows(reach_window_manager_enum_windows_proc, reinterpret_cast<LPARAM>(manager));

    for (size_t index = 0; index < manager->window_order.size();)
    {
        if (reach_window_manager_find_pending(manager->pending_windows,
                                              manager->window_order[index]) == nullptr)
        {
            manager->window_order.erase(manager->window_order.begin() + index);
        }
        else
        {
            ++index;
        }
    }

    for (const reach_window_snapshot &snapshot : manager->pending_windows)
    {
        HWND hwnd = reinterpret_cast<HWND>(snapshot.id);
        if (!reach_window_manager_contains(manager->window_order, hwnd))
        {
            manager->window_order.push_back(hwnd);
        }
    }

    manager->foreground = GetForegroundWindow();
    if (reach_window_manager_find_pending(manager->pending_windows, manager->foreground) != nullptr)
    {
        reach_window_manager_mark_seen(manager, manager->foreground);
    }

    manager->windows.clear();
    for (HWND hwnd : manager->window_order)
    {
        const reach_window_snapshot *snapshot =
            reach_window_manager_find_pending(manager->pending_windows, hwnd);
        if (snapshot != nullptr)
        {
            manager->windows.push_back(*snapshot);
        }
    }
    manager->pending_windows.clear();
    reach_window_manager_trim_metadata_cache(manager);
}

static reach_result reach_window_manager_refresh(reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    if (manager == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    int32_t metadata_changed = reach_window_manager_apply_metadata_results(manager);

    if (!metadata_changed && manager->pending_location_change &&
        reach_window_manager_refresh_location_change(manager))
    {
        manager->dirty = 0;
        manager->pending_location_change = 0;
        return REACH_OK;
    }

    reach_window_manager_refresh_windows(manager);
    manager->dirty = 0;
    manager->pending_location_change = 0;
    return REACH_OK;
}

static reach_result reach_window_manager_snap(reach_window_manager *manager, uintptr_t window_id,
                                              reach_split_mode mode)
{
    REACH_ASSERT(manager != nullptr);
    REACH_ASSERT(window_id != 0);
    if (manager == nullptr || window_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    HWND hwnd = reinterpret_cast<HWND>(window_id);
    if (!IsWindow(hwnd))
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result =
        reach_window_manager_send_helper(REACH_ELEVATION_HELPER_COMMAND_SNAP, window_id, mode);
    manager->foreground = GetForegroundWindow();
    manager->dirty = 1;
    return result;
}

static uintptr_t reach_window_manager_foreground(const reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    return reinterpret_cast<uintptr_t>(manager->foreground);
}

static int32_t reach_window_manager_foreground_is_maximized(const reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    return manager != nullptr && reach_window_is_app(manager->foreground) &&
           IsZoomed(manager->foreground);
}

static int32_t reach_window_manager_foreground_is_fullscreen(const reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    if (manager == nullptr)
    {
        return 0;
    }
    return reach_window_is_fullscreen_on_primary(manager->foreground);
}

static int32_t
reach_window_manager_foreground_is_exclusive_fullscreen(const reach_window_manager *manager)
{
    REACH_ASSERT(manager != nullptr);
    if (manager == nullptr)
    {
        return 0;
    }
    return reach_window_is_exclusive_fullscreen(manager->foreground);
}

static int32_t reach_window_manager_dock_should_auto_hide(const reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return 0;
    }
    if (reach_window_any_visible_exclusive_fullscreen_on_primary())
    {
        return 1;
    }
    if (reach_window_any_visible_fullscreen_on_primary())
    {
        return 1;
    }
    return reach_window_any_visible_maximized_on_primary();
}

static int32_t reach_window_manager_needs_refresh(const reach_window_manager *manager)
{
    if (manager == nullptr)
    {
        return 0;
    }

    if (manager->dirty)
    {
        return 1;
    }

    reach_window_manager *mutable_manager = const_cast<reach_window_manager *>(manager);
    std::lock_guard<std::mutex> lock(mutable_manager->metadata_mutex);
    return !mutable_manager->metadata_results.empty();
}

static size_t reach_window_manager_window_count(const reach_window_manager *manager)
{
    return manager == nullptr ? 0 : manager->windows.size();
}

static reach_result reach_window_manager_window_at(const reach_window_manager *manager,
                                                   size_t index, reach_window_snapshot *out_window)
{
    if (manager == nullptr || out_window == nullptr || index >= manager->windows.size())
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_window = manager->windows[index];
    return REACH_OK;
}

static reach_result reach_window_manager_pin_app_for_window(reach_window_manager *manager,
                                                            uintptr_t window_id,
                                                            const reach_window_snapshot *snapshot,
                                                            reach_pinned_app_model *out_app)
{
    (void)manager;
    if (window_id == 0 || out_app == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    HWND hwnd = reinterpret_cast<HWND>(window_id);
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_window_snapshot local_snapshot = {};
    if (snapshot == nullptr)
    {
        if (!reach_window_manager_build_snapshot(manager, hwnd, &local_snapshot))
        {
            return REACH_ERROR;
        }
        snapshot = &local_snapshot;
    }

    *out_app = {};

    (void)reach_copy_utf16(out_app->path, 260, snapshot->path);
    out_app->arguments[0] = 0;

    IPropertyStore *store = nullptr;
    HRESULT hr = SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&store));
    if (SUCCEEDED(hr) && store != nullptr)
    {
        uint16_t display_name[128] = {};
        if (reach_window_property_string(store, PKEY_AppUserModel_RelaunchDisplayNameResource,
                                         display_name, 128) &&
            display_name[0] != '@')
        {
            (void)reach_copy_utf16(out_app->title, 128, display_name);
        }

        store->Release();
    }

    if (out_app->icon_ref[0] == 0)
    {
        (void)reach_copy_utf16(out_app->icon_ref, 260, out_app->path);
    }

    if (out_app->app_user_model_id[0] == 0 && snapshot->app_user_model_id[0] != 0)
    {
        (void)reach_copy_utf16(out_app->app_user_model_id, 260, snapshot->app_user_model_id);
    }

    if (out_app->app_user_model_id[0] == 0)
    {
        (void)reach_window_app_user_model_id(hwnd, out_app->app_user_model_id, 260);
    }

    if (out_app->app_user_model_id[0] == 0)
    {
        (void)reach_window_process_app_user_model_id(hwnd, out_app->app_user_model_id, 260);
    }

    return out_app->path[0] != 0 ? REACH_OK : REACH_ERROR;
}

static reach_result reach_window_manager_activate(reach_window_manager *manager,
                                                  uintptr_t window_id)
{
    if (manager == nullptr || window_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    HWND hwnd = reinterpret_cast<HWND>(window_id);
    if (!IsWindow(hwnd))
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result activate_result = reach_window_manager_send_helper(
        REACH_ELEVATION_HELPER_COMMAND_ACTIVATE, window_id, REACH_SPLIT_LEFT);
    manager->foreground = GetForegroundWindow();
    if (activate_result != REACH_OK)
    {
        manager->dirty = 1;
        return activate_result;
    }

    reach_window_manager_mark_seen(manager, hwnd);
    manager->dirty = 1;

    return REACH_OK;
}

static reach_result reach_window_manager_minimize(reach_window_manager *manager,
                                                  uintptr_t window_id)
{
    if (manager == nullptr || window_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    HWND hwnd = reinterpret_cast<HWND>(window_id);
    if (!IsWindow(hwnd))
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = reach_window_manager_send_helper(REACH_ELEVATION_HELPER_COMMAND_MINIMIZE,
                                                           window_id, REACH_SPLIT_LEFT);

    manager->foreground = GetForegroundWindow();
    manager->dirty = 1;

    return result;
}

static reach_result reach_window_manager_close(reach_window_manager *manager, uintptr_t window_id)
{
    if (manager == nullptr || window_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    HWND hwnd = reinterpret_cast<HWND>(window_id);
    if (!IsWindow(hwnd))
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_result result = reach_window_manager_send_helper(REACH_ELEVATION_HELPER_COMMAND_CLOSE,
                                                           window_id, REACH_SPLIT_LEFT);
    manager->dirty = 1;
    return result;
}

static reach_result reach_window_manager_kill_process(reach_window_manager *manager,
                                                      uintptr_t window_id)
{
    if (manager == nullptr || window_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    HWND hwnd = reinterpret_cast<HWND>(window_id);
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD process_id = 0;
    GetWindowThreadProcessId(hwnd, &process_id);

    if (process_id == 0 || process_id == GetCurrentProcessId())
    {
        return REACH_INVALID_ARGUMENT;
    }

    HANDLE process =
        OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);

    if (process == nullptr)
    {
        return REACH_ERROR;
    }

    BOOL terminated = TerminateProcess(process, 1);
    CloseHandle(process);

    if (!terminated)
    {
        return REACH_ERROR;
    }

    manager->dirty = 1;
    return REACH_OK;
}

static void reach_window_manager_destroy(reach_window_manager *manager)
{
    if (manager != nullptr)
    {
        (void)reach_window_manager_stop(manager);
    }
    delete manager;
}

reach_result reach_windows_create_window_manager(reach_window_manager_port *out_port)
{
    REACH_ASSERT(out_port != nullptr);
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

    manager->foreground = GetForegroundWindow();
    reach_window_manager_refresh_windows(manager);
    out_port->manager = manager;
    out_port->ops.start = reach_window_manager_start;
    out_port->ops.stop = reach_window_manager_stop;
    out_port->ops.refresh = reach_window_manager_refresh;
    out_port->ops.snap = reach_window_manager_snap;
    out_port->ops.foreground = reach_window_manager_foreground;
    out_port->ops.foreground_is_maximized = reach_window_manager_foreground_is_maximized;
    out_port->ops.foreground_is_fullscreen = reach_window_manager_foreground_is_fullscreen;
    out_port->ops.foreground_is_exclusive_fullscreen =
        reach_window_manager_foreground_is_exclusive_fullscreen;
    out_port->ops.dock_should_auto_hide = reach_window_manager_dock_should_auto_hide;
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
    out_port->ops.kill_process = reach_window_manager_kill_process;
    out_port->ops.destroy = reach_window_manager_destroy;
    return REACH_OK;
}
