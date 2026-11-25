#include "window_focus_win32.h"

#include <tlhelp32.h>
#include <wchar.h>

struct reach_window_owner_search
{
    HWND target;
    int32_t found;
};

static BOOL CALLBACK reach_window_owner_search_proc(HWND hwnd, LPARAM lparam)
{
    auto *search = reinterpret_cast<reach_window_owner_search *>(lparam);
    if (hwnd != search->target)
    {
        return TRUE;
    }
    search->found = 1;
    return FALSE;
}

// Workaround: GetWindowThreadProcessId reports a classic console window as owned by its client
// (powershell.exe, cmd.exe), but the window and its input queue belong to a conhost.exe thread.
// Attaching to the reported thread does not lift the foreground lock (measured 2026-11-25).
static DWORD reach_window_console_input_thread(HWND hwnd)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    DWORD hosts[64] = {};
    uint32_t host_count = 0;
    PROCESSENTRY32W process = {};
    process.dwSize = sizeof(process);
    for (BOOL more = Process32FirstW(snapshot, &process); more && host_count < 64;
         more = Process32NextW(snapshot, &process))
    {
        if (_wcsicmp(process.szExeFile, L"conhost.exe") == 0)
        {
            hosts[host_count++] = process.th32ProcessID;
        }
    }

    DWORD owner = 0;
    THREADENTRY32 thread = {};
    thread.dwSize = sizeof(thread);
    for (BOOL more = host_count > 0 && Thread32First(snapshot, &thread); more && owner == 0;
         more = Thread32Next(snapshot, &thread))
    {
        for (uint32_t index = 0; index < host_count; ++index)
        {
            if (thread.th32OwnerProcessID != hosts[index])
            {
                continue;
            }
            reach_window_owner_search search = {hwnd, 0};
            EnumThreadWindows(thread.th32ThreadID, reach_window_owner_search_proc,
                              reinterpret_cast<LPARAM>(&search));
            if (search.found)
            {
                owner = thread.th32ThreadID;
            }
            break;
        }
    }
    CloseHandle(snapshot);
    return owner;
}

DWORD reach_window_input_thread(HWND hwnd)
{
    if (hwnd == nullptr)
    {
        return 0;
    }

    wchar_t class_name[32] = {};
    if (GetClassNameW(hwnd, class_name, 32) > 0 && wcscmp(class_name, L"ConsoleWindowClass") == 0)
    {
        DWORD owner = reach_window_console_input_thread(hwnd);
        if (owner != 0)
        {
            return owner;
        }
    }
    return GetWindowThreadProcessId(hwnd, nullptr);
}

static int32_t reach_window_foreground_matches(HWND target)
{
    HWND foreground = GetForegroundWindow();
    return target != nullptr && foreground != nullptr &&
           (foreground == target || GetAncestor(foreground, GA_ROOT) == target ||
            GetAncestor(foreground, GA_ROOTOWNER) == target);
}

// A hung window's thread never processes the activation messages that sharing its input queue
// waits on, so it is never attached.
static DWORD reach_window_attachable_thread(HWND hwnd)
{
    return hwnd != nullptr && !IsHungAppWindow(hwnd) ? reach_window_input_thread(hwnd) : 0;
}

int32_t reach_window_take_foreground(HWND target, uint32_t flags)
{
    if (target == nullptr || !IsWindow(target))
    {
        return 0;
    }

    DWORD current_thread = GetCurrentThreadId();
    DWORD target_thread = reach_window_attachable_thread(target);
    DWORD foreground_thread = reach_window_attachable_thread(GetForegroundWindow());

    // Workaround: attaching to the target and foreground threads bypasses the foreground lock.
    BOOL attached_target = FALSE;
    BOOL attached_foreground = FALSE;
    if (target_thread != 0 && target_thread != current_thread)
    {
        attached_target = AttachThreadInput(current_thread, target_thread, TRUE);
    }
    if (foreground_thread != 0 && foreground_thread != current_thread &&
        foreground_thread != target_thread)
    {
        attached_foreground = AttachThreadInput(current_thread, foreground_thread, TRUE);
    }

    BOOL foreground_ok = SetForegroundWindow(target);
    if ((flags & REACH_WINDOW_FOCUS_RAISE) != 0)
    {
        BringWindowToTop(target);
    }
    if ((flags & REACH_WINDOW_FOCUS_KEYBOARD) != 0)
    {
        if (GetForegroundWindow() != target)
        {
            (void)SetActiveWindow(target);
        }
        HWND focus = GetFocus();
        if (focus != target && (focus == nullptr || !IsChild(target, focus)))
        {
            (void)SetFocus(target);
        }
    }

    if (attached_foreground)
    {
        (void)AttachThreadInput(current_thread, foreground_thread, FALSE);
    }
    if (attached_target)
    {
        (void)AttachThreadInput(current_thread, target_thread, FALSE);
    }

    return foreground_ok || reach_window_foreground_matches(target);
}
