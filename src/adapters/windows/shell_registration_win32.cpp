#include "windows_adapters_internal.h"

#include <windows.h>
#include <shellapi.h>

#include <cwchar>

static const wchar_t *REACH_WINLOGON_KEY = L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon";
static const wchar_t *REACH_RESTORE_KEY = L"Software\\Reach";
static const wchar_t *REACH_SHELL_VALUE = L"Shell";
static const wchar_t *REACH_PREVIOUS_SHELL_VALUE = L"PreviousShell";
static const wchar_t *REACH_STARTUP_ATTEMPTS_VALUE = L"StartupAttemptCount";
static const wchar_t *REACH_DEFAULT_SHELL = L"explorer.exe";
static const DWORD REACH_STARTUP_ATTEMPT_LIMIT = 3;

static reach_result reach_read_string_value(HKEY root, const wchar_t *key, const wchar_t *value, wchar_t *buffer, DWORD buffer_count)
{
    REACH_ASSERT(key != nullptr);
    REACH_ASSERT(value != nullptr);
    REACH_ASSERT(buffer != nullptr);
    if (key == nullptr || value == nullptr || buffer == nullptr || buffer_count == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD type = REG_SZ;
    DWORD bytes = buffer_count * sizeof(wchar_t);
    LONG status = RegGetValueW(root, key, value, RRF_RT_REG_SZ, &type, buffer, &bytes);
    if (status != ERROR_SUCCESS) {
        buffer[0] = 0;
        return REACH_ERROR;
    }

    buffer[buffer_count - 1] = 0;
    return REACH_OK;
}

static reach_result reach_write_string_value(HKEY root, const wchar_t *key, const wchar_t *value, const wchar_t *text)
{
    REACH_ASSERT(key != nullptr);
    REACH_ASSERT(value != nullptr);
    REACH_ASSERT(text != nullptr);
    if (key == nullptr || value == nullptr || text == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    HKEY handle = nullptr;
    LONG status = RegCreateKeyExW(root, key, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &handle, nullptr);
    if (status != ERROR_SUCCESS) {
        return REACH_ERROR;
    }

    DWORD bytes = (DWORD)((wcslen(text) + 1) * sizeof(wchar_t));
    status = RegSetValueExW(handle, value, 0, REG_SZ, reinterpret_cast<const BYTE *>(text), bytes);
    RegCloseKey(handle);
    return status == ERROR_SUCCESS ? REACH_OK : REACH_ERROR;
}

static reach_result reach_read_dword_value(HKEY root, const wchar_t *key, const wchar_t *value, DWORD *out_value)
{
    REACH_ASSERT(key != nullptr);
    REACH_ASSERT(value != nullptr);
    REACH_ASSERT(out_value != nullptr);
    if (key == nullptr || value == nullptr || out_value == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD type = REG_DWORD;
    DWORD bytes = sizeof(*out_value);
    LONG status = RegGetValueW(root, key, value, RRF_RT_REG_DWORD, &type, out_value, &bytes);
    return status == ERROR_SUCCESS ? REACH_OK : REACH_ERROR;
}

static reach_result reach_write_dword_value(HKEY root, const wchar_t *key, const wchar_t *value, DWORD number)
{
    REACH_ASSERT(key != nullptr);
    REACH_ASSERT(value != nullptr);
    if (key == nullptr || value == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    HKEY handle = nullptr;
    LONG status = RegCreateKeyExW(root, key, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &handle, nullptr);
    if (status != ERROR_SUCCESS) {
        return REACH_ERROR;
    }

    status = RegSetValueExW(handle, value, 0, REG_DWORD, reinterpret_cast<const BYTE *>(&number), sizeof(number));
    RegCloseKey(handle);
    return status == ERROR_SUCCESS ? REACH_OK : REACH_ERROR;
}

static int32_t reach_shell_equals(const wchar_t *a, const wchar_t *b)
{
    return a != nullptr && b != nullptr && CompareStringOrdinal(a, -1, b, -1, TRUE) == CSTR_EQUAL;
}

reach_result reach_windows_shell_install_current_user(const uint16_t *exe_path)
{
    REACH_ASSERT(exe_path != nullptr);
    if (exe_path == nullptr || exe_path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    const wchar_t *path = reinterpret_cast<const wchar_t *>(exe_path);
    wchar_t current[260] = {};
    if (reach_read_string_value(HKEY_CURRENT_USER, REACH_WINLOGON_KEY, REACH_SHELL_VALUE, current, 260) != REACH_OK) {
        wcscpy_s(current, REACH_DEFAULT_SHELL);
    }

    if (!reach_shell_equals(current, path)) {
        reach_result backup_result = reach_write_string_value(HKEY_CURRENT_USER, REACH_RESTORE_KEY, REACH_PREVIOUS_SHELL_VALUE, current);
        if (backup_result != REACH_OK) {
            return backup_result;
        }
    }

    reach_result install_result = reach_write_string_value(HKEY_CURRENT_USER, REACH_WINLOGON_KEY, REACH_SHELL_VALUE, path);
    if (install_result == REACH_OK) {
        (void)reach_windows_shell_clear_startup_attempts();
    }
    return install_result;
}

reach_result reach_windows_shell_restore_current_user(void)
{
    wchar_t previous[260] = {};
    if (reach_read_string_value(HKEY_CURRENT_USER, REACH_RESTORE_KEY, REACH_PREVIOUS_SHELL_VALUE, previous, 260) != REACH_OK ||
        previous[0] == 0) {
        wcscpy_s(previous, REACH_DEFAULT_SHELL);
    }

    reach_result restore_result = reach_write_string_value(HKEY_CURRENT_USER, REACH_WINLOGON_KEY, REACH_SHELL_VALUE, previous);
    if (restore_result == REACH_OK) {
        (void)reach_windows_shell_clear_startup_attempts();
    }
    return restore_result;
}

reach_result reach_windows_shell_query_current_user(const uint16_t *exe_path, reach_shell_registration_status *out_status)
{
    REACH_ASSERT(out_status != nullptr);
    if (out_status == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_status = {};
    wchar_t current[260] = {};
    wchar_t previous[260] = {};
    if (reach_read_string_value(HKEY_CURRENT_USER, REACH_WINLOGON_KEY, REACH_SHELL_VALUE, current, 260) != REACH_OK) {
        wcscpy_s(current, REACH_DEFAULT_SHELL);
    }
    (void)reach_read_string_value(HKEY_CURRENT_USER, REACH_RESTORE_KEY, REACH_PREVIOUS_SHELL_VALUE, previous, 260);

    (void)reach_copy_utf16(out_status->current_shell, 260, reinterpret_cast<const uint16_t *>(current));
    (void)reach_copy_utf16(out_status->previous_shell, 260, reinterpret_cast<const uint16_t *>(previous));
    out_status->reach_is_shell = exe_path != nullptr && reach_shell_equals(current, reinterpret_cast<const wchar_t *>(exe_path));
    DWORD attempts = 0;
    if (reach_read_dword_value(HKEY_CURRENT_USER, REACH_RESTORE_KEY, REACH_STARTUP_ATTEMPTS_VALUE, &attempts) == REACH_OK) {
        out_status->startup_attempt_count = attempts;
    }
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

reach_result reach_windows_shell_mark_startup_attempt(const uint16_t *exe_path, uint32_t *out_attempt_count, int32_t *out_restore_required)
{
    REACH_ASSERT(exe_path != nullptr);
    if (exe_path == nullptr || exe_path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    if (out_attempt_count != nullptr) {
        *out_attempt_count = 0;
    }
    if (out_restore_required != nullptr) {
        *out_restore_required = 0;
    }

    reach_shell_registration_status status = {};
    reach_result query_result = reach_windows_shell_query_current_user(exe_path, &status);
    if (query_result != REACH_OK || !status.reach_is_shell) {
        return query_result;
    }

    DWORD attempts = status.startup_attempt_count;
    if (attempts < UINT32_MAX) {
        ++attempts;
    }

    reach_result write_result = reach_write_dword_value(HKEY_CURRENT_USER, REACH_RESTORE_KEY, REACH_STARTUP_ATTEMPTS_VALUE, attempts);
    if (write_result != REACH_OK) {
        return write_result;
    }

    if (out_attempt_count != nullptr) {
        *out_attempt_count = attempts;
    }
    if (out_restore_required != nullptr) {
        *out_restore_required = attempts >= REACH_STARTUP_ATTEMPT_LIMIT;
    }
    return REACH_OK;
}

reach_result reach_windows_shell_clear_startup_attempts(void)
{
    return reach_write_dword_value(HKEY_CURRENT_USER, REACH_RESTORE_KEY, REACH_STARTUP_ATTEMPTS_VALUE, 0);
}
