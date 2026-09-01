#include "mouse_hook_thread_win32.h"

#include <new>

struct reach_mouse_hook
{
    HOOKPROC proc;
    HHOOK handle;
};

static const wchar_t *REACH_MOUSE_HOOK_WINDOW_CLASS = L"ReachMouseHookThreadWindow";
static const UINT REACH_MOUSE_HOOK_WM_INSTALL = WM_APP + 1;
static const UINT REACH_MOUSE_HOOK_WM_REMOVE = WM_APP + 2;

static SRWLOCK g_mouse_hook_lock = SRWLOCK_INIT;
static HANDLE g_mouse_hook_thread;
static HANDLE g_mouse_hook_ready;
static DWORD g_mouse_hook_thread_id;
static HWND g_mouse_hook_window;
static size_t g_mouse_hook_count;

static LRESULT CALLBACK reach_mouse_hook_window_proc(HWND hwnd, UINT message, WPARAM wparam,
                                                     LPARAM lparam)
{
    if (message == REACH_MOUSE_HOOK_WM_INSTALL || message == REACH_MOUSE_HOOK_WM_REMOVE)
    {
        reach_mouse_hook *hook = reinterpret_cast<reach_mouse_hook *>(lparam);
        if (hook == nullptr)
        {
            return 0;
        }
        if (message == REACH_MOUSE_HOOK_WM_INSTALL)
        {
            hook->handle = SetWindowsHookExW(WH_MOUSE_LL, hook->proc, GetModuleHandleW(nullptr), 0);
            return hook->handle != nullptr ? 1 : 0;
        }
        if (hook->handle != nullptr)
        {
            UnhookWindowsHookEx(hook->handle);
            hook->handle = nullptr;
        }
        return 1;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static DWORD WINAPI reach_mouse_hook_thread_main(void *context)
{
    (void)context;

    HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = reach_mouse_hook_window_proc;
    wc.hInstance = instance;
    wc.lpszClassName = REACH_MOUSE_HOOK_WINDOW_CLASS;
    RegisterClassW(&wc);

    g_mouse_hook_window = CreateWindowExW(0, REACH_MOUSE_HOOK_WINDOW_CLASS, L"", 0, 0, 0, 0, 0,
                                          HWND_MESSAGE, nullptr, instance, nullptr);
    SetEvent(g_mouse_hook_ready);
    if (g_mouse_hook_window == nullptr)
    {
        return 0;
    }

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    DestroyWindow(g_mouse_hook_window);
    g_mouse_hook_window = nullptr;
    return 0;
}

static int32_t reach_mouse_hook_start_thread(void)
{
    if (g_mouse_hook_thread != nullptr)
    {
        return g_mouse_hook_window != nullptr;
    }

    g_mouse_hook_ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (g_mouse_hook_ready == nullptr)
    {
        return 0;
    }

    g_mouse_hook_thread =
        CreateThread(nullptr, 0, reach_mouse_hook_thread_main, nullptr, 0, &g_mouse_hook_thread_id);
    if (g_mouse_hook_thread == nullptr)
    {
        CloseHandle(g_mouse_hook_ready);
        g_mouse_hook_ready = nullptr;
        return 0;
    }

    WaitForSingleObject(g_mouse_hook_ready, INFINITE);
    CloseHandle(g_mouse_hook_ready);
    g_mouse_hook_ready = nullptr;

    if (g_mouse_hook_window == nullptr)
    {
        WaitForSingleObject(g_mouse_hook_thread, INFINITE);
        CloseHandle(g_mouse_hook_thread);
        g_mouse_hook_thread = nullptr;
        g_mouse_hook_thread_id = 0;
        return 0;
    }

    return 1;
}

static void reach_mouse_hook_stop_thread(void)
{
    if (g_mouse_hook_thread == nullptr)
    {
        return;
    }

    PostThreadMessageW(g_mouse_hook_thread_id, WM_QUIT, 0, 0);
    WaitForSingleObject(g_mouse_hook_thread, INFINITE);
    CloseHandle(g_mouse_hook_thread);
    g_mouse_hook_thread = nullptr;
    g_mouse_hook_thread_id = 0;
}

reach_result reach_windows_install_mouse_hook(HOOKPROC proc, reach_mouse_hook **out_hook)
{
    if (proc == nullptr || out_hook == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_hook = nullptr;
    reach_mouse_hook *hook = new (std::nothrow) reach_mouse_hook();
    if (hook == nullptr)
    {
        return REACH_ERROR;
    }
    hook->proc = proc;
    hook->handle = nullptr;

    AcquireSRWLockExclusive(&g_mouse_hook_lock);
    reach_result result = REACH_ERROR;
    if (reach_mouse_hook_start_thread())
    {
        if (SendMessageW(g_mouse_hook_window, REACH_MOUSE_HOOK_WM_INSTALL, 0,
                         reinterpret_cast<LPARAM>(hook)) != 0)
        {
            ++g_mouse_hook_count;
            result = REACH_OK;
        }
        else if (g_mouse_hook_count == 0)
        {
            reach_mouse_hook_stop_thread();
        }
    }
    ReleaseSRWLockExclusive(&g_mouse_hook_lock);

    if (result != REACH_OK)
    {
        delete hook;
        return result;
    }

    *out_hook = hook;
    return REACH_OK;
}

void reach_windows_remove_mouse_hook(reach_mouse_hook *hook)
{
    if (hook == nullptr)
    {
        return;
    }

    AcquireSRWLockExclusive(&g_mouse_hook_lock);
    if (g_mouse_hook_window != nullptr)
    {
        SendMessageW(g_mouse_hook_window, REACH_MOUSE_HOOK_WM_REMOVE, 0,
                     reinterpret_cast<LPARAM>(hook));
    }
    if (g_mouse_hook_count > 0)
    {
        --g_mouse_hook_count;
    }
    if (g_mouse_hook_count == 0)
    {
        reach_mouse_hook_stop_thread();
    }
    ReleaseSRWLockExclusive(&g_mouse_hook_lock);

    delete hook;
}
