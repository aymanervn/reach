#include "windows_adapters_internal.h"

#include <windows.h>
#include <tlhelp32.h>
#include <shlwapi.h>

#include <mutex>
#include <new>
#include <string>
#include <vector>

struct reach_application_resolver
{
    int32_t active;
};

struct reach_windows_process_entry
{
    DWORD process_id;
    DWORD parent_process_id;
};

struct reach_windows_aumid_association
{
    std::wstring app_user_model_id;
    std::wstring executable_path;
};

static std::once_flag reach_windows_aumid_once;
static std::vector<reach_windows_aumid_association> reach_windows_aumid_associations;

static int32_t reach_windows_query_process_path(DWORD process_id, wchar_t *out_path,
                                                size_t capacity)
{
    if (process_id == 0 || out_path == nullptr || capacity == 0)
    {
        return 0;
    }
    out_path[0] = 0;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr)
    {
        return 0;
    }
    DWORD count = (DWORD)capacity;
    int32_t result = QueryFullProcessImageNameW(process, 0, out_path, &count) != 0;
    CloseHandle(process);
    return result;
}

static int32_t reach_windows_same_application_tree(const wchar_t *candidate, const wchar_t *runtime)
{
    if (candidate == nullptr || runtime == nullptr || candidate[0] == 0 || runtime[0] == 0)
    {
        return 0;
    }
    wchar_t candidate_directory[260] = {};
    wchar_t runtime_directory[260] = {};
    wcscpy_s(candidate_directory, candidate);
    wcscpy_s(runtime_directory, runtime);
    if (!PathRemoveFileSpecW(candidate_directory) || !PathRemoveFileSpecW(runtime_directory))
    {
        return 0;
    }
    if (lstrcmpiW(candidate_directory, runtime_directory) == 0)
    {
        return 1;
    }
    size_t candidate_length = wcslen(candidate_directory);
    return _wcsnicmp(candidate_directory, runtime_directory, candidate_length) == 0 &&
           (runtime_directory[candidate_length] == L'\\' ||
            runtime_directory[candidate_length] == L'/');
}

static void
reach_windows_collect_process_entries(std::vector<reach_windows_process_entry> *out_entries)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return;
    }
    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            out_entries->push_back({entry.th32ProcessID, entry.th32ParentProcessID});
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
}

static DWORD
reach_windows_parent_process_id(const std::vector<reach_windows_process_entry> &entries,
                                DWORD process_id)
{
    for (const reach_windows_process_entry &entry : entries)
    {
        if (entry.process_id == process_id)
        {
            return entry.parent_process_id;
        }
    }
    return 0;
}

static int32_t reach_windows_process_launch_ancestor(DWORD process_id, const uint16_t *runtime_path,
                                                     uint16_t *out_path)
{
    if (process_id == 0 || runtime_path == nullptr || runtime_path[0] == 0 || out_path == nullptr)
    {
        return 0;
    }
    std::vector<reach_windows_process_entry> entries;
    reach_windows_collect_process_entries(&entries);
    DWORD parent_process_id = reach_windows_parent_process_id(entries, process_id);
    int32_t found = 0;
    for (size_t depth = 0; parent_process_id != 0 && depth < 32; ++depth)
    {
        wchar_t candidate[260] = {};
        if (!reach_windows_query_process_path(parent_process_id, candidate, 260) ||
            !reach_windows_same_application_tree(candidate,
                                                 reinterpret_cast<const wchar_t *>(runtime_path)))
        {
            break;
        }
        if (lstrcmpiW(candidate, reinterpret_cast<const wchar_t *>(runtime_path)) != 0)
        {
            (void)reach_copy_utf16(out_path, REACH_APPLICATION_TEXT_CAPACITY,
                                   reinterpret_cast<const uint16_t *>(candidate));
            found = 1;
        }
        DWORD next = reach_windows_parent_process_id(entries, parent_process_id);
        if (next == parent_process_id)
        {
            break;
        }
        parent_process_id = next;
    }
    return found;
}

static int32_t reach_windows_read_registry_text(HKEY key, const wchar_t *value_name,
                                                std::wstring *out_text)
{
    DWORD type = 0;
    DWORD bytes = 0;
    LONG result = RegQueryValueExW(key, value_name, nullptr, &type, nullptr, &bytes);
    if (result != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) ||
        bytes < sizeof(wchar_t))
    {
        return 0;
    }
    std::vector<wchar_t> value(bytes / sizeof(wchar_t) + 1);
    result = RegQueryValueExW(key, value_name, nullptr, &type,
                              reinterpret_cast<BYTE *>(value.data()), &bytes);
    if (result != ERROR_SUCCESS)
    {
        return 0;
    }
    value.back() = 0;
    if (type == REG_EXPAND_SZ)
    {
        DWORD required = ExpandEnvironmentStringsW(value.data(), nullptr, 0);
        if (required == 0)
        {
            return 0;
        }
        std::vector<wchar_t> expanded(required);
        if (ExpandEnvironmentStringsW(value.data(), expanded.data(), required) == 0)
        {
            return 0;
        }
        *out_text = expanded.data();
        return 1;
    }
    *out_text = value.data();
    return 1;
}

static int32_t reach_windows_command_executable(const std::wstring &command, std::wstring *out_path)
{
    if (command.empty())
    {
        return 0;
    }
    size_t begin = command.find_first_not_of(L" \t");
    if (begin == std::wstring::npos)
    {
        return 0;
    }
    size_t end = std::wstring::npos;
    if (command[begin] == L'"')
    {
        ++begin;
        end = command.find(L'"', begin);
    }
    else
    {
        size_t executable_end = command.find(L".exe", begin);
        if (executable_end != std::wstring::npos)
        {
            end = executable_end + 4;
        }
        else
        {
            end = command.find_first_of(L" \t", begin);
        }
    }
    if (end == std::wstring::npos)
    {
        end = command.size();
    }
    if (end <= begin)
    {
        return 0;
    }
    *out_path = command.substr(begin, end - begin);
    return !out_path->empty();
}

static void reach_windows_collect_aumid_key(HKEY classes, const wchar_t *subkey)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(classes, subkey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
    {
        return;
    }
    std::wstring app_user_model_id;
    if (!reach_windows_read_registry_text(key, L"AppUserModelID", &app_user_model_id))
    {
        RegCloseKey(key);
        return;
    }
    RegCloseKey(key);

    std::wstring command_key(subkey);
    command_key.append(L"\\shell\\open\\command");
    if (RegOpenKeyExW(classes, command_key.c_str(), 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
    {
        command_key.assign(subkey);
        command_key.append(L"\\shell\\explore\\command");
        if (RegOpenKeyExW(classes, command_key.c_str(), 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        {
            return;
        }
    }
    std::wstring command;
    int32_t read = reach_windows_read_registry_text(key, nullptr, &command);
    RegCloseKey(key);
    std::wstring executable_path;
    if (read && reach_windows_command_executable(command, &executable_path))
    {
        reach_windows_aumid_associations.push_back(
            {std::move(app_user_model_id), std::move(executable_path)});
    }
}

static void reach_windows_collect_aumid_root(HKEY root)
{
    HKEY classes = nullptr;
    if (RegOpenKeyExW(root, L"Software\\Classes", 0, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS,
                      &classes) != ERROR_SUCCESS)
    {
        return;
    }
    DWORD subkey_count = 0;
    DWORD longest_subkey = 0;
    if (RegQueryInfoKeyW(classes, nullptr, nullptr, nullptr, &subkey_count, &longest_subkey,
                         nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
    {
        std::vector<wchar_t> name(longest_subkey + 2);
        for (DWORD index = 0; index < subkey_count; ++index)
        {
            DWORD length = (DWORD)name.size();
            if (RegEnumKeyExW(classes, index, name.data(), &length, nullptr, nullptr, nullptr,
                              nullptr) == ERROR_SUCCESS)
            {
                name[length] = 0;
                reach_windows_collect_aumid_key(classes, name.data());
            }
        }
    }
    RegCloseKey(classes);
}

static void reach_windows_collect_aumid_associations()
{
    reach_windows_collect_aumid_root(HKEY_CURRENT_USER);
    reach_windows_collect_aumid_root(HKEY_LOCAL_MACHINE);
}

static int32_t reach_windows_aumid_executable(const uint16_t *app_user_model_id, uint16_t *out_path)
{
    if (app_user_model_id == nullptr || app_user_model_id[0] == 0 || out_path == nullptr)
    {
        return 0;
    }
    std::call_once(reach_windows_aumid_once, reach_windows_collect_aumid_associations);
    const wchar_t *wanted = reinterpret_cast<const wchar_t *>(app_user_model_id);
    for (const reach_windows_aumid_association &association : reach_windows_aumid_associations)
    {
        if (lstrcmpiW(association.app_user_model_id.c_str(), wanted) == 0)
        {
            (void)reach_copy_utf16(
                out_path, REACH_APPLICATION_TEXT_CAPACITY,
                reinterpret_cast<const uint16_t *>(association.executable_path.c_str()));
            return 1;
        }
    }
    return 0;
}

static int32_t reach_windows_packaged_aumid(const uint16_t *app_user_model_id)
{
    if (app_user_model_id == nullptr)
    {
        return 0;
    }
    const wchar_t *value = reinterpret_cast<const wchar_t *>(app_user_model_id);
    const wchar_t *family = wcschr(value, L'_');
    const wchar_t *entry = wcschr(value, L'!');
    return family != nullptr && entry != nullptr && family < entry && entry[1] != 0;
}

reach_result reach_windows_enrich_application(reach_application *application)
{
    if (application == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (reach_windows_packaged_aumid(application->identity.app_user_model_id))
    {
        if (application->launch.path[0] == 0)
        {
            application->launch.kind = REACH_APPLICATION_LAUNCH_PACKAGED;
            if (swprintf_s(reinterpret_cast<wchar_t *>(application->launch.path),
                           REACH_APPLICATION_TEXT_CAPACITY, L"shell:AppsFolder\\%ls",
                           reinterpret_cast<const wchar_t *>(
                               application->identity.app_user_model_id)) <= 0)
            {
                application->launch = {};
                return REACH_ERROR;
            }
        }
        return REACH_OK;
    }

    uint16_t associated_path[REACH_APPLICATION_TEXT_CAPACITY] = {};
    if (reach_windows_aumid_executable(application->identity.app_user_model_id, associated_path))
    {
        (void)reach_application_identity_add_runtime_path(&application->identity, associated_path);
        if (application->launch.path[0] == 0)
        {
            application->launch.kind = REACH_APPLICATION_LAUNCH_EXECUTABLE;
            (void)reach_copy_utf16(application->launch.path, REACH_APPLICATION_TEXT_CAPACITY,
                                   associated_path);
        }
    }
    return REACH_OK;
}

static reach_result
reach_windows_application_resolve(reach_application_resolver *resolver,
                                  const reach_application_observation *observation,
                                  reach_application *out_application)
{
    if (resolver == nullptr || observation == nullptr || out_application == nullptr ||
        (observation->runtime_path[0] == 0 && observation->app_user_model_id[0] == 0))
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_application = {};
    (void)reach_copy_utf16(out_application->identity.app_user_model_id,
                           REACH_APPLICATION_TEXT_CAPACITY, observation->app_user_model_id);
    (void)reach_application_identity_add_runtime_path(&out_application->identity,
                                                      observation->runtime_path);
    (void)reach_copy_utf16(out_application->icon_ref, REACH_APPLICATION_TEXT_CAPACITY,
                           observation->icon_ref);
    (void)reach_windows_enrich_application(out_application);

    if (out_application->launch.path[0] == 0 && observation->runtime_path[0] != 0)
    {
        uint16_t launch_path[REACH_APPLICATION_TEXT_CAPACITY] = {};
        if (!reach_windows_process_launch_ancestor((DWORD)observation->process_id,
                                                   observation->runtime_path, launch_path))
        {
            (void)reach_copy_utf16(launch_path, REACH_APPLICATION_TEXT_CAPACITY,
                                   observation->runtime_path);
        }
        out_application->launch.kind = REACH_APPLICATION_LAUNCH_EXECUTABLE;
        (void)reach_copy_utf16(out_application->launch.path, REACH_APPLICATION_TEXT_CAPACITY,
                               launch_path);
        (void)reach_application_identity_add_runtime_path(&out_application->identity, launch_path);
    }
    return out_application->launch.path[0] != 0 ? REACH_OK : REACH_ERROR;
}

static reach_result reach_windows_application_enrich(reach_application_resolver *resolver,
                                                     reach_application *application)
{
    return resolver != nullptr ? reach_windows_enrich_application(application)
                               : REACH_INVALID_ARGUMENT;
}

static void reach_windows_application_resolver_destroy(reach_application_resolver *resolver)
{
    delete resolver;
}

reach_result reach_windows_create_application_resolver(reach_application_resolver_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_port = {};
    reach_application_resolver *resolver = new (std::nothrow) reach_application_resolver();
    if (resolver == nullptr)
    {
        return REACH_ERROR;
    }
    resolver->active = 1;
    out_port->resolver = resolver;
    out_port->ops.resolve = reach_windows_application_resolve;
    out_port->ops.enrich = reach_windows_application_enrich;
    out_port->ops.destroy = reach_windows_application_resolver_destroy;
    return REACH_OK;
}
