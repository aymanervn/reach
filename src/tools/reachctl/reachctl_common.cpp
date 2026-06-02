#include "reachctl_common.h"

#include "reach/platform/windows_adapters.h"
#include "reach/platform/windows_messages.h"

#include <shlwapi.h>
#include <cwchar>

void reachctl_print(const wchar_t *message)
{
    if (message == nullptr)
    {
        return;
    }

    DWORD written = 0;
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!WriteConsoleW(output, message, (DWORD)wcslen(message), &written, nullptr))
    {
        char utf8[1024] = {};
        int bytes = WideCharToMultiByte(CP_UTF8, 0, message, -1, utf8, (int)sizeof(utf8) - 3,
                                        nullptr, nullptr);
        if (bytes > 0)
        {
            utf8[bytes - 1] = '\r';
            utf8[bytes] = '\n';
            WriteFile(output, utf8, (DWORD)(bytes + 1), &written, nullptr);
        }
        return;
    }
    WriteConsoleW(output, L"\r\n", 2, &written, nullptr);
}

reach_result reachctl_target_exe(uint16_t *path, DWORD path_count)
{
    if (path == nullptr || path_count == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD length = GetModuleFileNameW(nullptr, reinterpret_cast<wchar_t *>(path), path_count);
    if (length == 0 || length >= path_count)
    {
        return REACH_ERROR;
    }

    wchar_t *path_w = reinterpret_cast<wchar_t *>(path);
    if (!PathRemoveFileSpecW(path_w))
    {
        return REACH_ERROR;
    }
    if (!PathAppendW(path_w, L"reach.exe"))
    {
        return REACH_ERROR;
    }
    return REACH_OK;
}

reach_result reachctl_current_exe(uint16_t *path, DWORD path_count)
{
    if (path == nullptr || path_count == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD length = GetModuleFileNameW(nullptr, reinterpret_cast<wchar_t *>(path), path_count);
    return length > 0 && length < path_count ? REACH_OK : REACH_ERROR;
}

reach_result reachctl_open_config_store(reach_config_store_port *out_store)
{
    uint16_t config_path[260] = {};
    reach_result result = reach_windows_default_config_path(config_path, 260);
    if (result != REACH_OK)
    {
        return result;
    }
    return reach_windows_create_config_store(config_path, out_store);
}

reach_result reachctl_absolute_path(const uint16_t *path, uint16_t *out_path, DWORD out_path_count)
{
    if (path == nullptr || path[0] == 0 || out_path == nullptr || out_path_count == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    wchar_t full_path[260] = {};
    DWORD length =
        GetFullPathNameW(reinterpret_cast<const wchar_t *>(path), 260, full_path, nullptr);
    if (length == 0 || length >= out_path_count)
    {
        return REACH_ERROR;
    }

    return reach_copy_utf16(out_path, out_path_count,
                            reinterpret_cast<const uint16_t *>(full_path));
}

reach_result reachctl_run_process_wait(const wchar_t *path, const wchar_t *arguments,
                                       const wchar_t *working_directory)
{
    if (path == nullptr || path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    wchar_t command_line[1024] = {};
    if (arguments != nullptr && arguments[0] != 0)
    {
        swprintf_s(command_line, L"\"%ls\" %ls", path, arguments);
    }
    else
    {
        swprintf_s(command_line, L"\"%ls\"", path);
    }

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);

    PROCESS_INFORMATION process = {};
    BOOL ok = CreateProcessW(nullptr, command_line, nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                             nullptr, working_directory, &startup, &process);

    if (!ok)
    {
        return REACH_ERROR;
    }

    CloseHandle(process.hThread);

    DWORD wait_result = WaitForSingleObject(process.hProcess, 15000);

    DWORD exit_code = 1;
    if (wait_result == WAIT_OBJECT_0)
    {
        (void)GetExitCodeProcess(process.hProcess, &exit_code);
    }

    CloseHandle(process.hProcess);

    return wait_result == WAIT_OBJECT_0 && exit_code == 0 ? REACH_OK : REACH_ERROR;
}

struct reachctl_notify_config_state
{
    int32_t posted;
};

static BOOL CALLBACK reachctl_notify_config_window_proc(HWND hwnd, LPARAM param)
{
    reachctl_notify_config_state *state = reinterpret_cast<reachctl_notify_config_state *>(param);

    if (state == nullptr || hwnd == nullptr || !IsWindow(hwnd))
    {
        return TRUE;
    }

    wchar_t class_name[64] = {};
    GetClassNameW(hwnd, class_name, 64);

    if (lstrcmpiW(class_name, L"ReachPlatformWindow") != 0)
    {
        return TRUE;
    }

    if (PostMessageW(hwnd, REACH_WM_CONFIG_CHANGED, 0, 0))
    {
        state->posted = 1;
        return FALSE;
    }

    return TRUE;
}

reach_result reachctl_notify_config_changed(void)
{
    reachctl_notify_config_state state = {};
    if (!EnumWindows(reachctl_notify_config_window_proc, reinterpret_cast<LPARAM>(&state)))
    {
        return state.posted ? REACH_OK : REACH_ERROR;
    }

    return state.posted ? REACH_OK : REACH_ERROR;
}

int32_t reachctl_path_equals_ci(const uint16_t *a, const uint16_t *b)
{
    if (a == nullptr || b == nullptr)
    {
        return 0;
    }

    size_t index = 0;
    while (a[index] != 0 && b[index] != 0)
    {
        uint16_t ca = a[index];
        uint16_t cb = b[index];

        if (ca >= 'A' && ca <= 'Z')
        {
            ca = (uint16_t)(ca + ('a' - 'A'));
        }
        if (cb >= 'A' && cb <= 'Z')
        {
            cb = (uint16_t)(cb + ('a' - 'A'));
        }

        if (ca != cb)
        {
            return 0;
        }

        ++index;
    }

    return a[index] == b[index];
}
