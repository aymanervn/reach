#include "reach/protocol/reach_service_protocol.h"
#include "../../adapters/windows/window_management/reach_service_shared_state_win32.h"
#include "../../adapters/windows/window_management/window_query_win32.h"
#include "window_actions.h"
#include "hotkeys.h"

#include <windows.h>
#include <sddl.h>

#include <vector>

struct reach_helper_session_state
{
    HANDLE window_event_thread;
    DWORD window_event_thread_id;
    HWINEVENTHOOK create_hook;
    HWINEVENTHOOK destroy_hook;
    HWINEVENTHOOK show_hook;
    HWINEVENTHOOK hide_hook;
    HWINEVENTHOOK minimize_start_hook;
    HWINEVENTHOOK minimize_end_hook;
    HWINEVENTHOOK foreground_hook;
    HWINEVENTHOOK location_hook;
    HWINEVENTHOOK name_hook;
};

struct reach_helper_window_metadata
{
    HWND hwnd;
    DWORD process_id;
    uint64_t seen_generation;
    wchar_t class_name[128];
    wchar_t process_path[260];
    wchar_t app_user_model_id[260];
    wchar_t integrity[32];
};

struct reach_helper_window_state
{
    uint32_t window_count;
    reach_service_window_snapshot windows[REACH_SERVICE_MAX_WINDOWS];
};

static reach_helper_session_state g_session;
static INIT_ONCE g_metadata_cache_once = INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION g_metadata_cache_lock;
static std::vector<reach_helper_window_metadata> g_metadata_cache;
static uint64_t g_metadata_generation;
static INIT_ONCE g_window_state_once = INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION g_window_state_lock;
static reach_helper_window_state g_window_state;
static LONG g_game_mode_active;
/* The shell-instance mutex name is the shared protocol constant
   REACH_SHELL_INSTANCE_MUTEX_NAME (reach_service_protocol.h). */
static const wchar_t *REACH_SHELL_INSTANCE_MUTEX = REACH_SHELL_INSTANCE_MUTEX_NAME;
static const wchar_t *REACH_HELPER_INSTANCE_MUTEX = L"Local\\ReachServiceInstance";
static const UINT REACH_HELPER_WM_MINIMIZE_GAME = WM_APP + 41;

static reach_result reach_helper_execute(const reach_service_request *request,
                                         reach_service_response *response);
static void reach_helper_publish_window_state(void);

static void reach_helper_copy_wide(wchar_t *destination, size_t destination_count,
                                   const wchar_t *source)
{
    if (destination == nullptr || destination_count == 0)
    {
        return;
    }
    destination[0] = 0;
    if (source != nullptr)
    {
        lstrcpynW(destination, source, static_cast<int>(destination_count));
    }
}

static int32_t reach_helper_string_in_list(const wchar_t *value, const wchar_t *const *items,
                                           size_t count)
{
    if (value == nullptr)
    {
        return 0;
    }
    for (size_t index = 0; index < count; ++index)
    {
        if (lstrcmpiW(value, items[index]) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static int32_t reach_helper_string_has_prefix(const wchar_t *value, const wchar_t *const *prefixes,
                                              size_t count)
{
    if (value == nullptr)
    {
        return 0;
    }
    for (size_t index = 0; index < count; ++index)
    {
        size_t prefix_length = lstrlenW(prefixes[index]);
        if (_wcsnicmp(value, prefixes[index], prefix_length) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static int32_t reach_helper_is_reach_window_class(const wchar_t *class_name)
{
    return lstrcmpiW(class_name, L"ReachPlatformWindow") == 0 ||
           lstrcmpiW(class_name, L"ReachInputMessageWindow") == 0 ||
           lstrcmpiW(class_name, L"ReachWallpaperWindow") == 0;
}

static int32_t reach_helper_window_cloaked(HWND hwnd)
{
    typedef HRESULT(WINAPI * reach_dwm_get_window_attribute_fn)(HWND, DWORD, PVOID, DWORD);
    static HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    static reach_dwm_get_window_attribute_fn get_window_attribute =
        dwm != nullptr ? reinterpret_cast<reach_dwm_get_window_attribute_fn>(
                             GetProcAddress(dwm, "DwmGetWindowAttribute"))
                       : nullptr;
    static const DWORD reach_dwma_cloaked = 14;
    DWORD cloaked = 0;
    HRESULT hr = get_window_attribute != nullptr
                     ? get_window_attribute(hwnd, reach_dwma_cloaked, &cloaked, sizeof(cloaked))
                     : E_FAIL;
    return SUCCEEDED(hr) && cloaked != 0;
}

static int32_t reach_helper_window_has_foreground(HWND hwnd)
{
    HWND foreground = GetForegroundWindow();
    if (hwnd == nullptr || foreground == nullptr)
    {
        return 0;
    }

    if (foreground == hwnd || GetAncestor(foreground, GA_ROOT) == hwnd ||
        GetAncestor(foreground, GA_ROOTOWNER) == hwnd)
    {
        return 1;
    }

    HWND root_owner = GetAncestor(hwnd, GA_ROOTOWNER);
    HWND popup = root_owner != nullptr ? GetLastActivePopup(root_owner) : nullptr;
    return popup != nullptr && (foreground == popup || GetAncestor(foreground, GA_ROOT) == popup ||
                                GetAncestor(foreground, GA_ROOTOWNER) == popup);
}

static BOOL CALLBACK reach_helper_initialize_metadata_cache(PINIT_ONCE init_once, PVOID parameter,
                                                            PVOID *context)
{
    (void)init_once;
    (void)parameter;
    (void)context;
    InitializeCriticalSection(&g_metadata_cache_lock);
    return TRUE;
}

static void reach_helper_lock_metadata_cache(void)
{
    InitOnceExecuteOnce(&g_metadata_cache_once, reach_helper_initialize_metadata_cache, nullptr,
                        nullptr);
    EnterCriticalSection(&g_metadata_cache_lock);
}

static void reach_helper_unlock_metadata_cache(void)
{
    LeaveCriticalSection(&g_metadata_cache_lock);
}

static BOOL CALLBACK reach_helper_initialize_window_state(PINIT_ONCE init_once, PVOID parameter,
                                                          PVOID *context)
{
    (void)init_once;
    (void)parameter;
    (void)context;
    InitializeCriticalSection(&g_window_state_lock);
    return TRUE;
}

static void reach_helper_lock_window_state(void)
{
    InitOnceExecuteOnce(&g_window_state_once, reach_helper_initialize_window_state, nullptr,
                        nullptr);
    EnterCriticalSection(&g_window_state_lock);
}

static void reach_helper_unlock_window_state(void)
{
    LeaveCriticalSection(&g_window_state_lock);
}

static void reach_helper_process_path(DWORD process_id, wchar_t *path, size_t path_count)
{
    if (path == nullptr || path_count == 0)
    {
        return;
    }
    path[0] = 0;
    if (process_id == 0)
    {
        return;
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr)
    {
        return;
    }

    DWORD size = static_cast<DWORD>(path_count);
    if (!QueryFullProcessImageNameW(process, 0, path, &size))
    {
        path[0] = 0;
    }
    CloseHandle(process);
}

static void reach_helper_process_integrity_text(DWORD process_id, wchar_t *text, size_t text_count)
{
    if (text == nullptr || text_count == 0)
    {
        return;
    }
    text[0] = 0;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr)
    {
        reach_helper_copy_wide(text, text_count, L"unknown");
        return;
    }

    HANDLE token = nullptr;
    if (!OpenProcessToken(process, TOKEN_QUERY, &token))
    {
        CloseHandle(process);
        reach_helper_copy_wide(text, text_count, L"unknown");
        return;
    }

    BYTE buffer[512] = {};
    DWORD needed = 0;
    DWORD integrity = 0;
    if (GetTokenInformation(token, TokenIntegrityLevel, buffer, sizeof(buffer), &needed))
    {
        TOKEN_MANDATORY_LABEL *label = reinterpret_cast<TOKEN_MANDATORY_LABEL *>(buffer);
        DWORD sub_authority_count = *GetSidSubAuthorityCount(label->Label.Sid);
        integrity = *GetSidSubAuthority(label->Label.Sid, sub_authority_count - 1);
    }

    CloseHandle(token);
    CloseHandle(process);

    if (integrity >= SECURITY_MANDATORY_SYSTEM_RID)
    {
        reach_helper_copy_wide(text, text_count, L"system");
    }
    else if (integrity >= SECURITY_MANDATORY_HIGH_RID)
    {
        reach_helper_copy_wide(text, text_count, L"high");
    }
    else if (integrity >= SECURITY_MANDATORY_MEDIUM_RID)
    {
        reach_helper_copy_wide(text, text_count, L"medium");
    }
    else if (integrity >= SECURITY_MANDATORY_LOW_RID)
    {
        reach_helper_copy_wide(text, text_count, L"low");
    }
    else
    {
        reach_helper_copy_wide(text, text_count, L"unknown");
    }
}

static void reach_helper_copy_metadata_to_snapshot(const reach_helper_window_metadata *metadata,
                                                   reach_service_window_snapshot *snapshot)
{
    if (metadata == nullptr || snapshot == nullptr)
    {
        return;
    }
    reach_helper_copy_wide(snapshot->class_name, 128, metadata->class_name);
    reach_helper_copy_wide(snapshot->process_path, 260, metadata->process_path);
    reach_helper_copy_wide(snapshot->app_user_model_id, 260, metadata->app_user_model_id);
    reach_helper_copy_wide(snapshot->integrity, 32, metadata->integrity);
}

static int32_t reach_helper_cached_window_metadata(HWND hwnd, DWORD process_id,
                                                   uint64_t seen_generation,
                                                   reach_service_window_snapshot *snapshot)
{
    if (hwnd == nullptr || snapshot == nullptr)
    {
        return 0;
    }

    int32_t found = 0;
    reach_helper_lock_metadata_cache();
    for (reach_helper_window_metadata &metadata : g_metadata_cache)
    {
        if (metadata.hwnd == hwnd && metadata.process_id == process_id)
        {
            metadata.seen_generation = seen_generation;
            reach_helper_copy_metadata_to_snapshot(&metadata, snapshot);
            found = 1;
            break;
        }
    }
    reach_helper_unlock_metadata_cache();
    return found;
}

static void reach_helper_query_window_metadata(HWND hwnd, DWORD process_id,
                                               uint64_t seen_generation,
                                               reach_service_window_snapshot *snapshot)
{
    if (hwnd == nullptr || snapshot == nullptr)
    {
        return;
    }

    reach_helper_window_metadata metadata = {};
    metadata.hwnd = hwnd;
    metadata.process_id = process_id;
    metadata.seen_generation = seen_generation;
    GetClassNameW(hwnd, metadata.class_name, 128);
    reach_helper_process_path(process_id, metadata.process_path, 260);
    if (!reach_window_app_user_model_id(
            hwnd, reinterpret_cast<uint16_t *>(metadata.app_user_model_id), 260))
    {
        (void)reach_window_process_app_user_model_id_for_process(
            process_id, reinterpret_cast<uint16_t *>(metadata.app_user_model_id), 260);
    }
    reach_helper_process_integrity_text(process_id, metadata.integrity, 32);

    reach_helper_lock_metadata_cache();
    int32_t updated = 0;
    for (reach_helper_window_metadata &cached : g_metadata_cache)
    {
        if (cached.hwnd == hwnd)
        {
            cached = metadata;
            updated = 1;
            break;
        }
    }
    if (!updated)
    {
        g_metadata_cache.push_back(metadata);
    }
    reach_helper_unlock_metadata_cache();

    reach_helper_copy_metadata_to_snapshot(&metadata, snapshot);
}

static void reach_helper_load_window_metadata(HWND hwnd, DWORD process_id, uint64_t seen_generation,
                                              reach_service_window_snapshot *snapshot)
{
    if (!reach_helper_cached_window_metadata(hwnd, process_id, seen_generation, snapshot))
    {
        reach_helper_query_window_metadata(hwnd, process_id, seen_generation, snapshot);
    }
}

static void reach_helper_forget_window_metadata(HWND hwnd)
{
    if (hwnd == nullptr)
    {
        return;
    }

    reach_helper_lock_metadata_cache();
    for (size_t index = 0; index < g_metadata_cache.size();)
    {
        if (g_metadata_cache[index].hwnd == hwnd)
        {
            g_metadata_cache.erase(g_metadata_cache.begin() + index);
        }
        else
        {
            ++index;
        }
    }
    reach_helper_unlock_metadata_cache();
}

static void reach_helper_prune_window_metadata(uint64_t seen_generation)
{
    reach_helper_lock_metadata_cache();
    for (size_t index = 0; index < g_metadata_cache.size();)
    {
        if (g_metadata_cache[index].seen_generation != seen_generation ||
            !IsWindow(g_metadata_cache[index].hwnd))
        {
            g_metadata_cache.erase(g_metadata_cache.begin() + index);
        }
        else
        {
            ++index;
        }
    }
    reach_helper_unlock_metadata_cache();
}

static void reach_helper_classify_window(reach_service_window_snapshot *snapshot)
{
    if (snapshot == nullptr)
    {
        return;
    }

    static const wchar_t *helper_classes[] = {
        L"GDI+ Hook Window Class",
        L"OleDdeWndClass",
        L"ConsoleWindowClass",
        L"OperationStatusWindow",
        L"COMTASKSWINDOWCLASS",
        L"Dwm",
        L"PushNotificationsPowerManagement",
        L"BluetoothNotificationAreaIconWindowClass",
        L"MiracastConnectionWindow",
        L"MS_WebcheckMonitor",
        L"TscShellContainerClass",
        L"WorkerW",
        L"Progman",
        L"Shell_TrayWnd",
        L"Shell_SecondaryTrayWnd",
        L"DV2ControlHost",
        L"Windows.UI.Core.CoreWindow",
    };
    static const wchar_t *helper_class_prefixes[] = {
        L".NET-BroadcastEventWindow",
        L"HwndWrapper[",
        L"NvContainerWindowClass",
    };
    static const wchar_t *helper_titles[] = {
        L"DDE Server Window",
        L"MediaContextNotificationWindow",
        L"SystemResourceNotifyWindow",
        L"Windows Push Notifications Platform",
        L"DWM Notification Window",
        L"Task Host Window",
        L"RemoteApp",
    };
    static const wchar_t *helper_title_prefixes[] = {
        L"GDI+ Window",
        L".NET-BroadcastEventWindow",
    };

    snapshot->kind = REACH_SERVICE_WINDOW_UNKNOWN;
    snapshot->include_in_switcher = 0;

    HWND hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(snapshot->window));
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        snapshot->kind = REACH_SERVICE_WINDOW_UNKNOWN;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"not a window");
        return;
    }

    if (hwnd == GetShellWindow() || reach_helper_is_reach_window_class(snapshot->class_name))
    {
        snapshot->kind = REACH_SERVICE_WINDOW_SHELL;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"shell/reach surface");
        return;
    }

    if (snapshot->cloaked)
    {
        snapshot->kind = REACH_SERVICE_WINDOW_SYSTEM;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"cloaked window");
        return;
    }

    if (GetAncestor(hwnd, GA_ROOT) != hwnd)
    {
        snapshot->kind = REACH_SERVICE_WINDOW_HELPER;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"not root top-level window");
        return;
    }

    LONG_PTR ex_style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((ex_style & WS_EX_TOOLWINDOW) != 0)
    {
        snapshot->kind = REACH_SERVICE_WINDOW_HELPER;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"tool window");
        return;
    }

    int32_t app_window = (ex_style & WS_EX_APPWINDOW) != 0;
    int32_t helper_class =
        reach_helper_string_in_list(snapshot->class_name, helper_classes,
                                    sizeof(helper_classes) / sizeof(helper_classes[0])) ||
        reach_helper_string_has_prefix(snapshot->class_name, helper_class_prefixes,
                                       sizeof(helper_class_prefixes) /
                                           sizeof(helper_class_prefixes[0]));
    if (helper_class && !app_window)
    {
        snapshot->kind = REACH_SERVICE_WINDOW_HELPER;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"known helper class");
        return;
    }

    if (reach_helper_string_in_list(snapshot->title, helper_titles,
                                    sizeof(helper_titles) / sizeof(helper_titles[0])) ||
        reach_helper_string_has_prefix(snapshot->title, helper_title_prefixes,
                                       sizeof(helper_title_prefixes) /
                                           sizeof(helper_title_prefixes[0])))
    {
        snapshot->kind = REACH_SERVICE_WINDOW_HELPER;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"known helper title");
        return;
    }

    if (snapshot->iconic)
    {
        snapshot->kind = REACH_SERVICE_WINDOW_MINIMIZED_APP;
        snapshot->include_in_switcher = 1;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"accepted minimized app");
        return;
    }

    if (snapshot->title[0] == 0)
    {
        snapshot->kind = REACH_SERVICE_WINDOW_HELPER;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"empty title");
        return;
    }

    if (!snapshot->visible && !snapshot->iconic)
    {
        snapshot->kind = REACH_SERVICE_WINDOW_HELPER;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"hidden non-minimized");
        return;
    }

    snapshot->kind = REACH_SERVICE_WINDOW_APP;
    snapshot->include_in_switcher = 1;
    reach_helper_copy_wide(snapshot->classification_reason, 160, L"accepted visible app");
}

static reach_service_window_snapshot reach_helper_inspect_window(HWND hwnd)
{
    reach_service_window_snapshot snapshot = {};
    snapshot.window = reinterpret_cast<uint64_t>(hwnd);
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        reach_helper_classify_window(&snapshot);
        return snapshot;
    }

    DWORD process_id = 0;
    DWORD thread_id = GetWindowThreadProcessId(hwnd, &process_id);
    snapshot.process_id = process_id;
    snapshot.thread_id = thread_id;
    /* InternalGetWindowText reads the title without a synchronous message to
       the window's thread — a hung app (frozen while launching) must never
       stall the enumerator and stale every consumer's window data. */
    InternalGetWindowText(hwnd, snapshot.title, 260);
    reach_helper_load_window_metadata(hwnd, process_id, g_metadata_generation, &snapshot);
    snapshot.visible = IsWindowVisible(hwnd) ? 1 : 0;
    snapshot.iconic = IsIconic(hwnd) ? 1 : 0;
    snapshot.cloaked = reach_helper_window_cloaked(hwnd);
    snapshot.focused = reach_helper_window_has_foreground(hwnd);
    snapshot.enabled = IsWindowEnabled(hwnd) ? 1 : 0;
    snapshot.maximized = IsZoomed(hwnd) ? 1 : 0;
    reach_helper_classify_window(&snapshot);
    return snapshot;
}

static LONG reach_helper_abs_long(LONG value)
{
    return value < 0 ? -value : value;
}

static int32_t reach_helper_rect_matches_monitor(RECT window_rect, RECT monitor_rect)
{
    const LONG tolerance = 2;
    return reach_helper_abs_long(window_rect.left - monitor_rect.left) <= tolerance &&
           reach_helper_abs_long(window_rect.top - monitor_rect.top) <= tolerance &&
           reach_helper_abs_long(window_rect.right - monitor_rect.right) <= tolerance &&
           reach_helper_abs_long(window_rect.bottom - monitor_rect.bottom) <= tolerance;
}

static int32_t reach_helper_window_occupies_whole_monitor(HWND hwnd, RECT window_rect)
{
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (monitor == nullptr)
    {
        return 0;
    }

    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info))
    {
        return 0;
    }

    return reach_helper_rect_matches_monitor(window_rect, info.rcMonitor);
}

static int32_t reach_helper_window_is_game(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd) || !IsWindowVisible(hwnd) || IsIconic(hwnd) ||
        IsZoomed(hwnd))
    {
        return 0;
    }

    RECT rect = {};
    if (!GetWindowRect(hwnd, &rect))
    {
        return 0;
    }

    return reach_helper_window_occupies_whole_monitor(hwnd, rect);
}

static int32_t reach_helper_rect_contains_rect(RECT outer, RECT inner)
{
    const LONG tolerance = 2;
    return inner.left >= outer.left - tolerance && inner.top >= outer.top - tolerance &&
           inner.right <= outer.right + tolerance && inner.bottom <= outer.bottom + tolerance;
}

static int32_t reach_helper_rect_is_virtual_screen(RECT rect)
{
    RECT virtual_screen = {};
    virtual_screen.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    virtual_screen.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    virtual_screen.right = virtual_screen.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    virtual_screen.bottom = virtual_screen.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return reach_helper_rect_matches_monitor(rect, virtual_screen);
}

static void reach_helper_release_game_cursor_clip(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return;
    }

    RECT clip = {};
    if (!GetClipCursor(&clip) || reach_helper_rect_is_virtual_screen(clip))
    {
        return;
    }

    RECT window_rect = {};
    if (!GetWindowRect(hwnd, &window_rect))
    {
        return;
    }

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (monitor == nullptr || !GetMonitorInfoW(monitor, &info))
    {
        return;
    }

    if (reach_helper_rect_contains_rect(window_rect, clip) ||
        reach_helper_rect_contains_rect(info.rcMonitor, clip))
    {
        (void)ClipCursor(nullptr);
    }
}

static int32_t reach_helper_detect_game_mode(void)
{
    return reach_helper_window_is_game(GetForegroundWindow());
}

static void reach_helper_publish_game_mode(void)
{
    int32_t active = reach_helper_detect_game_mode();
    if (active)
    {
        reach_helper_clear_hotkey_state();
    }
    InterlockedExchange(&g_game_mode_active, active ? 1 : 0);
    (void)reach_service_shared_publish_game_mode(active);
}

struct reach_helper_snapshot_builder
{
    reach_service_response *response;
};
static BOOL CALLBACK reach_helper_enum_windows_proc(HWND hwnd, LPARAM param)
{
    reach_helper_snapshot_builder *builder =
        reinterpret_cast<reach_helper_snapshot_builder *>(param);
    if (builder == nullptr || builder->response == nullptr)
    {
        return FALSE;
    }

    reach_service_window_snapshot snapshot = reach_helper_inspect_window(hwnd);
    if (!snapshot.include_in_switcher)
    {
        return TRUE;
    }

    if (builder->response->window_count >= REACH_SERVICE_MAX_WINDOWS)
    {
        return FALSE;
    }

    builder->response->windows[builder->response->window_count++] = snapshot;
    return TRUE;
}

static void reach_helper_build_snapshot_response(reach_service_response *response)
{
    if (response == nullptr)
    {
        return;
    }
    reach_helper_snapshot_builder builder = {};
    builder.response = response;
    ++g_metadata_generation;
    EnumWindows(reach_helper_enum_windows_proc, reinterpret_cast<LPARAM>(&builder));
    reach_helper_prune_window_metadata(g_metadata_generation);
}

static void reach_helper_publish_window_state(void)
{
    reach_service_response response = {};
    response.version = reach_service_protocol_version();
    reach_helper_build_snapshot_response(&response);

    reach_service_window_snapshot windows[REACH_SERVICE_MAX_WINDOWS] = {};
    uint32_t window_count = 0;
    for (uint32_t index = 0;
         index < response.window_count && window_count < REACH_SERVICE_MAX_WINDOWS;
         ++index)
    {
        if (response.windows[index].include_in_switcher)
        {
            windows[window_count++] = response.windows[index];
        }
    }

    (void)reach_service_shared_publish_windows(windows, window_count);
    reach_helper_lock_window_state();
    g_window_state.window_count = window_count;
    for (uint32_t index = 0; index < window_count; ++index)
    {
        g_window_state.windows[index] = windows[index];
    }
    for (uint32_t index = window_count; index < REACH_SERVICE_MAX_WINDOWS; ++index)
    {
        g_window_state.windows[index] = {};
    }
    reach_helper_unlock_window_state();
    reach_helper_publish_game_mode();
}

static reach_helper_window_state reach_helper_current_window_state(void)
{
    reach_helper_window_state state = {};
    reach_helper_lock_window_state();
    state = g_window_state;
    reach_helper_unlock_window_state();
    return state;
}

static void reach_helper_publish_cached_window_state(const reach_helper_window_state *state)
{
    if (state == nullptr)
    {
        return;
    }
    (void)reach_service_shared_publish_windows(state->windows, state->window_count);
    reach_helper_lock_window_state();
    g_window_state = *state;
    reach_helper_unlock_window_state();
}

static int32_t reach_helper_publish_foreground_change(void)
{
    reach_helper_window_state state = reach_helper_current_window_state();
    if (state.window_count == 0)
    {
        return 0;
    }

    int32_t changed = 0;
    for (uint32_t index = 0; index < state.window_count; ++index)
    {
        HWND hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(state.windows[index].window));
        if (hwnd == nullptr || !IsWindow(hwnd))
        {
            return 0;
        }
        int32_t focused = reach_helper_window_has_foreground(hwnd);
        if (state.windows[index].focused != focused)
        {
            state.windows[index].focused = focused;
            changed = 1;
        }
    }

    if (changed)
    {
        reach_helper_publish_cached_window_state(&state);
    }
    reach_helper_publish_game_mode();
    return 1;
}

static int32_t reach_helper_publish_foreground_placement_change(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd) || !reach_helper_window_has_foreground(hwnd))
    {
        return 1;
    }

    reach_helper_window_state state = reach_helper_current_window_state();
    uint64_t window_id = reinterpret_cast<uint64_t>(hwnd);
    for (uint32_t index = 0; index < state.window_count; ++index)
    {
        if (state.windows[index].window != window_id)
        {
            continue;
        }

        int32_t maximized = IsZoomed(hwnd) ? 1 : 0;
        if (state.windows[index].maximized != maximized)
        {
            state.windows[index].maximized = maximized;
            reach_helper_publish_cached_window_state(&state);
            reach_helper_publish_game_mode();
        }
        return 1;
    }

    return 0;
}

static int32_t reach_helper_publish_name_change(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return 0;
    }

    reach_helper_window_state state = reach_helper_current_window_state();
    uint32_t target_index = REACH_SERVICE_MAX_WINDOWS;
    uint64_t window_id = reinterpret_cast<uint64_t>(hwnd);
    for (uint32_t index = 0; index < state.window_count; ++index)
    {
        if (state.windows[index].window == window_id)
        {
            target_index = index;
            break;
        }
    }

    if (target_index == REACH_SERVICE_MAX_WINDOWS)
    {
        return 0;
    }

    reach_service_window_snapshot updated = reach_helper_inspect_window(hwnd);
    if (!updated.include_in_switcher)
    {
        return 0;
    }

    state.windows[target_index] = updated;
    reach_helper_publish_cached_window_state(&state);
    reach_helper_publish_game_mode();
    return 1;
}

static void reach_helper_close_window_event_hooks(void)
{
    HWINEVENTHOOK *hooks[] = {
        &g_session.create_hook,     &g_session.destroy_hook,        &g_session.show_hook,
        &g_session.hide_hook,       &g_session.minimize_start_hook, &g_session.minimize_end_hook,
        &g_session.foreground_hook, &g_session.location_hook,       &g_session.name_hook,
    };
    for (HWINEVENTHOOK *hook : hooks)
    {
        if (*hook != nullptr)
        {
            UnhookWinEvent(*hook);
            *hook = nullptr;
        }
    }
}

static void CALLBACK reach_helper_window_event_proc(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
                                                    LONG object_id, LONG child_id,
                                                    DWORD event_thread, DWORD event_time)
{
    (void)hook;
    (void)event_thread;
    (void)event_time;

    if (object_id == OBJID_WINDOW && child_id == CHILDID_SELF)
    {
        if (event == EVENT_OBJECT_DESTROY)
        {
            reach_helper_forget_window_metadata(hwnd);
        }
        if (event == EVENT_SYSTEM_MINIMIZESTART)
        {
            reach_window_management_prepare_minimize(hwnd);
        }

        if (event == EVENT_SYSTEM_FOREGROUND && reach_helper_publish_foreground_change())
        {
            return;
        }
        if (event == EVENT_OBJECT_LOCATIONCHANGE &&
            reach_helper_publish_foreground_placement_change(hwnd))
        {
            return;
        }
        if (event == EVENT_OBJECT_NAMECHANGE && reach_helper_publish_name_change(hwnd))
        {
            return;
        }
        reach_helper_publish_window_state();
    }
}

static void reach_helper_minimize_game(HWND hwnd)
{
    reach_helper_publish_window_state();

    if (!reach_helper_window_is_game(hwnd))
    {
        return;
    }

    reach_helper_release_game_cursor_clip(hwnd);
    (void)reach_window_management_minimize(hwnd);
    reach_helper_release_game_cursor_clip(hwnd);
    reach_helper_publish_window_state();
}

static DWORD WINAPI reach_helper_window_event_thread(void *param)
{
    (void)param;
    MSG message = {};
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    g_session.window_event_thread_id = GetCurrentThreadId();

    const DWORD flags = WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS;
    g_session.create_hook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_CREATE, nullptr,
                                            reach_helper_window_event_proc, 0, 0, flags);
    g_session.destroy_hook = SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, nullptr,
                                             reach_helper_window_event_proc, 0, 0, flags);
    g_session.show_hook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, nullptr,
                                          reach_helper_window_event_proc, 0, 0, flags);
    g_session.hide_hook = SetWinEventHook(EVENT_OBJECT_HIDE, EVENT_OBJECT_HIDE, nullptr,
                                          reach_helper_window_event_proc, 0, 0, flags);
    g_session.minimize_start_hook =
        SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZESTART, nullptr,
                        reach_helper_window_event_proc, 0, 0, flags);
    g_session.minimize_end_hook =
        SetWinEventHook(EVENT_SYSTEM_MINIMIZEEND, EVENT_SYSTEM_MINIMIZEEND, nullptr,
                        reach_helper_window_event_proc, 0, 0, flags);
    g_session.foreground_hook =
        SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
                        reach_helper_window_event_proc, 0, 0, flags);
    g_session.location_hook =
        SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr,
                        reach_helper_window_event_proc, 0, 0, flags);
    g_session.name_hook = SetWinEventHook(EVENT_OBJECT_NAMECHANGE, EVENT_OBJECT_NAMECHANGE, nullptr,
                                          reach_helper_window_event_proc, 0, 0, flags);

    reach_helper_publish_window_state();

    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (message.message == REACH_HELPER_WM_MINIMIZE_GAME)
        {
            HWND hwnd = reinterpret_cast<HWND>(message.wParam);
            reach_helper_minimize_game(hwnd);
        }
    }

    reach_helper_close_window_event_hooks();
    g_session.window_event_thread_id = 0;
    return 0;
}

static void reach_helper_start_window_events(void)
{
    if (g_session.window_event_thread == nullptr)
    {
        g_session.window_event_thread =
            CreateThread(nullptr, 0, reach_helper_window_event_thread, nullptr, 0, nullptr);
        for (int attempt = 0; g_session.window_event_thread != nullptr &&
                              g_session.window_event_thread_id == 0 && attempt < 20;
             ++attempt)
        {
            Sleep(10);
        }
    }
}

static void reach_helper_stop_window_events(void)
{
    if (g_session.window_event_thread != nullptr)
    {
        if (g_session.window_event_thread_id != 0)
        {
            PostThreadMessageW(g_session.window_event_thread_id, WM_QUIT, 0, 0);
        }
        WaitForSingleObject(g_session.window_event_thread, 1000);
        CloseHandle(g_session.window_event_thread);
        g_session.window_event_thread = nullptr;
    }
    reach_helper_close_window_event_hooks();
    g_session.window_event_thread_id = 0;
}

static int32_t reach_helper_validate_window_identity(const reach_service_request *request,
                                                     HWND hwnd)
{
    if (request == nullptr || hwnd == nullptr || !IsWindow(hwnd))
    {
        return 0;
    }

    DWORD process_id = 0;
    DWORD thread_id = GetWindowThreadProcessId(hwnd, &process_id);
    if (request->process_id != 0 && request->process_id != process_id)
    {
        return 0;
    }
    if (request->thread_id != 0 && request->thread_id != thread_id)
    {
        return 0;
    }
    if (request->class_name[0] != 0)
    {
        wchar_t class_name[128] = {};
        GetClassNameW(hwnd, class_name, 128);
        if (lstrcmpiW(request->class_name, class_name) != 0)
        {
            return 0;
        }
    }

    reach_service_window_snapshot snapshot = reach_helper_inspect_window(hwnd);
    return snapshot.include_in_switcher != 0;
}

static reach_result reach_helper_query_token_user(HANDLE token, std::vector<BYTE> *out_user)
{
    if (token == nullptr || out_user == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD needed = 0;
    (void)GetTokenInformation(token, TokenUser, nullptr, 0, &needed);
    if (needed == 0)
    {
        return REACH_ERROR;
    }

    out_user->resize(needed);
    if (!GetTokenInformation(token, TokenUser, out_user->data(), needed, &needed))
    {
        out_user->clear();
        return REACH_ERROR;
    }

    return REACH_OK;
}

static int32_t reach_helper_same_user_client(HANDLE pipe)
{
    HANDLE process_token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token))
    {
        return 0;
    }

    std::vector<BYTE> process_user;
    reach_result process_result = reach_helper_query_token_user(process_token, &process_user);
    CloseHandle(process_token);
    if (process_result != REACH_OK)
    {
        return 0;
    }

    if (!ImpersonateNamedPipeClient(pipe))
    {
        return 0;
    }

    HANDLE client_token = nullptr;
    BOOL opened_client = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &client_token);
    RevertToSelf();
    if (!opened_client)
    {
        return 0;
    }

    std::vector<BYTE> client_user;
    reach_result client_result = reach_helper_query_token_user(client_token, &client_user);
    CloseHandle(client_token);
    if (client_result != REACH_OK)
    {
        return 0;
    }

    TOKEN_USER *process_token_user = reinterpret_cast<TOKEN_USER *>(process_user.data());
    TOKEN_USER *client_token_user = reinterpret_cast<TOKEN_USER *>(client_user.data());
    if (process_token_user == nullptr || client_token_user == nullptr ||
        process_token_user->User.Sid == nullptr || client_token_user->User.Sid == nullptr)
    {
        return 0;
    }

    return EqualSid(process_token_user->User.Sid, client_token_user->User.Sid);
}

static int32_t reach_helper_post_minimize_game(HWND hwnd)
{
    if (hwnd != nullptr && g_session.window_event_thread_id != 0)
    {
        return PostThreadMessageW(g_session.window_event_thread_id, REACH_HELPER_WM_MINIMIZE_GAME,
                                  reinterpret_cast<WPARAM>(hwnd), 0)
                   ? 1
                   : 0;
    }
    return 0;
}

static int32_t reach_helper_game_mode_active(void)
{
    return InterlockedCompareExchange(&g_game_mode_active, 0, 0) != 0;
}

static reach_result reach_helper_execute(const reach_service_request *request,
                                         reach_service_response *response)
{
    if (!reach_service_request_valid(request))
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (request->command == REACH_SERVICE_COMMAND_PING)
    {
        return REACH_OK;
    }

    HWND hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(request->window));
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (!reach_helper_validate_window_identity(request, hwnd))
    {
        if (response != nullptr)
        {
            response->action_result = REACH_SERVICE_ACTION_STALE_WINDOW;
        }
        return REACH_ERROR;
    }

    reach_result result = REACH_INVALID_ARGUMENT;
    switch (request->command)
    {
    case REACH_SERVICE_COMMAND_ACTIVATE:
        result = reach_window_management_activate(hwnd);
        break;
    case REACH_SERVICE_COMMAND_MINIMIZE:
        result = reach_window_management_minimize(hwnd);
        break;
    case REACH_SERVICE_COMMAND_RESTORE:
        result = reach_window_management_activate(hwnd);
        break;
    case REACH_SERVICE_COMMAND_SNAP:
        result =
            reach_window_management_snap(hwnd, static_cast<reach_split_mode>(request->split_mode));
        break;
    case REACH_SERVICE_COMMAND_CLOSE:
        result = reach_window_management_close(hwnd);
        break;
    default:
        result = REACH_INVALID_ARGUMENT;
        break;
    }

    if (response != nullptr)
    {
        response->action_result = result == REACH_OK ? REACH_SERVICE_ACTION_SUCCEEDED
                                                     : REACH_SERVICE_ACTION_TIMED_OUT;
    }
    if (result == REACH_OK)
    {
        reach_helper_publish_window_state();
    }
    return result;
}

static SECURITY_ATTRIBUTES reach_helper_pipe_security(PSECURITY_DESCRIPTOR *sd)
{
    *sd = nullptr;
    SECURITY_ATTRIBUTES attributes = {};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = FALSE;

    const wchar_t *sddl = L"D:P(A;;GA;;;IU)(A;;GA;;;SY)(A;;GA;;;BA)";
    if (ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, sd, nullptr))
    {
        attributes.lpSecurityDescriptor = *sd;
    }

    return attributes;
}

static HANDLE reach_helper_create_pipe(void)
{
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    SECURITY_ATTRIBUTES security = reach_helper_pipe_security(&descriptor);
    HANDLE pipe = CreateNamedPipeW(
        REACH_SERVICE_PIPE_NAME, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES,
        sizeof(reach_service_response), sizeof(reach_service_request), 1000,
        security.lpSecurityDescriptor != nullptr ? &security : nullptr);

    if (descriptor != nullptr)
    {
        LocalFree(descriptor);
    }

    return pipe;
}

enum reach_helper_pipe_wait_result
{
    REACH_HELPER_PIPE_WAIT_COMPLETED,
    REACH_HELPER_PIPE_WAIT_SHELL_EXITED,
    REACH_HELPER_PIPE_WAIT_FAILED,
};

static reach_helper_pipe_wait_result reach_helper_wait_for_pipe_io(HANDLE shell_mutex, HANDLE pipe,
                                                                   OVERLAPPED *overlapped,
                                                                   DWORD *out_transferred)
{
    HANDLE waits[] = {shell_mutex, overlapped->hEvent};
    DWORD wait = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
    if (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED_0)
    {
        (void)CancelIoEx(pipe, overlapped);
        DWORD transferred = 0;
        (void)GetOverlappedResult(pipe, overlapped, &transferred, TRUE);
        return REACH_HELPER_PIPE_WAIT_SHELL_EXITED;
    }
    if (wait != WAIT_OBJECT_0 + 1)
    {
        (void)CancelIoEx(pipe, overlapped);
        return REACH_HELPER_PIPE_WAIT_FAILED;
    }

    DWORD transferred = 0;
    if (!GetOverlappedResult(pipe, overlapped, &transferred, FALSE))
    {
        return REACH_HELPER_PIPE_WAIT_FAILED;
    }
    *out_transferred = transferred;
    return REACH_HELPER_PIPE_WAIT_COMPLETED;
}

static reach_helper_pipe_wait_result
reach_helper_read_request(HANDLE shell_mutex, HANDLE pipe, OVERLAPPED *overlapped,
                          reach_service_request *request)
{
    ResetEvent(overlapped->hEvent);
    DWORD read = 0;
    if (ReadFile(pipe, request, sizeof(*request), &read, overlapped))
    {
        return read == sizeof(*request) ? REACH_HELPER_PIPE_WAIT_COMPLETED
                                        : REACH_HELPER_PIPE_WAIT_FAILED;
    }
    if (GetLastError() != ERROR_IO_PENDING)
    {
        return REACH_HELPER_PIPE_WAIT_FAILED;
    }

    reach_helper_pipe_wait_result wait =
        reach_helper_wait_for_pipe_io(shell_mutex, pipe, overlapped, &read);
    return wait == REACH_HELPER_PIPE_WAIT_COMPLETED && read != sizeof(*request)
               ? REACH_HELPER_PIPE_WAIT_FAILED
               : wait;
}

static reach_helper_pipe_wait_result
reach_helper_write_response(HANDLE shell_mutex, HANDLE pipe, OVERLAPPED *overlapped,
                            const reach_service_response *response)
{
    ResetEvent(overlapped->hEvent);
    DWORD written = 0;
    if (WriteFile(pipe, response, sizeof(*response), &written, overlapped))
    {
        return written == sizeof(*response) ? REACH_HELPER_PIPE_WAIT_COMPLETED
                                            : REACH_HELPER_PIPE_WAIT_FAILED;
    }
    if (GetLastError() != ERROR_IO_PENDING)
    {
        return REACH_HELPER_PIPE_WAIT_FAILED;
    }

    reach_helper_pipe_wait_result wait =
        reach_helper_wait_for_pipe_io(shell_mutex, pipe, overlapped, &written);
    return wait == REACH_HELPER_PIPE_WAIT_COMPLETED && written != sizeof(*response)
               ? REACH_HELPER_PIPE_WAIT_FAILED
               : wait;
}

static reach_helper_pipe_wait_result reach_helper_serve_client(HANDLE shell_mutex, HANDLE pipe,
                                                               OVERLAPPED *overlapped)
{
    reach_service_request request = {};
    reach_helper_pipe_wait_result read =
        reach_helper_read_request(shell_mutex, pipe, overlapped, &request);
    if (read != REACH_HELPER_PIPE_WAIT_COMPLETED)
    {
        return read;
    }

    reach_service_response response = {};
    response.version = reach_service_protocol_version();
    reach_result result = REACH_ERROR;
    if (reach_helper_same_user_client(pipe))
    {
        result = reach_helper_execute(&request, &response);
    }
    response.result = static_cast<int32_t>(result);
    return reach_helper_write_response(shell_mutex, pipe, overlapped, &response);
}

static void reach_helper_hide_minimized_window_icons(void)
{
    MINIMIZEDMETRICS metrics = {};
    metrics.cbSize = sizeof(metrics);
    if (!SystemParametersInfoW(SPI_GETMINIMIZEDMETRICS, sizeof(metrics), &metrics, 0))
    {
        return;
    }

    if ((metrics.iArrange & ARW_HIDE) != 0)
    {
        return;
    }

    metrics.iArrange |= ARW_HIDE;
    (void)SystemParametersInfoW(SPI_SETMINIMIZEDMETRICS, sizeof(metrics), &metrics,
                                SPIF_SENDCHANGE);
}

static int reach_helper_run_pipe_server(HANDLE shell_mutex)
{
    HANDLE pipe_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (pipe_event == nullptr)
    {
        return 1;
    }

    for (;;)
    {
        DWORD shell_wait = WaitForSingleObject(shell_mutex, 0);
        if (shell_wait == WAIT_OBJECT_0 || shell_wait == WAIT_ABANDONED)
        {
            CloseHandle(pipe_event);
            return 0;
        }

        HANDLE pipe = reach_helper_create_pipe();
        if (pipe == INVALID_HANDLE_VALUE)
        {
            CloseHandle(pipe_event);
            return 1;
        }

        OVERLAPPED overlapped = {};
        overlapped.hEvent = pipe_event;
        ResetEvent(pipe_event);

        reach_helper_pipe_wait_result connection = REACH_HELPER_PIPE_WAIT_FAILED;
        if (ConnectNamedPipe(pipe, &overlapped))
        {
            connection = REACH_HELPER_PIPE_WAIT_COMPLETED;
        }
        else if (GetLastError() == ERROR_IO_PENDING)
        {
            DWORD transferred = 0;
            connection =
                reach_helper_wait_for_pipe_io(shell_mutex, pipe, &overlapped, &transferred);
        }
        else if (GetLastError() == ERROR_PIPE_CONNECTED)
        {
            connection = REACH_HELPER_PIPE_WAIT_COMPLETED;
        }

        reach_helper_pipe_wait_result served = connection;
        if (connection == REACH_HELPER_PIPE_WAIT_COMPLETED)
        {
            served = reach_helper_serve_client(shell_mutex, pipe, &overlapped);
            (void)DisconnectNamedPipe(pipe);
        }
        CloseHandle(pipe);

        if (connection == REACH_HELPER_PIPE_WAIT_SHELL_EXITED ||
            served == REACH_HELPER_PIPE_WAIT_SHELL_EXITED)
        {
            CloseHandle(pipe_event);
            return 0;
        }
        if (connection == REACH_HELPER_PIPE_WAIT_FAILED)
        {
            CloseHandle(pipe_event);
            return 1;
        }
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show_command)
{
    (void)instance;
    (void)previous;
    (void)command_line;
    (void)show_command;

    HANDLE shell_mutex = OpenMutexW(SYNCHRONIZE, FALSE, REACH_SHELL_INSTANCE_MUTEX);
    if (shell_mutex == nullptr)
    {
        return 0;
    }

    HANDLE instance_mutex = CreateMutexW(nullptr, TRUE, REACH_HELPER_INSTANCE_MUTEX);
    if (instance_mutex == nullptr || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (instance_mutex != nullptr)
        {
            CloseHandle(instance_mutex);
        }
        CloseHandle(shell_mutex);
        return 0;
    }

    reach_helper_hide_minimized_window_icons();

    if (reach_service_shared_writer_start() != REACH_OK)
    {
        CloseHandle(instance_mutex);
        CloseHandle(shell_mutex);
        return 1;
    }
    reach_helper_hotkey_callbacks hotkey_callbacks = {};
    hotkey_callbacks.game_mode_active = reach_helper_game_mode_active;
    hotkey_callbacks.minimize_game = reach_helper_post_minimize_game;
    reach_helper_hotkeys_configure(&hotkey_callbacks);

    reach_helper_publish_window_state();
    reach_helper_start_window_events();
    (void)reach_helper_start_hotkeys();

    int exit_code = reach_helper_run_pipe_server(shell_mutex);
    reach_helper_stop_hotkeys();
    reach_helper_stop_window_events();
    reach_service_shared_writer_stop();
    CloseHandle(instance_mutex);
    CloseHandle(shell_mutex);
    return exit_code;
}
