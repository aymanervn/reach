
#include "windows_adapters_internal.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <strsafe.h>

static int32_t reach_windows_startup_extension_supported(const wchar_t *path)
{
    if (path == nullptr) {
        return 0;
    }

    const wchar_t *extension = wcsrchr(path, L'.');
    if (extension == nullptr) {
        return 0;
    }

    return
        _wcsicmp(extension, L".exe") == 0 ||
        _wcsicmp(extension, L".lnk") == 0 ||
        _wcsicmp(extension, L".bat") == 0 ||
        _wcsicmp(extension, L".cmd") == 0;
}

static void reach_windows_launch_path(const wchar_t *path)
{
    if (path == nullptr || path[0] == 0) {
        return;
    }

    SHELLEXECUTEINFOW execute = {};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS;
    execute.lpVerb = L"open";
    execute.lpFile = path;
    execute.nShow = SW_SHOWNORMAL;

    if (ShellExecuteExW(&execute) && execute.hProcess != nullptr) {
        CloseHandle(execute.hProcess);
    }
}

static void reach_windows_launch_startup_folder_known_id(REFKNOWNFOLDERID folder_id)
{
    PWSTR folder_path = nullptr;
    HRESULT hr = SHGetKnownFolderPath(folder_id, KF_FLAG_DEFAULT, nullptr, &folder_path);
    if (FAILED(hr) || folder_path == nullptr) {
        return;
    }

    wchar_t pattern[MAX_PATH] = {};
    if (FAILED(StringCchPrintfW(pattern, MAX_PATH, L"%ls\\*", folder_path))) {
        CoTaskMemFree(folder_path);
        return;
    }

    WIN32_FIND_DATAW find_data = {};
    HANDLE find = FindFirstFileW(pattern, &find_data);

    if (find != INVALID_HANDLE_VALUE) {
        do {
            if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                continue;
            }

            if (!reach_windows_startup_extension_supported(find_data.cFileName)) {
                continue;
            }

            wchar_t full_path[MAX_PATH] = {};
            if (FAILED(StringCchPrintfW(
                    full_path,
                    MAX_PATH,
                    L"%ls\\%ls",
                    folder_path,
                    find_data.cFileName))) {
                continue;
            }

            reach_windows_launch_path(full_path);

        } while (FindNextFileW(find, &find_data));

        FindClose(find);
    }

    CoTaskMemFree(folder_path);
}

static int32_t reach_windows_startup_value_enabled(HKEY root, const wchar_t *value_name)
{
    if (value_name == nullptr || value_name[0] == 0) {
        return 1;
    }

    HKEY key = nullptr;

    LONG open_result = RegOpenKeyExW(
        root,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run",
        0,
        KEY_READ,
        &key);

    if (open_result != ERROR_SUCCESS) {
        return 1;
    }

    BYTE data[64] = {};
    DWORD data_size = sizeof(data);
    DWORD type = 0;

    LONG query_result = RegQueryValueExW(
        key,
        value_name,
        nullptr,
        &type,
        data,
        &data_size);

    RegCloseKey(key);

    if (query_result != ERROR_SUCCESS ||
        type != REG_BINARY ||
        data_size == 0) {
        return 1;
    }

    return data[0] != 0x03;
}

static void reach_windows_launch_run_command(const wchar_t *command)
{
    if (command == nullptr || command[0] == 0) {
        return;
    }

    wchar_t executable[4096] = {};
    wchar_t arguments[4096] = {};

    const wchar_t *cursor = command;

    while (*cursor == L' ' || *cursor == L'\t') {
        ++cursor;
    }

    if (*cursor == L'"') {
        ++cursor;

        const wchar_t *end = wcschr(cursor, L'"');
        if (end == nullptr) {
            return;
        }

        size_t length = (size_t)(end - cursor);

        if (length >= _countof(executable)) {
            return;
        }

        wcsncpy_s(executable, cursor, length);
        cursor = end + 1;
    } else {
        const wchar_t *end = cursor;

        while (*end != 0 && *end != L' ' && *end != L'\t') {
            ++end;
        }

        size_t length = (size_t)(end - cursor);

        if (length >= _countof(executable)) {
            return;
        }

        wcsncpy_s(executable, cursor, length);
        cursor = end;
    }

    while (*cursor == L' ' || *cursor == L'\t') {
        ++cursor;
    }

    if (*cursor != 0) {
        StringCchCopyW(arguments, _countof(arguments), cursor);
    }

    SHELLEXECUTEINFOW execute = {};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS;
    execute.lpVerb = L"open";
    execute.lpFile = executable;
    execute.lpParameters = arguments[0] != 0 ? arguments : nullptr;
    execute.nShow = SW_SHOWNORMAL;

    if (ShellExecuteExW(&execute) && execute.hProcess != nullptr) {
        CloseHandle(execute.hProcess);
    }
}

static void reach_windows_launch_run_key(HKEY root)
{
    HKEY key = nullptr;

    LONG open_result = RegOpenKeyExW(
        root,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        KEY_READ,
        &key);

    if (open_result != ERROR_SUCCESS) {
        return;
    }

    for (DWORD index = 0;; ++index) {
        wchar_t value_name[512] = {};

        DWORD value_name_count =
            sizeof(value_name) / sizeof(value_name[0]);

        wchar_t raw_data[4096] = {};
        DWORD data_size = sizeof(raw_data);
        DWORD type = 0;

        LONG enum_result = RegEnumValueW(
            key,
            index,
            value_name,
            &value_name_count,
            nullptr,
            &type,
            reinterpret_cast<LPBYTE>(raw_data),
            &data_size);

        if (enum_result == ERROR_NO_MORE_ITEMS) {
            break;
        }

        if (enum_result != ERROR_SUCCESS) {
            continue;
        }

        if (type != REG_SZ && type != REG_EXPAND_SZ) {
            continue;
        }

        if (!reach_windows_startup_value_enabled(root, value_name)) {
            continue;
        }

        raw_data[_countof(raw_data) - 1] = 0;

        if (type == REG_EXPAND_SZ) {
            wchar_t expanded[4096] = {};

            if (ExpandEnvironmentStringsW(
                    raw_data,
                    expanded,
                    _countof(expanded)) > 0) {
                reach_windows_launch_run_command(expanded);
            }
        } else {
            reach_windows_launch_run_command(raw_data);
        }
    }

    RegCloseKey(key);
}

reach_result reach_windows_launch_startup_apps(void)
{
    reach_windows_launch_startup_folder_known_id(FOLDERID_Startup);
    reach_windows_launch_startup_folder_known_id(FOLDERID_CommonStartup);

    reach_windows_launch_run_key(HKEY_CURRENT_USER);
    reach_windows_launch_run_key(HKEY_LOCAL_MACHINE);

    return REACH_OK;
}
