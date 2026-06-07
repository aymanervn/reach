#include "../../adapters/windows/window_management/elevation_helper_protocol.h"
#include "../../adapters/windows/window_management/elevation_helper_shared_state_win32.h"
#include "../../adapters/windows/window_management/window_query_win32.h"
#include "window_actions.h"

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
    HWINEVENTHOOK name_hook;
};

struct reach_helper_hotkey_state
{
    HHOOK hook;
    HANDLE thread;
    DWORD thread_id;
    int32_t alt_down;
    int32_t shift_down;
    int32_t alt_tab_active;
    int32_t left_win_down;
    int32_t right_win_down;
    int32_t windows_key_chord;
};

static reach_helper_session_state g_session;
static reach_helper_hotkey_state g_hotkeys;
static LONG g_game_mode_active;
static const wchar_t *REACH_HELPER_INSTANCE_MUTEX = L"Local\\ReachElevationHelperInstance";
static const UINT REACH_HELPER_WM_MINIMIZE_GAME = WM_APP + 41;

static reach_result reach_helper_execute(const reach_elevation_helper_request *request,
                                         reach_elevation_helper_response *response);
static void reach_helper_clear_reach_hotkey_state(void);
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

static void reach_helper_classify_window(reach_elevation_helper_window_snapshot *snapshot)
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

    snapshot->kind = REACH_ELEVATION_HELPER_WINDOW_UNKNOWN;
    snapshot->include_in_switcher = 0;

    HWND hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(snapshot->window));
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        snapshot->kind = REACH_ELEVATION_HELPER_WINDOW_UNKNOWN;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"not a window");
        return;
    }

    if (hwnd == GetShellWindow() || reach_helper_is_reach_window_class(snapshot->class_name))
    {
        snapshot->kind = REACH_ELEVATION_HELPER_WINDOW_SHELL;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"shell/reach surface");
        return;
    }

    if (snapshot->cloaked)
    {
        snapshot->kind = REACH_ELEVATION_HELPER_WINDOW_SYSTEM;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"cloaked window");
        return;
    }

    if (GetAncestor(hwnd, GA_ROOT) != hwnd)
    {
        snapshot->kind = REACH_ELEVATION_HELPER_WINDOW_HELPER;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"not root top-level window");
        return;
    }

    LONG_PTR ex_style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((ex_style & WS_EX_TOOLWINDOW) != 0)
    {
        snapshot->kind = REACH_ELEVATION_HELPER_WINDOW_HELPER;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"tool window");
        return;
    }

    if (reach_helper_string_in_list(snapshot->class_name, helper_classes,
                                    sizeof(helper_classes) / sizeof(helper_classes[0])) ||
        reach_helper_string_has_prefix(snapshot->class_name, helper_class_prefixes,
                                       sizeof(helper_class_prefixes) /
                                           sizeof(helper_class_prefixes[0])))
    {
        snapshot->kind = REACH_ELEVATION_HELPER_WINDOW_HELPER;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"known helper class");
        return;
    }

    if (reach_helper_string_in_list(snapshot->title, helper_titles,
                                    sizeof(helper_titles) / sizeof(helper_titles[0])) ||
        reach_helper_string_has_prefix(snapshot->title, helper_title_prefixes,
                                       sizeof(helper_title_prefixes) /
                                           sizeof(helper_title_prefixes[0])))
    {
        snapshot->kind = REACH_ELEVATION_HELPER_WINDOW_HELPER;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"known helper title");
        return;
    }

    if (snapshot->iconic)
    {
        snapshot->kind = REACH_ELEVATION_HELPER_WINDOW_MINIMIZED_APP;
        snapshot->include_in_switcher = 1;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"accepted minimized app");
        return;
    }

    if (snapshot->title[0] == 0)
    {
        snapshot->kind = REACH_ELEVATION_HELPER_WINDOW_HELPER;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"empty title");
        return;
    }

    if (!snapshot->visible && !snapshot->iconic)
    {
        snapshot->kind = REACH_ELEVATION_HELPER_WINDOW_HELPER;
        reach_helper_copy_wide(snapshot->classification_reason, 160, L"hidden non-minimized");
        return;
    }

    snapshot->kind = REACH_ELEVATION_HELPER_WINDOW_APP;
    snapshot->include_in_switcher = 1;
    reach_helper_copy_wide(snapshot->classification_reason, 160, L"accepted visible app");
}

static reach_elevation_helper_window_snapshot reach_helper_inspect_window(HWND hwnd)
{
    reach_elevation_helper_window_snapshot snapshot = {};
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
    GetWindowTextW(hwnd, snapshot.title, 260);
    GetClassNameW(hwnd, snapshot.class_name, 128);
    reach_helper_process_path(snapshot.process_id, snapshot.process_path, 260);
    if (!reach_window_app_user_model_id(
            hwnd, reinterpret_cast<uint16_t *>(snapshot.app_user_model_id), 260))
    {
        (void)reach_window_process_app_user_model_id_for_process(
            snapshot.process_id, reinterpret_cast<uint16_t *>(snapshot.app_user_model_id), 260);
    }
    reach_helper_process_integrity_text(snapshot.process_id, snapshot.integrity, 32);
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

static int32_t reach_helper_detect_game_mode(void)
{
    return reach_helper_window_is_game(GetForegroundWindow());
}

static void reach_helper_publish_game_mode(void)
{
    int32_t active = reach_helper_detect_game_mode();
    if (active)
    {
        reach_helper_clear_reach_hotkey_state();
    }
    InterlockedExchange(&g_game_mode_active, active ? 1 : 0);
    (void)reach_elevation_helper_shared_publish_game_mode(active);
}

struct reach_helper_snapshot_builder
{
    reach_elevation_helper_response *response;
};
static BOOL CALLBACK reach_helper_enum_windows_proc(HWND hwnd, LPARAM param)
{
    reach_helper_snapshot_builder *builder =
        reinterpret_cast<reach_helper_snapshot_builder *>(param);
    if (builder == nullptr || builder->response == nullptr)
    {
        return FALSE;
    }

    reach_elevation_helper_window_snapshot snapshot = reach_helper_inspect_window(hwnd);
    if (!snapshot.include_in_switcher)
    {
        return TRUE;
    }

    if (builder->response->window_count >= REACH_ELEVATION_HELPER_MAX_WINDOWS)
    {
        return FALSE;
    }

    builder->response->windows[builder->response->window_count++] = snapshot;
    return TRUE;
}

static void reach_helper_build_snapshot_response(reach_elevation_helper_response *response)
{
    if (response == nullptr)
    {
        return;
    }
    reach_helper_snapshot_builder builder = {};
    builder.response = response;
    EnumWindows(reach_helper_enum_windows_proc, reinterpret_cast<LPARAM>(&builder));
}

static void reach_helper_publish_window_state(void)
{
    reach_elevation_helper_response response = {};
    response.version = reach_elevation_helper_protocol_version();
    reach_helper_build_snapshot_response(&response);

    reach_elevation_helper_window_snapshot windows[REACH_ELEVATION_HELPER_MAX_WINDOWS] = {};
    uint32_t window_count = 0;
    for (uint32_t index = 0;
         index < response.window_count && window_count < REACH_ELEVATION_HELPER_MAX_WINDOWS;
         ++index)
    {
        if (response.windows[index].include_in_switcher)
        {
            windows[window_count++] = response.windows[index];
        }
    }

    (void)reach_elevation_helper_shared_publish_windows(windows, window_count);
    reach_helper_publish_game_mode();
}

static void reach_helper_close_window_event_hooks(void)
{
    HWINEVENTHOOK *hooks[] = {
        &g_session.create_hook,     &g_session.destroy_hook,        &g_session.show_hook,
        &g_session.hide_hook,       &g_session.minimize_start_hook, &g_session.minimize_end_hook,
        &g_session.foreground_hook, &g_session.name_hook,
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
    (void)event;
    (void)hwnd;
    (void)object_id;
    (void)child_id;
    (void)event_thread;
    (void)event_time;

    if (object_id == OBJID_WINDOW && child_id == CHILDID_SELF)
    {
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

    (void)reach_window_management_minimize(hwnd);
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

static int32_t reach_helper_validate_window_identity(const reach_elevation_helper_request *request,
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

    reach_elevation_helper_window_snapshot snapshot = reach_helper_inspect_window(hwnd);
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

static uint32_t reach_helper_hotkey_modifiers(void)
{
    uint32_t modifiers = 0;
    if (g_hotkeys.alt_down)
    {
        modifiers |= REACH_ELEVATION_HELPER_MODIFIER_ALT;
    }
    if (g_hotkeys.shift_down)
    {
        modifiers |= REACH_ELEVATION_HELPER_MODIFIER_SHIFT;
    }
    if (g_hotkeys.left_win_down)
    {
        modifiers |= REACH_ELEVATION_HELPER_MODIFIER_LEFT_WIN;
    }
    if (g_hotkeys.right_win_down)
    {
        modifiers |= REACH_ELEVATION_HELPER_MODIFIER_RIGHT_WIN;
    }
    return modifiers;
}

static uint32_t reach_helper_hotkey_key_from_vk(DWORD vk)
{
    switch (vk)
    {
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
        return REACH_ELEVATION_HELPER_HOTKEY_ALT;
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
        return REACH_ELEVATION_HELPER_HOTKEY_SHIFT;
    case VK_TAB:
        return REACH_ELEVATION_HELPER_HOTKEY_TAB;
    case VK_ESCAPE:
        return REACH_ELEVATION_HELPER_HOTKEY_ESCAPE;
    case VK_LWIN:
        return REACH_ELEVATION_HELPER_HOTKEY_LEFT_WIN;
    case VK_RWIN:
        return REACH_ELEVATION_HELPER_HOTKEY_RIGHT_WIN;
    default:
        return 0;
    }
}

static void reach_helper_update_key_state(uint32_t key, int32_t pressed)
{
    switch (key)
    {
    case REACH_ELEVATION_HELPER_HOTKEY_ALT:
        g_hotkeys.alt_down = pressed;
        break;
    case REACH_ELEVATION_HELPER_HOTKEY_SHIFT:
        g_hotkeys.shift_down = pressed;
        break;
    case REACH_ELEVATION_HELPER_HOTKEY_LEFT_WIN:
        g_hotkeys.left_win_down = pressed;
        break;
    case REACH_ELEVATION_HELPER_HOTKEY_RIGHT_WIN:
        g_hotkeys.right_win_down = pressed;
        break;
    default:
        break;
    }
}

static int32_t reach_helper_virtual_key_down(int virtual_key)
{
    return (GetAsyncKeyState(virtual_key) & 0x8000) != 0;
}

static void reach_helper_reconcile_modifier_state(void)
{
    g_hotkeys.alt_down = reach_helper_virtual_key_down(VK_MENU) ||
                         reach_helper_virtual_key_down(VK_LMENU) ||
                         reach_helper_virtual_key_down(VK_RMENU);
    g_hotkeys.shift_down = reach_helper_virtual_key_down(VK_SHIFT) ||
                           reach_helper_virtual_key_down(VK_LSHIFT) ||
                           reach_helper_virtual_key_down(VK_RSHIFT);
    g_hotkeys.left_win_down = reach_helper_virtual_key_down(VK_LWIN);
    g_hotkeys.right_win_down = reach_helper_virtual_key_down(VK_RWIN);
    if (!g_hotkeys.left_win_down && !g_hotkeys.right_win_down)
    {
        g_hotkeys.windows_key_chord = 0;
    }
}

static void reach_helper_clear_reach_hotkey_state(void)
{
    g_hotkeys.alt_down = 0;
    g_hotkeys.shift_down = 0;
    g_hotkeys.alt_tab_active = 0;
    g_hotkeys.left_win_down = 0;
    g_hotkeys.right_win_down = 0;
    g_hotkeys.windows_key_chord = 0;
}

static int32_t reach_helper_alt_down(void)
{
    return reach_helper_virtual_key_down(VK_MENU) || reach_helper_virtual_key_down(VK_LMENU) ||
           reach_helper_virtual_key_down(VK_RMENU);
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

static int32_t reach_helper_hotkey_is_modifier(uint32_t key)
{
    return key == REACH_ELEVATION_HELPER_HOTKEY_ALT || key == REACH_ELEVATION_HELPER_HOTKEY_SHIFT ||
           key == REACH_ELEVATION_HELPER_HOTKEY_LEFT_WIN ||
           key == REACH_ELEVATION_HELPER_HOTKEY_RIGHT_WIN;
}

static void reach_helper_stop_hotkeys(void);

static LRESULT CALLBACK reach_helper_keyboard_proc(int code, WPARAM wparam, LPARAM lparam)
{
    const int32_t game_mode_active = InterlockedCompareExchange(&g_game_mode_active, 0, 0) != 0;
    const bool key_down = wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN;
    const bool key_up = wparam == WM_KEYUP || wparam == WM_SYSKEYUP;
    uint32_t key = 0;

    if (code == HC_ACTION)
    {
        const KBDLLHOOKSTRUCT *keyboard = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lparam);
        if (keyboard != nullptr && (keyboard->flags & LLKHF_INJECTED) == 0)
        {
            key = reach_helper_hotkey_key_from_vk(keyboard->vkCode);
            if (game_mode_active)
            {
                if (key == REACH_ELEVATION_HELPER_HOTKEY_TAB && key_down && reach_helper_alt_down())
                {
                    HWND game = GetForegroundWindow();
                    reach_helper_clear_reach_hotkey_state();
                    if (reach_helper_post_minimize_game(game))
                    {
                        return 1;
                    }
                }
                return CallNextHookEx(g_hotkeys.hook, code, wparam, lparam);
            }
            if (key == 0 && key_down && (g_hotkeys.left_win_down || g_hotkeys.right_win_down))
            {
                g_hotkeys.windows_key_chord = 1;
            }
            if (key != 0 && (key_down || key_up))
            {
                int32_t windows_chord = g_hotkeys.windows_key_chord;
                if (reach_helper_hotkey_is_modifier(key))
                {
                    reach_helper_update_key_state(key, key_down ? 1 : 0);
                }
                else
                {
                    reach_helper_reconcile_modifier_state();
                }
                if ((key == REACH_ELEVATION_HELPER_HOTKEY_LEFT_WIN ||
                     key == REACH_ELEVATION_HELPER_HOTKEY_RIGHT_WIN) &&
                    g_hotkeys.alt_tab_active)
                {
                    g_hotkeys.windows_key_chord = 0;
                    return 1;
                }
                uint32_t modifiers = reach_helper_hotkey_modifiers();
                if ((key == REACH_ELEVATION_HELPER_HOTKEY_LEFT_WIN ||
                     key == REACH_ELEVATION_HELPER_HOTKEY_RIGHT_WIN) &&
                    key_up && windows_chord)
                {
                    modifiers |= REACH_ELEVATION_HELPER_MODIFIER_CHORD;
                }
                uint32_t action = key_down ? REACH_ELEVATION_HELPER_HOTKEY_PRESSED
                                           : REACH_ELEVATION_HELPER_HOTKEY_RELEASED;
                (void)reach_elevation_helper_shared_append_hotkey(key, action, modifiers);

                if (key == REACH_ELEVATION_HELPER_HOTKEY_TAB && key_down && g_hotkeys.alt_down)
                {
                    g_hotkeys.alt_tab_active = 1;
                    return 1;
                }
                if (key == REACH_ELEVATION_HELPER_HOTKEY_ESCAPE && key_down &&
                    g_hotkeys.alt_tab_active)
                {
                    g_hotkeys.alt_tab_active = 0;
                    return 1;
                }
                if (key == REACH_ELEVATION_HELPER_HOTKEY_ALT && key_up && g_hotkeys.alt_tab_active)
                {
                    g_hotkeys.alt_tab_active = 0;
                }
                if (key == REACH_ELEVATION_HELPER_HOTKEY_LEFT_WIN ||
                    key == REACH_ELEVATION_HELPER_HOTKEY_RIGHT_WIN)
                {
                    if (key_up && !g_hotkeys.left_win_down && !g_hotkeys.right_win_down)
                    {
                        g_hotkeys.windows_key_chord = 0;
                    }
                    return 1;
                }
                if ((g_hotkeys.left_win_down || g_hotkeys.right_win_down) && key_down)
                {
                    g_hotkeys.windows_key_chord = 1;
                }
            }
        }
    }

    return CallNextHookEx(g_hotkeys.hook, code, wparam, lparam);
}

static DWORD WINAPI reach_helper_hotkey_thread(void *param)
{
    (void)param;
    MSG message = {};
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    g_hotkeys.thread_id = GetCurrentThreadId();
    g_hotkeys.hook =
        SetWindowsHookExW(WH_KEYBOARD_LL, reach_helper_keyboard_proc, GetModuleHandleW(nullptr), 0);
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
    }
    if (g_hotkeys.hook != nullptr)
    {
        UnhookWindowsHookEx(g_hotkeys.hook);
        g_hotkeys.hook = nullptr;
    }
    return 0;
}

static reach_result reach_helper_start_hotkeys(void)
{
    if (g_hotkeys.thread != nullptr)
    {
        return REACH_OK;
    }

    g_hotkeys.thread = CreateThread(nullptr, 0, reach_helper_hotkey_thread, nullptr, 0, nullptr);
    for (int attempt = 0; g_hotkeys.thread != nullptr &&
                          (g_hotkeys.thread_id == 0 || g_hotkeys.hook == nullptr) && attempt < 20;
         ++attempt)
    {
        Sleep(10);
    }

    if (g_hotkeys.thread == nullptr || g_hotkeys.hook == nullptr)
    {
        reach_helper_stop_hotkeys();
        return REACH_ERROR;
    }

    return REACH_OK;
}

static void reach_helper_stop_hotkeys(void)
{
    if (g_hotkeys.thread != nullptr)
    {
        if (g_hotkeys.thread_id != 0)
        {
            PostThreadMessageW(g_hotkeys.thread_id, WM_QUIT, 0, 0);
        }
        WaitForSingleObject(g_hotkeys.thread, 1000);
        CloseHandle(g_hotkeys.thread);
        g_hotkeys.thread = nullptr;
    }

    g_hotkeys = {};
}

static reach_result reach_helper_execute(const reach_elevation_helper_request *request,
                                         reach_elevation_helper_response *response)
{
    if (!reach_elevation_helper_request_valid(request))
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (request->command == REACH_ELEVATION_HELPER_COMMAND_PING)
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
            response->action_result = REACH_ELEVATION_HELPER_ACTION_STALE_WINDOW;
        }
        return REACH_ERROR;
    }

    reach_result result = REACH_INVALID_ARGUMENT;
    switch (request->command)
    {
    case REACH_ELEVATION_HELPER_COMMAND_ACTIVATE:
        result = reach_window_management_activate(hwnd);
        break;
    case REACH_ELEVATION_HELPER_COMMAND_MINIMIZE:
        result = reach_window_management_minimize(hwnd);
        break;
    case REACH_ELEVATION_HELPER_COMMAND_RESTORE:
        result = reach_window_management_activate(hwnd);
        break;
    case REACH_ELEVATION_HELPER_COMMAND_SNAP:
        result =
            reach_window_management_snap(hwnd, static_cast<reach_split_mode>(request->split_mode));
        break;
    case REACH_ELEVATION_HELPER_COMMAND_CLOSE:
        result = reach_window_management_close(hwnd);
        break;
    default:
        result = REACH_INVALID_ARGUMENT;
        break;
    }

    if (response != nullptr)
    {
        response->action_result = result == REACH_OK ? REACH_ELEVATION_HELPER_ACTION_SUCCEEDED
                                                     : REACH_ELEVATION_HELPER_ACTION_TIMED_OUT;
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
        REACH_ELEVATION_HELPER_PIPE_NAME, PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES,
        sizeof(reach_elevation_helper_response), sizeof(reach_elevation_helper_request), 1000,
        security.lpSecurityDescriptor != nullptr ? &security : nullptr);

    if (descriptor != nullptr)
    {
        LocalFree(descriptor);
    }

    return pipe;
}

static void reach_helper_serve_client(HANDLE pipe)
{
    reach_elevation_helper_request request = {};
    DWORD read = 0;
    reach_result result = REACH_ERROR;

    if (ReadFile(pipe, &request, sizeof(request), &read, nullptr) && read == sizeof(request))
    {
        if (reach_helper_same_user_client(pipe))
        {
            reach_elevation_helper_response response = {};
            response.version = reach_elevation_helper_protocol_version();
            result = reach_helper_execute(&request, &response);
            response.result = static_cast<int32_t>(result);

            DWORD written = 0;
            (void)WriteFile(pipe, &response, sizeof(response), &written, nullptr);
            FlushFileBuffers(pipe);
            return;
        }
        else
        {
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

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show_command)
{
    (void)instance;
    (void)previous;
    (void)command_line;
    (void)show_command;

    HANDLE instance_mutex = CreateMutexW(nullptr, TRUE, REACH_HELPER_INSTANCE_MUTEX);
    if (instance_mutex == nullptr || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (instance_mutex != nullptr)
        {
            CloseHandle(instance_mutex);
        }
        return 0;
    }

    if (reach_elevation_helper_shared_writer_start() != REACH_OK)
    {
        CloseHandle(instance_mutex);
        return 1;
    }
    reach_helper_publish_window_state();
    reach_helper_start_window_events();
    (void)reach_helper_start_hotkeys();

    for (;;)
    {
        HANDLE pipe = reach_helper_create_pipe();
        if (pipe == INVALID_HANDLE_VALUE)
        {
            Sleep(1000);
            continue;
        }

        BOOL connected =
            ConnectNamedPipe(pipe, nullptr) ? TRUE : GetLastError() == ERROR_PIPE_CONNECTED;
        if (connected)
        {
            reach_helper_serve_client(pipe);
            DisconnectNamedPipe(pipe);
        }

        CloseHandle(pipe);
    }
}
