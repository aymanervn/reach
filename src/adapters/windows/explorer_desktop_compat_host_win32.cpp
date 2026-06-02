#include "windows_adapters_internal.h"

#include <windows.h>

#include <atomic>
#include <vector>
#include <wchar.h>

static HWND g_progmanHwnd = nullptr;
static HWND g_workerwHwnd = nullptr;
static std::vector<HWND> g_hiddenReachWallpaperHwnds;
static HWINEVENTHOOK g_wpeCreateHook = nullptr;
static HWINEVENTHOOK g_wpeDestroyHook = nullptr;
static HANDLE g_hostThread = nullptr;
static HANDLE g_hostReadyEvent = nullptr;
static std::atomic<DWORD> g_hostThreadId{0};
static std::atomic<int32_t> g_hostCreateResult{REACH_ERROR};
static int32_t g_syncScheduled = 0;
static std::atomic<int32_t> g_wallpaperEngineRendererPresent{-1};

static const UINT_PTR REACH_EXPLORER_DESKTOP_COMPAT_SYNC_TIMER = 0x381;
static const UINT REACH_EXPLORER_DESKTOP_COMPAT_SYNC_DELAY_MS = 33;
static const UINT REACH_EXPLORER_DESKTOP_COMPAT_REQUEST_SYNC = WM_APP + 0x321;
static const UINT REACH_EXPLORER_DESKTOP_COMPAT_SHUTDOWN = WM_APP + 0x322;

extern "C" void reach_windows_notify_desktop_environment_changed(void);
extern "C" void reach_windows_request_desktop_environment_sync(void);

static const wchar_t *reach_explorer_desktop_compat_progman_class(void)
{
    return L"Progman";
}

static const wchar_t *reach_explorer_desktop_compat_workerw_class(void)
{
    return L"WorkerW";
}

static const wchar_t *reach_explorer_desktop_compat_wallpaper_visible_property(void)
{
    return L"ReachWallpaperIntendedVisible";
}

static void reach_explorer_desktop_compat_schedule_sync(UINT delay_ms)
{
    if (g_progmanHwnd != nullptr && IsWindow(g_progmanHwnd))
    {
        if (g_syncScheduled)
        {
            return;
        }

        if (SetTimer(g_progmanHwnd, REACH_EXPLORER_DESKTOP_COMPAT_SYNC_TIMER, delay_ms, nullptr) !=
            0)
        {
            g_syncScheduled = 1;
        }
    }
}

static LRESULT CALLBACK reach_explorer_desktop_compat_host_proc(HWND hwnd, UINT message,
                                                                WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case REACH_EXPLORER_DESKTOP_COMPAT_REQUEST_SYNC:
        reach_explorer_desktop_compat_schedule_sync(REACH_EXPLORER_DESKTOP_COMPAT_SYNC_DELAY_MS);
        return 0;

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_NCHITTEST:
        return HTTRANSPARENT;

    case WM_PARENTNOTIFY:
        if (LOWORD(wparam) == WM_CREATE || LOWORD(wparam) == WM_DESTROY)
        {
            reach_explorer_desktop_compat_schedule_sync(
                REACH_EXPLORER_DESKTOP_COMPAT_SYNC_DELAY_MS);
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);

    case WM_TIMER:
        if (wparam == REACH_EXPLORER_DESKTOP_COMPAT_SYNC_TIMER)
        {
            KillTimer(hwnd, REACH_EXPLORER_DESKTOP_COMPAT_SYNC_TIMER);
            g_syncScheduled = 0;
            reach_windows_notify_desktop_environment_changed();
            return 0;
        }

        return DefWindowProcW(hwnd, message, wparam, lparam);

    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

static reach_result reach_explorer_desktop_compat_register_class(HINSTANCE instance,
                                                                 const wchar_t *class_name)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = reach_explorer_desktop_compat_host_proc;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;

    ATOM atom = RegisterClassExW(&wc);
    if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return REACH_ERROR;
    }

    return REACH_OK;
}

static void reach_explorer_desktop_compat_virtual_screen(int *out_x, int *out_y, int *out_width,
                                                         int *out_height)
{
    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    if (width <= 0)
    {
        x = 0;
        width = GetSystemMetrics(SM_CXSCREEN);
    }

    if (height <= 0)
    {
        y = 0;
        height = GetSystemMetrics(SM_CYSCREEN);
    }

    if (width <= 0)
    {
        width = 1;
    }

    if (height <= 0)
    {
        height = 1;
    }

    if (out_x != nullptr)
    {
        *out_x = x;
    }

    if (out_y != nullptr)
    {
        *out_y = y;
    }

    if (out_width != nullptr)
    {
        *out_width = width;
    }

    if (out_height != nullptr)
    {
        *out_height = height;
    }
}

static int32_t reach_explorer_desktop_compat_process_basename_matches(HWND hwnd,
                                                                      const wchar_t *name_a,
                                                                      const wchar_t *name_b)
{
    DWORD process_id = 0;
    GetWindowThreadProcessId(hwnd, &process_id);

    if (process_id == 0)
    {
        return 0;
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr)
    {
        return 0;
    }

    wchar_t path[512] = {};
    DWORD path_count = 512;
    BOOL ok = QueryFullProcessImageNameW(process, 0, path, &path_count);
    CloseHandle(process);

    if (!ok || path[0] == 0)
    {
        return 0;
    }

    const wchar_t *base = wcsrchr(path, L'\\');
    base = base != nullptr ? base + 1 : path;

    return lstrcmpiW(base, name_a) == 0 || lstrcmpiW(base, name_b) == 0;
}

static int32_t reach_explorer_desktop_compat_is_wallpaper_engine_child(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return 0;
    }

    wchar_t class_name[128] = {};
    wchar_t title[128] = {};

    GetClassNameW(hwnd, class_name, 128);
    GetWindowTextW(hwnd, title, 128);

    int32_t candidate = (lstrcmpiW(class_name, L"WPEDesktopDX11Window") == 0 &&
                         lstrcmpW(title, L"WPELiveWallpaper") == 0) ||
                        (lstrcmpiW(class_name, L"WPEVideoWallpaper") == 0 &&
                         lstrcmpW(title, L"WPEVideoWallpaper") == 0);

    if (!candidate)
    {
        return 0;
    }

    return reach_explorer_desktop_compat_process_basename_matches(hwnd, L"wallpaper32.exe",
                                                                  L"wallpaper64.exe");
}

static void CALLBACK reach_explorer_desktop_compat_winevent_proc(HWINEVENTHOOK hook, DWORD event,
                                                                 HWND hwnd, LONG object_id,
                                                                 LONG child_id, DWORD event_thread,
                                                                 DWORD event_time)
{
    (void)hook;
    (void)child_id;
    (void)event_thread;
    (void)event_time;

    if (object_id != OBJID_WINDOW)
    {
        return;
    }

    if (event == EVENT_OBJECT_CREATE || event == EVENT_OBJECT_DESTROY)
    {
        DWORD host_thread_id = g_hostThreadId.load();
        if (host_thread_id != 0)
        {
            PostThreadMessageW(host_thread_id, REACH_EXPLORER_DESKTOP_COMPAT_REQUEST_SYNC, 0, 0);
        }
    }
}

static void reach_explorer_desktop_compat_unhook_winevent(HWINEVENTHOOK *hook)
{
    if (hook != nullptr && *hook != nullptr)
    {
        UnhookWinEvent(*hook);
        *hook = nullptr;
    }
}

static reach_result reach_explorer_desktop_compat_start_winevent_hooks(void)
{
    if (g_wpeCreateHook == nullptr)
    {
        g_wpeCreateHook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_CREATE, nullptr,
                                          reach_explorer_desktop_compat_winevent_proc, 0, 0,
                                          WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    }

    if (g_wpeDestroyHook == nullptr)
    {
        g_wpeDestroyHook = SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, nullptr,
                                           reach_explorer_desktop_compat_winevent_proc, 0, 0,
                                           WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    }

    return g_wpeCreateHook != nullptr && g_wpeDestroyHook != nullptr ? REACH_OK : REACH_ERROR;
}

static void reach_explorer_desktop_compat_stop_winevent_hooks(void)
{
    reach_explorer_desktop_compat_unhook_winevent(&g_wpeCreateHook);
    reach_explorer_desktop_compat_unhook_winevent(&g_wpeDestroyHook);
}

struct reach_explorer_desktop_compat_resize_children_state
{
    RECT worker_client;
    int32_t found_wallpaper_engine_renderer;
};

static int32_t reach_explorer_desktop_compat_attach_wallpaper_engine_child(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd) || g_workerwHwnd == nullptr || !IsWindow(g_workerwHwnd))
    {
        return 0;
    }

    int32_t changed = 0;
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    LONG_PTR desired_style = (style | WS_CHILD | WS_VISIBLE) & ~WS_POPUP;
    if (desired_style != style)
    {
        SetWindowLongPtrW(hwnd, GWL_STYLE, desired_style);
        changed = 1;
    }

    LONG_PTR ex_style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    LONG_PTR desired_ex_style = ex_style & ~WS_EX_TOPMOST;
    if (desired_ex_style != ex_style)
    {
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, desired_ex_style);
        changed = 1;
    }

    if (GetParent(hwnd) != g_workerwHwnd)
    {
        SetParent(hwnd, g_workerwHwnd);
        changed = 1;
    }

    return changed;
}

static void reach_explorer_desktop_compat_size_wallpaper_engine_child(HWND hwnd,
                                                                      const RECT *worker_client,
                                                                      int32_t force_frame_changed)
{
    if (hwnd == nullptr || worker_client == nullptr || !IsWindow(hwnd))
    {
        return;
    }

    int width = worker_client->right - worker_client->left;
    int height = worker_client->bottom - worker_client->top;

    if (width <= 0)
    {
        width = 1;
    }

    if (height <= 0)
    {
        height = 1;
    }

    RECT window_rect = {};
    RECT worker_rect = {};
    int32_t needs_position = 1;
    if (GetWindowRect(hwnd, &window_rect) && GetWindowRect(g_workerwHwnd, &worker_rect))
    {
        needs_position = window_rect.left != worker_rect.left ||
                         window_rect.top != worker_rect.top ||
                         window_rect.right - window_rect.left != width ||
                         window_rect.bottom - window_rect.top != height;
    }

    int32_t needs_show = !IsWindowVisible(hwnd);
    if (!needs_position && !needs_show && !force_frame_changed)
    {
        return;
    }

    UINT flags = SWP_NOACTIVATE;
    if (!needs_show)
    {
        flags |= SWP_NOOWNERZORDER;
    }
    else
    {
        flags |= SWP_SHOWWINDOW;
    }
    if (force_frame_changed)
    {
        flags |= SWP_FRAMECHANGED;
    }

    SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, width, height, flags);
}

static BOOL CALLBACK reach_explorer_desktop_compat_resize_child_proc(HWND hwnd, LPARAM param)
{
    reach_explorer_desktop_compat_resize_children_state *state =
        reinterpret_cast<reach_explorer_desktop_compat_resize_children_state *>(param);

    if (state == nullptr)
    {
        return TRUE;
    }

    if (!reach_explorer_desktop_compat_is_wallpaper_engine_child(hwnd))
    {
        return TRUE;
    }

    state->found_wallpaper_engine_renderer = 1;
    int32_t changed = reach_explorer_desktop_compat_attach_wallpaper_engine_child(hwnd);
    reach_explorer_desktop_compat_size_wallpaper_engine_child(hwnd, &state->worker_client, changed);

    return TRUE;
}

static BOOL CALLBACK reach_explorer_desktop_compat_find_wallpaper_proc(HWND hwnd, LPARAM param)
{
    reach_explorer_desktop_compat_resize_children_state *state =
        reinterpret_cast<reach_explorer_desktop_compat_resize_children_state *>(param);

    if (state == nullptr || hwnd == nullptr || !IsWindow(hwnd))
    {
        return TRUE;
    }

    if (hwnd == g_progmanHwnd || hwnd == g_workerwHwnd)
    {
        return TRUE;
    }

    if (reach_explorer_desktop_compat_is_wallpaper_engine_child(hwnd))
    {
        state->found_wallpaper_engine_renderer = 1;
        int32_t changed = reach_explorer_desktop_compat_attach_wallpaper_engine_child(hwnd);
        reach_explorer_desktop_compat_size_wallpaper_engine_child(hwnd, &state->worker_client,
                                                                  changed);
        return TRUE;
    }

    EnumChildWindows(hwnd, reach_explorer_desktop_compat_find_wallpaper_proc, param);
    return TRUE;
}

static int32_t reach_explorer_desktop_compat_hidden_wallpaper_contains(HWND hwnd)
{
    for (HWND existing : g_hiddenReachWallpaperHwnds)
    {
        if (existing == hwnd)
        {
            return 1;
        }
    }

    return 0;
}

static int32_t reach_explorer_desktop_compat_is_reach_wallpaper_window(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return 0;
    }

    DWORD process_id = 0;
    GetWindowThreadProcessId(hwnd, &process_id);

    if (process_id != GetCurrentProcessId())
    {
        return 0;
    }

    wchar_t class_name[128] = {};
    GetClassNameW(hwnd, class_name, 128);

    return lstrcmpiW(class_name, L"ReachWallpaperWindow") == 0;
}

static int32_t reach_explorer_desktop_compat_reach_wallpaper_intended_visible(HWND hwnd)
{
    return GetPropW(hwnd, reach_explorer_desktop_compat_wallpaper_visible_property()) != nullptr;
}

static BOOL CALLBACK reach_explorer_desktop_compat_hide_reach_wallpaper_proc(HWND hwnd,
                                                                             LPARAM param)
{
    (void)param;

    if (!reach_explorer_desktop_compat_is_reach_wallpaper_window(hwnd))
    {
        return TRUE;
    }

    if (!reach_explorer_desktop_compat_reach_wallpaper_intended_visible(hwnd))
    {
        return TRUE;
    }

    if (!reach_explorer_desktop_compat_hidden_wallpaper_contains(hwnd))
    {
        g_hiddenReachWallpaperHwnds.push_back(hwnd);
    }

    if (IsWindowVisible(hwnd))
    {
        ShowWindow(hwnd, SW_HIDE);
    }
    return TRUE;
}

static void reach_explorer_desktop_compat_restore_reach_wallpaper_fallback(void)
{
    for (size_t index = 0; index < g_hiddenReachWallpaperHwnds.size();)
    {
        HWND hwnd = g_hiddenReachWallpaperHwnds[index];

        if (hwnd == nullptr || !IsWindow(hwnd) ||
            !reach_explorer_desktop_compat_is_reach_wallpaper_window(hwnd))
        {
            g_hiddenReachWallpaperHwnds.erase(g_hiddenReachWallpaperHwnds.begin() + index);
            continue;
        }

        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

        g_hiddenReachWallpaperHwnds.erase(g_hiddenReachWallpaperHwnds.begin() + index);
    }
}

static void reach_explorer_desktop_compat_sync_reach_wallpaper_fallback(
    int32_t wallpaper_engine_renderer_present)
{
    if (wallpaper_engine_renderer_present)
    {
        EnumWindows(reach_explorer_desktop_compat_hide_reach_wallpaper_proc, 0);
    }
    else if (g_wallpaperEngineRendererPresent.load() != 0 || !g_hiddenReachWallpaperHwnds.empty())
    {
        reach_explorer_desktop_compat_restore_reach_wallpaper_fallback();
    }

    g_wallpaperEngineRendererPresent.store(wallpaper_engine_renderer_present ? 1 : 0);
}

static void reach_explorer_desktop_compat_resize_wallpaper_children(void)
{
    if (g_workerwHwnd == nullptr || !IsWindow(g_workerwHwnd))
    {
        return;
    }

    reach_explorer_desktop_compat_resize_children_state state = {};
    GetClientRect(g_workerwHwnd, &state.worker_client);

    EnumWindows(reach_explorer_desktop_compat_find_wallpaper_proc,
                reinterpret_cast<LPARAM>(&state));
    EnumChildWindows(g_workerwHwnd, reach_explorer_desktop_compat_resize_child_proc,
                     reinterpret_cast<LPARAM>(&state));

    reach_explorer_desktop_compat_sync_reach_wallpaper_fallback(
        state.found_wallpaper_engine_renderer);
}

static void reach_explorer_desktop_compat_resize_host(void)
{
    if (g_progmanHwnd == nullptr || g_workerwHwnd == nullptr || !IsWindow(g_progmanHwnd) ||
        !IsWindow(g_workerwHwnd))
    {
        return;
    }

    int x = 0;
    int y = 0;
    int width = 1;
    int height = 1;

    reach_explorer_desktop_compat_virtual_screen(&x, &y, &width, &height);

    RECT progman_rect = {};
    if (!GetWindowRect(g_progmanHwnd, &progman_rect) || progman_rect.left != x ||
        progman_rect.top != y || progman_rect.right - progman_rect.left != width ||
        progman_rect.bottom - progman_rect.top != height || !IsWindowVisible(g_progmanHwnd))
    {
        SetWindowPos(g_progmanHwnd, HWND_BOTTOM, x, y, width, height,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    RECT worker_rect = {};
    RECT progman_rect_after = {};
    if (!GetWindowRect(g_workerwHwnd, &worker_rect) ||
        !GetWindowRect(g_progmanHwnd, &progman_rect_after) ||
        worker_rect.left != progman_rect_after.left || worker_rect.top != progman_rect_after.top ||
        worker_rect.right - worker_rect.left != width ||
        worker_rect.bottom - worker_rect.top != height || !IsWindowVisible(g_workerwHwnd))
    {
        SetWindowPos(g_workerwHwnd, nullptr, 0, 0, width, height,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    reach_explorer_desktop_compat_resize_wallpaper_children();
}

static reach_result reach_explorer_desktop_compat_create_host_windows(void)
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (instance == nullptr)
    {
        return REACH_ERROR;
    }

    reach_result result = reach_explorer_desktop_compat_register_class(
        instance, reach_explorer_desktop_compat_progman_class());

    if (result != REACH_OK)
    {
        return result;
    }

    result = reach_explorer_desktop_compat_register_class(
        instance, reach_explorer_desktop_compat_workerw_class());

    if (result != REACH_OK)
    {
        return result;
    }

    int x = 0;
    int y = 0;
    int width = 1;
    int height = 1;

    reach_explorer_desktop_compat_virtual_screen(&x, &y, &width, &height);

    DWORD progman_style = WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

    DWORD progman_ex_style = WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP | WS_EX_NOACTIVATE;

    g_progmanHwnd = CreateWindowExW(progman_ex_style, reach_explorer_desktop_compat_progman_class(),
                                    L"Program Manager", progman_style, x, y, width, height, nullptr,
                                    nullptr, instance, nullptr);

    if (g_progmanHwnd == nullptr)
    {
        return REACH_ERROR;
    }

    ShowWindow(g_progmanHwnd, SW_SHOWNOACTIVATE);

    DWORD worker_style = WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

    DWORD worker_ex_style = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT;

    g_workerwHwnd = CreateWindowExW(worker_ex_style, reach_explorer_desktop_compat_workerw_class(),
                                    L"", worker_style, 0, 0, width, height, g_progmanHwnd, nullptr,
                                    instance, nullptr);

    if (g_workerwHwnd == nullptr)
    {
        DestroyWindow(g_progmanHwnd);
        g_progmanHwnd = nullptr;
        return REACH_ERROR;
    }

    ShowWindow(g_workerwHwnd, SW_SHOWNOACTIVATE);

    result = reach_explorer_desktop_compat_start_winevent_hooks();
    if (result != REACH_OK)
    {
        DestroyWindow(g_workerwHwnd);
        g_workerwHwnd = nullptr;

        DestroyWindow(g_progmanHwnd);
        g_progmanHwnd = nullptr;

        return result;
    }

    return REACH_OK;
}

static void reach_explorer_desktop_compat_destroy_host_windows(void)
{
    reach_explorer_desktop_compat_stop_winevent_hooks();

    if (g_progmanHwnd != nullptr && IsWindow(g_progmanHwnd))
    {
        KillTimer(g_progmanHwnd, REACH_EXPLORER_DESKTOP_COMPAT_SYNC_TIMER);
    }
    g_syncScheduled = 0;

    reach_explorer_desktop_compat_restore_reach_wallpaper_fallback();
    g_hiddenReachWallpaperHwnds.clear();
    g_wallpaperEngineRendererPresent.store(-1);

    if (g_workerwHwnd != nullptr)
    {
        DestroyWindow(g_workerwHwnd);
        g_workerwHwnd = nullptr;
    }

    if (g_progmanHwnd != nullptr)
    {
        DestroyWindow(g_progmanHwnd);
        g_progmanHwnd = nullptr;
    }
}

static DWORD WINAPI reach_explorer_desktop_compat_host_thread_proc(void *param)
{
    (void)param;

    MSG message = {};
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

    g_hostThreadId.store(GetCurrentThreadId());
    reach_result create_result = reach_explorer_desktop_compat_create_host_windows();
    g_hostCreateResult.store(create_result);
    if (g_hostReadyEvent != nullptr)
    {
        SetEvent(g_hostReadyEvent);
    }

    if (create_result != REACH_OK)
    {
        reach_explorer_desktop_compat_destroy_host_windows();
        return 1;
    }

    reach_explorer_desktop_compat_schedule_sync(REACH_EXPLORER_DESKTOP_COMPAT_SYNC_DELAY_MS);

    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (message.message == REACH_EXPLORER_DESKTOP_COMPAT_REQUEST_SYNC)
        {
            reach_explorer_desktop_compat_schedule_sync(
                REACH_EXPLORER_DESKTOP_COMPAT_SYNC_DELAY_MS);
            continue;
        }

        if (message.message == REACH_EXPLORER_DESKTOP_COMPAT_SHUTDOWN)
        {
            break;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    reach_explorer_desktop_compat_destroy_host_windows();
    return 0;
}

extern "C" reach_result reach_windows_create_explorer_desktop_compat_host(void)
{
    if (g_hostThread != nullptr)
    {
        DWORD wait_result = WaitForSingleObject(g_hostThread, 0);
        if (wait_result == WAIT_TIMEOUT)
        {
            return static_cast<reach_result>(g_hostCreateResult.load());
        }

        CloseHandle(g_hostThread);
        g_hostThread = nullptr;
        g_hostThreadId.store(0);
        reach_result previous_result = static_cast<reach_result>(g_hostCreateResult.load());
        if (previous_result != REACH_OK)
        {
            return previous_result;
        }
        g_hostCreateResult.store(REACH_ERROR);
    }

    g_hostCreateResult.store(REACH_ERROR);
    g_hostReadyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (g_hostReadyEvent == nullptr)
    {
        return REACH_ERROR;
    }

    g_hostThread = CreateThread(nullptr, 0, reach_explorer_desktop_compat_host_thread_proc, nullptr,
                                0, nullptr);
    if (g_hostThread == nullptr)
    {
        CloseHandle(g_hostReadyEvent);
        g_hostReadyEvent = nullptr;
        g_hostThreadId.store(0);
        return REACH_ERROR;
    }

    WaitForSingleObject(g_hostReadyEvent, INFINITE);
    CloseHandle(g_hostReadyEvent);
    g_hostReadyEvent = nullptr;

    reach_result create_result = static_cast<reach_result>(g_hostCreateResult.load());
    if (create_result != REACH_OK)
    {
        WaitForSingleObject(g_hostThread, INFINITE);
        CloseHandle(g_hostThread);
        g_hostThread = nullptr;
        g_hostThreadId.store(0);
        return create_result;
    }

    return REACH_OK;
}

extern "C" void reach_windows_destroy_explorer_desktop_compat_host(void)
{
    if (g_hostThread == nullptr)
    {
        reach_explorer_desktop_compat_destroy_host_windows();
        g_hostThreadId.store(0);
        return;
    }

    DWORD host_thread_id = g_hostThreadId.load();
    if (host_thread_id != 0)
    {
        PostThreadMessageW(host_thread_id, REACH_EXPLORER_DESKTOP_COMPAT_SHUTDOWN, 0, 0);
    }

    WaitForSingleObject(g_hostThread, INFINITE);
    CloseHandle(g_hostThread);
    g_hostThread = nullptr;
    g_hostThreadId.store(0);
    g_hostCreateResult.store(REACH_ERROR);
}

extern "C" void reach_windows_notify_desktop_environment_changed(void)
{
    DWORD host_thread_id = g_hostThreadId.load();
    if (host_thread_id == 0)
    {
        return;
    }

    if (GetCurrentThreadId() != host_thread_id)
    {
        reach_windows_request_desktop_environment_sync();
        return;
    }

    reach_explorer_desktop_compat_resize_host();
}

extern "C" void reach_windows_request_desktop_environment_sync(void)
{
    DWORD host_thread_id = g_hostThreadId.load();
    if (host_thread_id != 0)
    {
        PostThreadMessageW(host_thread_id, REACH_EXPLORER_DESKTOP_COMPAT_REQUEST_SYNC, 0, 0);
    }
}

extern "C" int32_t reach_windows_desktop_compat_external_wallpaper_active(void)
{
    return g_wallpaperEngineRendererPresent.load() == 1;
}
