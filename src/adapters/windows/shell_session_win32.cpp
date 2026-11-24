#include "reach/adapters/windows/shell_session.h"

#include "reach/adapters/windows/diagnostics_log.h"
#include "reach/support/crash_loop.h"
#include "windows_adapters_internal.h"

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>

static const wchar_t *REACH_SHELL_SESSION_HISTORY_FILE = L"start_history";
static const wchar_t *REACH_SHELL_SESSION_NOTICE_FILE = L"notice.pending";
static const wchar_t *REACH_SHELL_SESSION_NOTICE_CLASS = L"ReachFallbackNotice";
static const wchar_t *REACH_SHELL_SESSION_NOTICE_TITLE = L"Reach";
static const wchar_t *REACH_SHELL_SESSION_NOTICE_TEXT =
    L"This Reach version kept failing to start and was replaced by Windows Explorer. A crash log "
    L"was saved. Reinstall Reach to switch back.";
static const UINT REACH_SHELL_SESSION_NOTICE_MESSAGE = WM_APP + 1;
static const UINT REACH_SHELL_SESSION_NOTICE_ICON_ID = 1;
static const UINT_PTR REACH_SHELL_SESSION_NOTICE_TIMER_ID = 1;
static const UINT REACH_SHELL_SESSION_NOTICE_BOUND_MS = 120000;
static const DWORD REACH_SHELL_SESSION_HEALTHY_MS = 10000;

static HANDLE reach_shell_session_claim_thread = nullptr;
static DWORD reach_shell_session_claim_thread_id = 0;
static reach_result reach_shell_session_claim_result = REACH_ERROR;
static SRWLOCK reach_shell_session_lock = SRWLOCK_INIT;

static UINT reach_shell_session_taskbar_created = 0;
static int32_t reach_shell_session_notice_icon_added = 0;

static int32_t reach_shell_session_history_path(wchar_t *out_path)
{
    return reach_diagnostics_directory(out_path, MAX_PATH) &&
           PathAppendW(out_path, REACH_SHELL_SESSION_HISTORY_FILE);
}

static uint64_t reach_shell_session_now_ms(void)
{
    FILETIME now = {};
    GetSystemTimeAsFileTime(&now);
    uint64_t ticks = ((uint64_t)now.dwHighDateTime << 32) | now.dwLowDateTime;
    return ticks / 10000ull;
}

int32_t reach_windows_shell_session_record_start(void)
{
    wchar_t path[MAX_PATH] = {};
    if (!reach_shell_session_history_path(path))
    {
        reach_log_error("start history path unavailable");
        return 0;
    }

    reach_crash_loop loop = {};
    HANDLE file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        reach_log_error("start history not opened");
        return 0;
    }

    DWORD transferred = 0;
    if (!ReadFile(file, &loop, sizeof(loop), &transferred, nullptr) || transferred != sizeof(loop))
    {
        loop = {};
    }

    int32_t looping = reach_crash_loop_record_start(&loop, reach_shell_session_now_ms());

    SetFilePointer(file, 0, nullptr, FILE_BEGIN);
    if (!WriteFile(file, &loop, sizeof(loop), &transferred, nullptr) || transferred != sizeof(loop))
    {
        reach_log_error("start history not written");
    }
    SetEndOfFile(file);
    CloseHandle(file);
    return looping;
}

static BOOL reach_shell_session_set_shell_window(HWND hwnd)
{
    typedef BOOL(WINAPI * reach_set_shell_window_fn)(HWND);
    reach_set_shell_window_fn set_shell_window = reinterpret_cast<reach_set_shell_window_fn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetShellWindow"));
    return set_shell_window != nullptr && set_shell_window(hwnd);
}

static DWORD WINAPI reach_shell_session_claim_proc(void *ready_event)
{
    MSG message = {};
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

    HWND placeholder =
        CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC", L"", WS_POPUP, 0, 0, 0, 0,
                        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    reach_shell_session_claim_result =
        placeholder != nullptr && reach_shell_session_set_shell_window(placeholder) ? REACH_OK
                                                                                    : REACH_ERROR;
    SetEvent(static_cast<HANDLE>(ready_event));

    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        DispatchMessageW(&message);
    }

    if (placeholder != nullptr)
    {
        DestroyWindow(placeholder);
    }
    return 0;
}

reach_result reach_windows_shell_session_claim(void)
{
    AcquireSRWLockExclusive(&reach_shell_session_lock);
    REACH_ASSERT(reach_shell_session_claim_thread == nullptr);

    reach_result result = REACH_ERROR;
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (ready != nullptr)
    {
        reach_shell_session_claim_thread =
            CreateThread(nullptr, 0, reach_shell_session_claim_proc, ready, 0,
                         &reach_shell_session_claim_thread_id);
        if (reach_shell_session_claim_thread != nullptr)
        {
            WaitForSingleObject(ready, INFINITE);
            result = reach_shell_session_claim_result;
        }
        CloseHandle(ready);
    }

    ReleaseSRWLockExclusive(&reach_shell_session_lock);
    if (result != REACH_OK)
    {
        reach_log_error("shell window claim failed; Windows will not restart this Reach");
    }
    return result;
}

static DWORD WINAPI reach_shell_session_health_proc(void *)
{
    Sleep(REACH_SHELL_SESSION_HEALTHY_MS);
    wchar_t path[MAX_PATH] = {};
    if (reach_shell_session_history_path(path))
    {
        HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE)
        {
            reach_crash_loop cleared = {};
            DWORD written = 0;
            WriteFile(file, &cleared, sizeof(cleared), &written, nullptr);
            CloseHandle(file);
        }
    }
    return 0;
}

void reach_windows_shell_session_start_health_timer(void)
{
    HANDLE thread = CreateThread(nullptr, 0, reach_shell_session_health_proc, nullptr, 0, nullptr);
    if (thread != nullptr)
    {
        CloseHandle(thread);
    }
}

reach_result reach_windows_shell_session_register(void *hwnd)
{
    if (hwnd == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    AcquireSRWLockExclusive(&reach_shell_session_lock);
    if (reach_shell_session_claim_thread != nullptr)
    {
        PostThreadMessageW(reach_shell_session_claim_thread_id, WM_QUIT, 0, 0);
        WaitForSingleObject(reach_shell_session_claim_thread, INFINITE);
        CloseHandle(reach_shell_session_claim_thread);
        reach_shell_session_claim_thread = nullptr;
        reach_shell_session_claim_thread_id = 0;
    }

    reach_result result =
        GetShellWindow() == nullptr && reach_shell_session_set_shell_window(static_cast<HWND>(hwnd))
            ? REACH_OK
            : REACH_ERROR;
    ReleaseSRWLockExclusive(&reach_shell_session_lock);
    return result;
}

static void reach_shell_session_write_notice(void)
{
    wchar_t path[MAX_PATH] = {};
    if (!reach_diagnostics_directory(path, MAX_PATH) ||
        !PathAppendW(path, REACH_SHELL_SESSION_NOTICE_FILE))
    {
        reach_log_error("notice pending file path unavailable");
        return;
    }

    HANDLE file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        reach_log_error("notice pending file not written");
        return;
    }

    SYSTEMTIME now = {};
    GetSystemTime(&now);
    char text[96] = {};
    int length = wsprintfA(text, "crash_loop_fallback %04u-%02u-%02uT%02u:%02u:%02uZ", now.wYear,
                           now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    DWORD written = 0;
    WriteFile(file, text, (DWORD)length, &written, nullptr);
    CloseHandle(file);
}

static NOTIFYICONDATAW reach_shell_session_notice_icon(HWND window)
{
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = window;
    data.uID = REACH_SHELL_SESSION_NOTICE_ICON_ID;
    return data;
}

static void reach_shell_session_notice_finish(HWND window)
{
    if (reach_shell_session_notice_icon_added)
    {
        NOTIFYICONDATAW data = reach_shell_session_notice_icon(window);
        Shell_NotifyIconW(NIM_DELETE, &data);
        reach_shell_session_notice_icon_added = 0;
    }

    KillTimer(window, REACH_SHELL_SESSION_NOTICE_TIMER_ID);
    DestroyWindow(window);
}

static void reach_shell_session_notice_show(HWND window)
{
    if (reach_shell_session_notice_icon_added)
    {
        NOTIFYICONDATAW stale = reach_shell_session_notice_icon(window);
        Shell_NotifyIconW(NIM_DELETE, &stale);
        reach_shell_session_notice_icon_added = 0;
    }

    NOTIFYICONDATAW data = reach_shell_session_notice_icon(window);
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = REACH_SHELL_SESSION_NOTICE_MESSAGE;
    data.hIcon = LoadIconW(nullptr, IDI_WARNING);
    wcscpy_s(data.szTip, REACH_SHELL_SESSION_NOTICE_TITLE);
    if (!Shell_NotifyIconW(NIM_ADD, &data))
    {
        reach_log_error("notice tray icon add failed");
        return;
    }
    reach_shell_session_notice_icon_added = 1;

    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);

    data.uFlags = NIF_INFO;
    data.dwInfoFlags = NIIF_WARNING;
    wcscpy_s(data.szInfoTitle, REACH_SHELL_SESSION_NOTICE_TITLE);
    wcscpy_s(data.szInfo, REACH_SHELL_SESSION_NOTICE_TEXT);
    if (!Shell_NotifyIconW(NIM_MODIFY, &data))
    {
        reach_log_error("notice balloon failed");
        reach_shell_session_notice_finish(window);
        return;
    }

    reach_log_info("notice shown");
}

static LRESULT CALLBACK reach_shell_session_notice_proc(HWND window, UINT message, WPARAM wparam,
                                                        LPARAM lparam)
{
    if (reach_shell_session_taskbar_created != 0 && message == reach_shell_session_taskbar_created)
    {
        reach_shell_session_notice_show(window);
        return 0;
    }

    switch (message)
    {
    case REACH_SHELL_SESSION_NOTICE_MESSAGE:
        switch (LOWORD(lparam))
        {
        case NIN_BALLOONUSERCLICK:
            reach_log_info("notice clicked");
            (void)reach_diagnostics_open_folder();
            reach_shell_session_notice_finish(window);
            break;
        case NIN_BALLOONTIMEOUT:
        case NIN_BALLOONHIDE:
            reach_log_info("notice closed");
            reach_shell_session_notice_finish(window);
            break;
        }
        return 0;
    case WM_TIMER:
        if (wparam == REACH_SHELL_SESSION_NOTICE_TIMER_ID)
        {
            reach_log_info("notice abandoned reason=timeout");
            reach_shell_session_notice_finish(window);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}

// The notice waits for Explorer's taskbar to announce itself, then shows a tray balloon.
static void reach_shell_session_run_notice(void)
{
    reach_shell_session_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
    if (reach_shell_session_taskbar_created == 0)
    {
        reach_log_error("notice TaskbarCreated registration failed");
        return;
    }

    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = reach_shell_session_notice_proc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.lpszClassName = REACH_SHELL_SESSION_NOTICE_CLASS;
    if (RegisterClassExW(&window_class) == 0)
    {
        reach_log_error("notice window class registration failed");
        return;
    }

    HWND window = CreateWindowExW(WS_EX_TOOLWINDOW, REACH_SHELL_SESSION_NOTICE_CLASS,
                                  REACH_SHELL_SESSION_NOTICE_TITLE, WS_POPUP, 0, 0, 0, 0, nullptr,
                                  nullptr, window_class.hInstance, nullptr);
    if (window == nullptr)
    {
        reach_log_error("notice window creation failed");
        return;
    }

    SetTimer(window, REACH_SHELL_SESSION_NOTICE_TIMER_ID, REACH_SHELL_SESSION_NOTICE_BOUND_MS,
             nullptr);
    reach_log_info("notice waiting for TaskbarCreated");

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

void reach_windows_shell_session_fall_back(void)
{
    reach_shell_session_write_notice();

    if (reach_windows_shell_restore_current_user() != REACH_OK)
    {
        reach_log_error("fallback could not restore the Explorer shell value");
    }

    if (reach_windows_shell_launch_explorer() != REACH_OK)
    {
        reach_log_error("fallback could not start Explorer");
        return;
    }

    reach_log_info("fallback done shell=explorer.exe");
    reach_shell_session_run_notice();
}
