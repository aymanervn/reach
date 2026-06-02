#include "windows_adapters_internal.h"

#include <windows.h>
#include <shellapi.h>

#include <cwchar>

static const wchar_t *REACH_WINLOGON_KEY =
    L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon";
static const wchar_t *REACH_SHELL_VALUE = L"Shell";
static const wchar_t *REACH_DEFAULT_SHELL = L"explorer.exe";

static reach_result reach_read_string_value(HKEY root, const wchar_t *key, const wchar_t *value,
                                            wchar_t *buffer, DWORD buffer_count)
{
    REACH_ASSERT(key != nullptr);
    REACH_ASSERT(value != nullptr);
    REACH_ASSERT(buffer != nullptr);

    if (key == nullptr || value == nullptr || buffer == nullptr || buffer_count == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD type = REG_SZ;
    DWORD bytes = buffer_count * sizeof(wchar_t);
    LONG status = RegGetValueW(root, key, value, RRF_RT_REG_SZ, &type, buffer, &bytes);
    if (status != ERROR_SUCCESS)
    {
        buffer[0] = 0;
        return REACH_ERROR;
    }

    buffer[buffer_count - 1] = 0;
    return REACH_OK;
}

static reach_result reach_write_string_value(HKEY root, const wchar_t *key, const wchar_t *value,
                                             const wchar_t *text)
{
    REACH_ASSERT(key != nullptr);
    REACH_ASSERT(value != nullptr);
    REACH_ASSERT(text != nullptr);

    if (key == nullptr || value == nullptr || text == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    HKEY handle = nullptr;
    LONG status =
        RegCreateKeyExW(root, key, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &handle, nullptr);

    if (status != ERROR_SUCCESS)
    {
        return REACH_ERROR;
    }

    DWORD bytes = static_cast<DWORD>((wcslen(text) + 1) * sizeof(wchar_t));
    status = RegSetValueExW(handle, value, 0, REG_SZ, reinterpret_cast<const BYTE *>(text), bytes);

    RegCloseKey(handle);
    return status == ERROR_SUCCESS ? REACH_OK : REACH_ERROR;
}

static int32_t reach_shell_equals(const wchar_t *a, const wchar_t *b)
{
    return a != nullptr && b != nullptr && CompareStringOrdinal(a, -1, b, -1, TRUE) == CSTR_EQUAL;
}

static void reach_shell_command_for_reach(const wchar_t *path, wchar_t *out_command,
                                          size_t out_count)
{
    if (out_command == nullptr || out_count == 0)
    {
        return;
    }

    out_command[0] = 0;

    if (path == nullptr || path[0] == 0)
    {
        return;
    }

    swprintf_s(out_command, out_count, L"\"%ls\" --launch", path);
}

static int32_t reach_shell_is_reach_command(const wchar_t *shell, const wchar_t *path)
{
    if (shell == nullptr || path == nullptr || shell[0] == 0 || path[0] == 0)
    {
        return 0;
    }

    if (reach_shell_equals(shell, path))
    {
        return 1;
    }

    wchar_t expected[640] = {};
    reach_shell_command_for_reach(path, expected, 640);
    return reach_shell_equals(shell, expected);
}

reach_result reach_windows_shell_install_command_current_user(const uint16_t *identity_exe_path,
                                                              const uint16_t *shell_command)
{
    (void)identity_exe_path;

    if (shell_command == nullptr || shell_command[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    return reach_write_string_value(HKEY_CURRENT_USER, REACH_WINLOGON_KEY, REACH_SHELL_VALUE,
                                    reinterpret_cast<const wchar_t *>(shell_command));
}

reach_result reach_windows_shell_install_current_user(const uint16_t *exe_path)
{
    REACH_ASSERT(exe_path != nullptr);

    if (exe_path == nullptr || exe_path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const wchar_t *path = reinterpret_cast<const wchar_t *>(exe_path);

    wchar_t shell_command[640] = {};
    reach_shell_command_for_reach(path, shell_command, 640);
    if (shell_command[0] == 0)
    {
        return REACH_ERROR;
    }

    return reach_write_string_value(HKEY_CURRENT_USER, REACH_WINLOGON_KEY, REACH_SHELL_VALUE,
                                    shell_command);
}

reach_result reach_windows_shell_restore_current_user(void)
{
    return reach_write_string_value(HKEY_CURRENT_USER, REACH_WINLOGON_KEY, REACH_SHELL_VALUE,
                                    REACH_DEFAULT_SHELL);
}

reach_result reach_windows_shell_query_current_user(const uint16_t *exe_path,
                                                    reach_shell_registration_status *out_status)
{
    REACH_ASSERT(out_status != nullptr);

    if (out_status == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_status = {};

    wchar_t current[640] = {};
    if (reach_read_string_value(HKEY_CURRENT_USER, REACH_WINLOGON_KEY, REACH_SHELL_VALUE, current,
                                640) != REACH_OK)
    {
        wcscpy_s(current, REACH_DEFAULT_SHELL);
    }

    (void)reach_copy_utf16(out_status->current_shell, 260,
                           reinterpret_cast<const uint16_t *>(current));

    out_status->previous_shell[0] = 0;

    out_status->reach_is_shell =
        exe_path != nullptr &&
        reach_shell_is_reach_command(current, reinterpret_cast<const wchar_t *>(exe_path));

    return REACH_OK;
}

reach_result reach_windows_shell_launch_explorer(void)
{
    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOASYNC;
    info.lpFile = REACH_DEFAULT_SHELL;
    info.nShow = SW_SHOWNORMAL;

    return ShellExecuteExW(&info) ? REACH_OK : REACH_ERROR;
}
