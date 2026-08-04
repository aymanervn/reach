
#include "windows_adapters_internal.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <tlhelp32.h>

#include <stdlib.h>
#include <string.h>

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

static int32_t reach_windows_startup_is_space(wchar_t value)
{
    return value == L' ' || value == L'\t';
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

static int32_t reach_windows_startup_resolve_executable_candidate(const wchar_t *candidate,
                                                                  wchar_t *out_path,
                                                                  DWORD out_path_count)
{
    if (candidate == nullptr || candidate[0] == 0 || out_path == nullptr || out_path_count == 0 ||
        !reach_windows_startup_extension_supported(candidate))
    {
        return 0;
    }

    wchar_t resolved[MAX_PATH] = {};
    if (!reach_windows_resolve_executable_path(candidate, resolved, MAX_PATH))
    {
        return 0;
    }

    DWORD attributes = GetFileAttributesW(resolved);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        return 0;
    }

    return SUCCEEDED(StringCchCopyW(out_path, out_path_count, resolved));
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

static const wchar_t *reach_windows_startup_run_path =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t *reach_windows_startup_approved_run_path =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
static const wchar_t *reach_windows_startup_approved_folder_path =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\StartupFolder";

static HKEY reach_windows_startup_source_root(reach_startup_app_source source)
{
    return reach_startup_app_source_is_machine(source) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
}

static int32_t reach_windows_startup_source_is_run(reach_startup_app_source source)
{
    return source == REACH_STARTUP_APP_SOURCE_USER_RUN ||
           source == REACH_STARTUP_APP_SOURCE_MACHINE_RUN;
}

static const wchar_t *reach_windows_startup_approved_path(reach_startup_app_source source)
{
    return reach_windows_startup_source_is_run(source) ? reach_windows_startup_approved_run_path
                                                       : reach_windows_startup_approved_folder_path;
}

static reach_startup_app_entry *reach_windows_startup_add_entry(reach_startup_app_list *list,
                                                                reach_startup_app_source source,
                                                                const wchar_t *key)
{
    if (list == nullptr || key == nullptr || key[0] == 0 ||
        list->count >= REACH_STARTUP_APP_MAX_ENTRIES)
    {
        return nullptr;
    }

    reach_startup_app_entry *entry = &list->entries[list->count];
    *entry = {};
    entry->source = source;
    entry->enabled = 1;
    (void)reach_copy_utf16(entry->key, REACH_STARTUP_APP_NAME_CAPACITY,
                           reinterpret_cast<const uint16_t *>(key));
    (void)reach_copy_utf16(entry->display_name, REACH_STARTUP_APP_NAME_CAPACITY,
                           reinterpret_cast<const uint16_t *>(key));
    ++list->count;
    return entry;
}

static int32_t reach_windows_startup_value_enabled(reach_startup_app_source source,
                                                   const wchar_t *value_name)
{
    if (value_name == nullptr || value_name[0] == 0)
    {
        return 1;
    }

    HKEY key = nullptr;

    LONG open_result =
        RegOpenKeyExW(reach_windows_startup_source_root(source),
                      reach_windows_startup_approved_path(source), 0, KEY_READ, &key);

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

static int32_t reach_windows_copy_command_slice(const wchar_t *start, const wchar_t *end,
                                                wchar_t *out_value, DWORD out_value_count)
{
    if (start == nullptr || end == nullptr || out_value == nullptr || out_value_count == 0 ||
        end < start)
    {
        return 0;
    }

    while (end > start && reach_windows_startup_is_space(*(end - 1)))
    {
        --end;
    }

    size_t length = (size_t)(end - start);
    if (length == 0 || length >= out_value_count)
    {
        return 0;
    }

    wcsncpy_s(out_value, out_value_count, start, length);
    return 1;
}

static int32_t reach_windows_parse_unquoted_run_command(const wchar_t *command,
                                                        wchar_t *out_executable,
                                                        DWORD out_executable_count,
                                                        wchar_t *out_arguments,
                                                        DWORD out_arguments_count)
{
    if (command == nullptr || out_executable == nullptr || out_arguments == nullptr)
    {
        return 0;
    }

    const wchar_t *end = command + wcslen(command);
    while (end > command && reach_windows_startup_is_space(*(end - 1)))
    {
        --end;
    }

    wchar_t candidate[4096] = {};
    if (reach_windows_copy_command_slice(command, end, candidate, _countof(candidate)) &&
        reach_windows_startup_resolve_executable_candidate(candidate, out_executable,
                                                           out_executable_count))
    {
        out_arguments[0] = 0;
        return 1;
    }

    for (const wchar_t *split = end; split > command; --split)
    {
        if (!reach_windows_startup_is_space(*(split - 1)))
        {
            continue;
        }

        if (!reach_windows_copy_command_slice(command, split - 1, candidate, _countof(candidate)))
        {
            continue;
        }

        if (!reach_windows_startup_resolve_executable_candidate(candidate, out_executable,
                                                                out_executable_count))
        {
            continue;
        }

        const wchar_t *arguments = split;
        while (reach_windows_startup_is_space(*arguments))
        {
            ++arguments;
        }

        if (*arguments != 0)
        {
            (void)StringCchCopyW(out_arguments, out_arguments_count, arguments);
        }
        else
        {
            out_arguments[0] = 0;
        }
        return 1;
    }

    return 0;
}

static int32_t reach_windows_parse_run_command(const wchar_t *command, wchar_t *out_executable,
                                               DWORD out_executable_count, wchar_t *out_arguments,
                                               DWORD out_arguments_count)
{
    if (command == nullptr || out_executable == nullptr || out_arguments == nullptr ||
        out_executable_count == 0 || out_arguments_count == 0)
    {
        return 0;
    }

    out_executable[0] = 0;
    out_arguments[0] = 0;

    const wchar_t *cursor = command;
    while (reach_windows_startup_is_space(*cursor))
    {
        ++cursor;
    }

    if (*cursor == L'"')
    {
        ++cursor;

        const wchar_t *end = wcschr(cursor, L'"');
        if (end == nullptr)
        {
            return 0;
        }

        wchar_t candidate[4096] = {};
        if (!reach_windows_copy_command_slice(cursor, end, candidate, _countof(candidate)) ||
            !reach_windows_startup_resolve_executable_candidate(candidate, out_executable,
                                                                out_executable_count))
        {
            return 0;
        }

        cursor = end + 1;
        while (reach_windows_startup_is_space(*cursor))
        {
            ++cursor;
        }

        if (*cursor != 0)
        {
            (void)StringCchCopyW(out_arguments, out_arguments_count, cursor);
        }
        return 1;
    }

    return reach_windows_parse_unquoted_run_command(cursor, out_executable, out_executable_count,
                                                    out_arguments, out_arguments_count);
}

static void reach_windows_startup_enumerate_folder(reach_startup_app_list *list,
                                                   reach_startup_app_source source,
                                                   REFKNOWNFOLDERID folder_id)
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

            reach_startup_app_entry *entry =
                reach_windows_startup_add_entry(list, source, find_data.cFileName);
            if (entry == nullptr)
            {
                break;
            }

            wchar_t display[MAX_PATH] = {};
            if (SUCCEEDED(StringCchCopyW(display, MAX_PATH, find_data.cFileName)))
            {
                PathRemoveExtensionW(display);
                (void)reach_copy_utf16(entry->display_name, REACH_STARTUP_APP_NAME_CAPACITY,
                                       reinterpret_cast<const uint16_t *>(display));
            }
            (void)reach_copy_utf16(entry->command, REACH_STARTUP_APP_COMMAND_CAPACITY,
                                   reinterpret_cast<const uint16_t *>(full_path));
            (void)reach_copy_utf16(entry->executable, REACH_STARTUP_APP_PATH_CAPACITY,
                                   reinterpret_cast<const uint16_t *>(full_path));
            entry->resolved = 1;
            entry->enabled = reach_windows_startup_value_enabled(source, find_data.cFileName);

        } while (FindNextFileW(find, &find_data));

        FindClose(find);
    }

    CoTaskMemFree(folder_path);
}

static void reach_windows_startup_enumerate_run_key(reach_startup_app_list *list,
                                                    reach_startup_app_source source)
{
    HKEY key = nullptr;

    LONG open_result = RegOpenKeyExW(reach_windows_startup_source_root(source),
                                     reach_windows_startup_run_path, 0, KEY_READ, &key);

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

        raw_data[_countof(raw_data) - 1] = 0;

        const wchar_t *command = raw_data;
        wchar_t expanded[4096] = {};
        if (type == REG_EXPAND_SZ &&
            ExpandEnvironmentStringsW(raw_data, expanded, _countof(expanded)) > 0)
        {
            command = expanded;
        }

        reach_startup_app_entry *entry =
            reach_windows_startup_add_entry(list, source, value_name);
        if (entry == nullptr)
        {
            break;
        }

        (void)reach_copy_utf16(entry->command, REACH_STARTUP_APP_COMMAND_CAPACITY,
                               reinterpret_cast<const uint16_t *>(command));
        entry->enabled = reach_windows_startup_value_enabled(source, value_name);

        wchar_t executable[4096] = {};
        wchar_t arguments[4096] = {};
        if (reach_windows_parse_run_command(command, executable, _countof(executable), arguments,
                                            _countof(arguments)))
        {
            (void)reach_copy_utf16(entry->executable, REACH_STARTUP_APP_PATH_CAPACITY,
                                   reinterpret_cast<const uint16_t *>(executable));
            (void)reach_copy_utf16(entry->arguments, REACH_STARTUP_APP_PATH_CAPACITY,
                                   reinterpret_cast<const uint16_t *>(arguments));
            entry->resolved = 1;
        }
    }

    RegCloseKey(key);
}

static reach_result reach_windows_startup_enumerate(reach_startup_apps *apps,
                                                    reach_startup_app_list *out_list)
{
    (void)apps;
    if (out_list == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_list = {};
    reach_windows_startup_enumerate_folder(out_list, REACH_STARTUP_APP_SOURCE_USER_FOLDER,
                                           FOLDERID_Startup);
    reach_windows_startup_enumerate_folder(out_list, REACH_STARTUP_APP_SOURCE_MACHINE_FOLDER,
                                           FOLDERID_CommonStartup);
    reach_windows_startup_enumerate_run_key(out_list, REACH_STARTUP_APP_SOURCE_USER_RUN);
    reach_windows_startup_enumerate_run_key(out_list, REACH_STARTUP_APP_SOURCE_MACHINE_RUN);
    return REACH_OK;
}

static LONG reach_windows_startup_write_approved(reach_startup_app_source source,
                                                 const wchar_t *value_name, int32_t enabled)
{
    HKEY key = nullptr;
    LONG open_result = RegCreateKeyExW(reach_windows_startup_source_root(source),
                                       reach_windows_startup_approved_path(source), 0, nullptr,
                                       REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key,
                                       nullptr);
    if (open_result != ERROR_SUCCESS)
    {
        return open_result;
    }

    BYTE data[12] = {};
    data[0] = enabled ? 0x02 : 0x03;
    if (!enabled)
    {
        FILETIME now = {};
        GetSystemTimeAsFileTime(&now);
        memcpy(&data[4], &now.dwLowDateTime, sizeof(now.dwLowDateTime));
        memcpy(&data[8], &now.dwHighDateTime, sizeof(now.dwHighDateTime));
    }

    LONG set_result = RegSetValueExW(key, value_name, 0, REG_BINARY, data, sizeof(data));
    RegCloseKey(key);
    return set_result;
}

static int32_t reach_windows_startup_elevation_helper(wchar_t *out_path, DWORD out_path_count)
{
    wchar_t module[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, module, MAX_PATH) == 0)
    {
        return 0;
    }
    PathRemoveFileSpecW(module);
    if (FAILED(StringCchCopyW(out_path, out_path_count, module)) ||
        !PathAppendW(out_path, L"reachElevate.exe"))
    {
        return 0;
    }
    return GetFileAttributesW(out_path) != INVALID_FILE_ATTRIBUTES;
}

static reach_result reach_windows_startup_set_enabled_elevated(reach_startup_app_source source,
                                                               const wchar_t *value_name,
                                                               int32_t enabled)
{
    if (wcschr(value_name, L'"') != nullptr)
    {
        return REACH_ERROR;
    }

    wchar_t helper[MAX_PATH] = {};
    if (!reach_windows_startup_elevation_helper(helper, MAX_PATH))
    {
        return REACH_ERROR;
    }

    wchar_t parameters[1024] = {};
    if (FAILED(StringCchPrintfW(parameters, _countof(parameters),
                                L"startup-set-enabled %ls %d \"%ls\"",
                                reach_windows_startup_source_is_run(source) ? L"machine-run"
                                                                            : L"machine-folder",
                                enabled ? 1 : 0, value_name)))
    {
        return REACH_ERROR;
    }

    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas";
    info.lpFile = helper;
    info.lpParameters = parameters;
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info) || info.hProcess == nullptr)
    {
        return REACH_ERROR;
    }

    WaitForSingleObject(info.hProcess, INFINITE);
    DWORD exit_code = (DWORD)-1;
    (void)GetExitCodeProcess(info.hProcess, &exit_code);
    CloseHandle(info.hProcess);
    return exit_code == 0 ? REACH_OK : REACH_ERROR;
}

static reach_result reach_windows_startup_set_enabled(reach_startup_apps *apps,
                                                      const reach_startup_app_entry *entry,
                                                      int32_t enabled)
{
    (void)apps;
    if (entry == nullptr || entry->key[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const wchar_t *value_name = reinterpret_cast<const wchar_t *>(entry->key);
    LONG result = reach_windows_startup_write_approved(entry->source, value_name, enabled);
    if (result == ERROR_SUCCESS)
    {
        return REACH_OK;
    }
    if (result == ERROR_ACCESS_DENIED && reach_startup_app_source_is_machine(entry->source))
    {
        return reach_windows_startup_set_enabled_elevated(entry->source, value_name, enabled);
    }
    return REACH_ERROR;
}

static void reach_windows_startup_destroy(reach_startup_apps *apps)
{
    (void)apps;
}

reach_result reach_windows_create_startup_apps(reach_startup_apps_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    out_port->ops.enumerate = reach_windows_startup_enumerate;
    out_port->ops.set_enabled = reach_windows_startup_set_enabled;
    out_port->ops.destroy = reach_windows_startup_destroy;
    return REACH_OK;
}

size_t reach_windows_collect_startup_apps(reach_app_launch_request *out_requests, size_t capacity)
{
    if (out_requests == nullptr || capacity == 0)
    {
        return 0;
    }

    reach_startup_app_list *list =
        static_cast<reach_startup_app_list *>(malloc(sizeof(reach_startup_app_list)));
    if (list == nullptr)
    {
        return 0;
    }
    (void)reach_windows_startup_enumerate(nullptr, list);

    size_t count = 0;
    for (size_t index = 0; index < list->count && count < capacity; ++index)
    {
        const reach_startup_app_entry *entry = &list->entries[index];
        if (!entry->enabled || !entry->resolved)
        {
            continue;
        }
        if (reach_windows_executable_running(reinterpret_cast<const wchar_t *>(entry->executable)))
        {
            continue;
        }

        reach_app_launch_request *request = &out_requests[count];
        *request = {};
        (void)reach_copy_utf16(request->path, 260, entry->executable);
        (void)reach_copy_utf16(request->arguments, 260, entry->arguments);
        ++count;
    }
    free(list);
    return count;
}

uintptr_t reach_windows_get_current_foreground(void)
{
    HWND foreground = GetForegroundWindow();
    if (foreground != nullptr && IsWindow(foreground))
    {
        return reinterpret_cast<uintptr_t>(foreground);
    }
    return 0;
}
