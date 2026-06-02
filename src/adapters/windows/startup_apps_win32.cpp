
#include "windows_adapters_internal.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <tlhelp32.h>

static int32_t reach_windows_startup_extension_supported(const wchar_t *path)
{
    if (path == nullptr)
    {
        return 0;
    }

    const wchar_t *extension = wcsrchr(path, L'.');
    if (extension == nullptr)
    {
        return 0;
    }

    return _wcsicmp(extension, L".exe") == 0 || _wcsicmp(extension, L".lnk") == 0 ||
           _wcsicmp(extension, L".bat") == 0 || _wcsicmp(extension, L".cmd") == 0;
}

static int32_t reach_windows_paths_equal(const wchar_t *a, const wchar_t *b)
{
    return a != nullptr && b != nullptr && a[0] != 0 && b[0] != 0 && _wcsicmp(a, b) == 0;
}

struct reach_windows_startup_com_scope
{
    HRESULT hr;
    int32_t uninitialize;

    reach_windows_startup_com_scope()
        : hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)), uninitialize(SUCCEEDED(hr) ? 1 : 0)
    {
    }

    ~reach_windows_startup_com_scope()
    {
        if (uninitialize)
        {
            CoUninitialize();
        }
    }
};

static int32_t reach_windows_resolve_shortcut_target(const wchar_t *path, wchar_t *out_target,
                                                     DWORD out_target_count)
{
    if (path == nullptr || out_target == nullptr || out_target_count == 0 ||
        _wcsicmp(PathFindExtensionW(path), L".lnk") != 0)
    {
        return 0;
    }

    out_target[0] = 0;

    reach_windows_startup_com_scope com_scope;

    IShellLinkW *link = nullptr;
    HRESULT hr =
        CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link));
    if (FAILED(hr) || link == nullptr)
    {
        return 0;
    }

    IPersistFile *persist = nullptr;
    hr = link->QueryInterface(IID_PPV_ARGS(&persist));
    if (SUCCEEDED(hr))
    {
        hr = persist->Load(path, STGM_READ);
    }
    if (SUCCEEDED(hr))
    {
        hr = link->GetPath(out_target, out_target_count, nullptr, SLGP_UNCPRIORITY);
    }

    if (persist != nullptr)
    {
        persist->Release();
    }
    link->Release();

    return SUCCEEDED(hr) && out_target[0] != 0;
}

static int32_t reach_windows_resolve_executable_path(const wchar_t *path, wchar_t *out_path,
                                                     DWORD out_path_count)
{
    if (path == nullptr || path[0] == 0 || out_path == nullptr || out_path_count == 0)
    {
        return 0;
    }

    out_path[0] = 0;

    wchar_t shortcut_target[MAX_PATH] = {};
    const wchar_t *candidate = path;
    if (reach_windows_resolve_shortcut_target(path, shortcut_target, MAX_PATH))
    {
        candidate = shortcut_target;
    }

    wchar_t expanded[4096] = {};
    DWORD expanded_count = ExpandEnvironmentStringsW(candidate, expanded, _countof(expanded));
    if (expanded_count > 0 && expanded_count < _countof(expanded))
    {
        candidate = expanded;
    }

    wchar_t full_path[MAX_PATH] = {};
    DWORD full_count = GetFullPathNameW(candidate, MAX_PATH, full_path, nullptr);
    if (full_count > 0 && full_count < MAX_PATH &&
        GetFileAttributesW(full_path) != INVALID_FILE_ATTRIBUTES)
    {
        return SUCCEEDED(StringCchCopyW(out_path, out_path_count, full_path));
    }

    wchar_t searched[MAX_PATH] = {};
    DWORD search_count = SearchPathW(nullptr, candidate, nullptr, MAX_PATH, searched, nullptr);
    if (search_count > 0 && search_count < MAX_PATH)
    {
        return SUCCEEDED(StringCchCopyW(out_path, out_path_count, searched));
    }

    return SUCCEEDED(StringCchCopyW(out_path, out_path_count, candidate));
}

static int32_t reach_windows_executable_running(const wchar_t *executable)
{
    wchar_t target[MAX_PATH] = {};
    if (!reach_windows_resolve_executable_path(executable, target, MAX_PATH))
    {
        return 0;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);

    int32_t running = 0;
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            HANDLE process =
                OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            if (process == nullptr)
            {
                continue;
            }

            wchar_t process_path[MAX_PATH] = {};
            DWORD process_path_count = MAX_PATH;
            if (QueryFullProcessImageNameW(process, 0, process_path, &process_path_count) &&
                reach_windows_paths_equal(target, process_path))
            {
                running = 1;
                CloseHandle(process);
                break;
            }

            CloseHandle(process);
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return running;
}

static void reach_windows_launch_path(const wchar_t *path)
{
    if (path == nullptr || path[0] == 0)
    {
        return;
    }

    if (reach_windows_executable_running(path))
    {
        return;
    }

    SHELLEXECUTEINFOW execute = {};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS;
    execute.lpVerb = L"open";
    execute.lpFile = path;
    execute.nShow = SW_SHOWNORMAL;

    if (ShellExecuteExW(&execute) && execute.hProcess != nullptr)
    {
        CloseHandle(execute.hProcess);
    }
}

static void reach_windows_launch_startup_folder_known_id(REFKNOWNFOLDERID folder_id)
{
    PWSTR folder_path = nullptr;
    HRESULT hr = SHGetKnownFolderPath(folder_id, KF_FLAG_DEFAULT, nullptr, &folder_path);
    if (FAILED(hr) || folder_path == nullptr)
    {
        return;
    }

    wchar_t pattern[MAX_PATH] = {};
    if (FAILED(StringCchPrintfW(pattern, MAX_PATH, L"%ls\\*", folder_path)))
    {
        CoTaskMemFree(folder_path);
        return;
    }

    WIN32_FIND_DATAW find_data = {};
    HANDLE find = FindFirstFileW(pattern, &find_data);

    if (find != INVALID_HANDLE_VALUE)
    {
        do
        {
            if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                continue;
            }

            if (!reach_windows_startup_extension_supported(find_data.cFileName))
            {
                continue;
            }

            wchar_t full_path[MAX_PATH] = {};
            if (FAILED(StringCchPrintfW(full_path, MAX_PATH, L"%ls\\%ls", folder_path,
                                        find_data.cFileName)))
            {
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
    if (value_name == nullptr || value_name[0] == 0)
    {
        return 1;
    }

    HKEY key = nullptr;

    LONG open_result = RegOpenKeyExW(
        root, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run", 0,
        KEY_READ, &key);

    if (open_result != ERROR_SUCCESS)
    {
        return 1;
    }

    BYTE data[64] = {};
    DWORD data_size = sizeof(data);
    DWORD type = 0;

    LONG query_result = RegQueryValueExW(key, value_name, nullptr, &type, data, &data_size);

    RegCloseKey(key);

    if (query_result != ERROR_SUCCESS || type != REG_BINARY || data_size == 0)
    {
        return 1;
    }

    return data[0] != 0x03;
}

static void reach_windows_launch_run_command(const wchar_t *command)
{
    if (command == nullptr || command[0] == 0)
    {
        return;
    }

    wchar_t executable[4096] = {};
    wchar_t arguments[4096] = {};

    const wchar_t *cursor = command;

    while (*cursor == L' ' || *cursor == L'\t')
    {
        ++cursor;
    }

    if (*cursor == L'"')
    {
        ++cursor;

        const wchar_t *end = wcschr(cursor, L'"');
        if (end == nullptr)
        {
            return;
        }

        size_t length = (size_t)(end - cursor);

        if (length >= _countof(executable))
        {
            return;
        }

        wcsncpy_s(executable, cursor, length);
        cursor = end + 1;
    }
    else
    {
        const wchar_t *end = cursor;

        while (*end != 0 && *end != L' ' && *end != L'\t')
        {
            ++end;
        }

        size_t length = (size_t)(end - cursor);

        if (length >= _countof(executable))
        {
            return;
        }

        wcsncpy_s(executable, cursor, length);
        cursor = end;
    }

    while (*cursor == L' ' || *cursor == L'\t')
    {
        ++cursor;
    }

    if (*cursor != 0)
    {
        StringCchCopyW(arguments, _countof(arguments), cursor);
    }

    if (reach_windows_executable_running(executable))
    {
        return;
    }

    SHELLEXECUTEINFOW execute = {};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS;
    execute.lpVerb = L"open";
    execute.lpFile = executable;
    execute.lpParameters = arguments[0] != 0 ? arguments : nullptr;
    execute.nShow = SW_SHOWNORMAL;

    if (ShellExecuteExW(&execute) && execute.hProcess != nullptr)
    {
        CloseHandle(execute.hProcess);
    }
}

static void reach_windows_launch_run_key(HKEY root)
{
    HKEY key = nullptr;

    LONG open_result = RegOpenKeyExW(root, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                                     KEY_READ, &key);

    if (open_result != ERROR_SUCCESS)
    {
        return;
    }

    for (DWORD index = 0;; ++index)
    {
        wchar_t value_name[512] = {};

        DWORD value_name_count = sizeof(value_name) / sizeof(value_name[0]);

        wchar_t raw_data[4096] = {};
        DWORD data_size = sizeof(raw_data);
        DWORD type = 0;

        LONG enum_result = RegEnumValueW(key, index, value_name, &value_name_count, nullptr, &type,
                                         reinterpret_cast<LPBYTE>(raw_data), &data_size);

        if (enum_result == ERROR_NO_MORE_ITEMS)
        {
            break;
        }

        if (enum_result != ERROR_SUCCESS)
        {
            continue;
        }

        if (type != REG_SZ && type != REG_EXPAND_SZ)
        {
            continue;
        }

        if (!reach_windows_startup_value_enabled(root, value_name))
        {
            continue;
        }

        raw_data[_countof(raw_data) - 1] = 0;

        if (type == REG_EXPAND_SZ)
        {
            wchar_t expanded[4096] = {};

            if (ExpandEnvironmentStringsW(raw_data, expanded, _countof(expanded)) > 0)
            {
                reach_windows_launch_run_command(expanded);
            }
        }
        else
        {
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
