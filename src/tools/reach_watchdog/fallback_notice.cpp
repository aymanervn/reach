#include "fallback_notice.h"

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>

#include "reach/adapters/windows/diagnostics_log.h"
#include "reach/support/util.h"

static const wchar_t *REACH_NOTICE_CLASS = L"ReachWatchdogFallbackNotice";
static const wchar_t *REACH_NOTICE_TITLE = L"Reach";
static const wchar_t *REACH_NOTICE_TEXT =
    L"Reach failed and was replaced by Windows Explorer. A crash log was saved. You can "
    L"reinstall Reach.";
static const wchar_t *REACH_NOTICE_PENDING_FILE = L"notice.pending";
static const UINT REACH_NOTICE_CALLBACK_MESSAGE = WM_APP + 1;
static const UINT REACH_NOTICE_ICON_ID = 1;
static const UINT_PTR REACH_NOTICE_TIMER_ID = 1;
static const UINT REACH_NOTICE_BOUND_MS = 120000;
static const DWORD REACH_NOTICE_READY_WAIT_MS = 3000;

static HWND reach_notice_window = nullptr;
static UINT reach_notice_taskbar_created = 0;
static int reach_notice_icon_added = 0;

static void reach_notice_write_pending(void)
{
    wchar_t path[MAX_PATH] = {};
    if (!reach_diagnostics_directory(path, MAX_PATH) ||
        !PathAppendW(path, REACH_NOTICE_PENDING_FILE))
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

static NOTIFYICONDATAW reach_notice_icon_data(HWND window)
{
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = window;
    data.uID = REACH_NOTICE_ICON_ID;
    return data;
}

static void reach_notice_finish(HWND window)
{
    if (reach_notice_icon_added)
    {
        NOTIFYICONDATAW data = reach_notice_icon_data(window);
        Shell_NotifyIconW(NIM_DELETE, &data);
        reach_notice_icon_added = 0;
    }

    KillTimer(window, REACH_NOTICE_TIMER_ID);
    DestroyWindow(window);
}

static void reach_notice_show(HWND window)
{
    if (reach_notice_icon_added)
    {
        NOTIFYICONDATAW stale = reach_notice_icon_data(window);
        Shell_NotifyIconW(NIM_DELETE, &stale);
        reach_notice_icon_added = 0;
    }

    NOTIFYICONDATAW data = reach_notice_icon_data(window);
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = REACH_NOTICE_CALLBACK_MESSAGE;
    data.hIcon = LoadIconW(nullptr, IDI_WARNING);
    wcscpy_s(data.szTip, REACH_NOTICE_TITLE);
    if (!Shell_NotifyIconW(NIM_ADD, &data))
    {
        reach_log_error("notice tray icon add failed");
        return;
    }
    reach_notice_icon_added = 1;

    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);

    data.uFlags = NIF_INFO;
    data.dwInfoFlags = NIIF_WARNING;
    wcscpy_s(data.szInfoTitle, REACH_NOTICE_TITLE);
    wcscpy_s(data.szInfo, REACH_NOTICE_TEXT);
    if (!Shell_NotifyIconW(NIM_MODIFY, &data))
    {
        reach_log_error("notice balloon failed");
        reach_notice_finish(window);
        return;
    }

    reach_log_info("notice shown");
}

static LRESULT CALLBACK reach_notice_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (reach_notice_taskbar_created != 0 && message == reach_notice_taskbar_created)
    {
        reach_notice_show(window);
        return 0;
    }

    switch (message)
    {
    case WM_APP + 1:
        switch (LOWORD(lparam))
        {
        case NIN_BALLOONUSERCLICK:
            reach_log_info("notice clicked");
            (void)reach_diagnostics_open_folder();
            reach_notice_finish(window);
            break;
        case NIN_BALLOONTIMEOUT:
        case NIN_BALLOONHIDE:
            reach_log_info("notice closed");
            reach_notice_finish(window);
            break;
        }
        return 0;
    case WM_TIMER:
        if (wparam == REACH_NOTICE_TIMER_ID)
        {
            reach_log_info("notice abandoned reason=timeout");
            reach_notice_finish(window);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}

static int reach_notice_prepare(void)
{
    reach_notice_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
    if (reach_notice_taskbar_created == 0)
    {
        reach_log_error("notice TaskbarCreated registration failed");
        return 0;
    }

    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = reach_notice_proc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.lpszClassName = REACH_NOTICE_CLASS;
    if (RegisterClassExW(&window_class) == 0)
    {
        reach_log_error("notice window class registration failed");
        return 0;
    }

    reach_notice_window =
        CreateWindowExW(WS_EX_TOOLWINDOW, REACH_NOTICE_CLASS, REACH_NOTICE_TITLE, WS_POPUP, 0, 0, 0,
                        0, nullptr, nullptr, window_class.hInstance, nullptr);
    if (reach_notice_window == nullptr)
    {
        reach_log_error("notice window creation failed");
        return 0;
    }

    SetTimer(reach_notice_window, REACH_NOTICE_TIMER_ID, REACH_NOTICE_BOUND_MS, nullptr);
    return 1;
}

static void reach_notice_wait(void)
{
    if (reach_notice_window == nullptr)
    {
        return;
    }

    reach_log_info("notice waiting for TaskbarCreated");

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    reach_notice_window = nullptr;
}

int reach_fallback_notice_run(void)
{
    if (reach_notice_prepare())
    {
        reach_notice_wait();
    }

    return 0;
}

static int reach_notice_create_child(wchar_t *command_line, DWORD flags,
                                     PROCESS_INFORMATION *process)
{
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    return CreateProcessW(nullptr, command_line, nullptr, nullptr, FALSE, flags, nullptr, nullptr,
                          &startup, process);
}

int reach_fallback_notice_launch(void)
{
    reach_notice_write_pending();

    wchar_t path[MAX_PATH] = {};
    DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        reach_log_error("notice process path unavailable");
        return 0;
    }

    wchar_t command_line[MAX_PATH + 32] = {};
    wsprintfW(command_line, L"\"%ls\" %ls", path, REACH_FALLBACK_NOTICE_ARGUMENT);

    PROCESS_INFORMATION process = {};
    if (!reach_notice_create_child(command_line, CREATE_BREAKAWAY_FROM_JOB | DETACHED_PROCESS,
                                   &process))
    {
        reach_log_error("notice breakaway launch failed, launching in the current job");
        wsprintfW(command_line, L"\"%ls\" %ls", path, REACH_FALLBACK_NOTICE_ARGUMENT);
        if (!reach_notice_create_child(command_line, DETACHED_PROCESS, &process))
        {
            reach_log_error("notice process launch failed");
            return 0;
        }
    }

    CloseHandle(process.hThread);
    WaitForInputIdle(process.hProcess, REACH_NOTICE_READY_WAIT_MS);
    CloseHandle(process.hProcess);
    return 1;
}
