#include "windows_adapters_internal.h"

#include "reach/support/search_catalog.h"
#include "reach/support/search_types.h"

#include <windows.h>

#include <new>
#include <vector>

static const size_t REACH_CATALOG_ENTRY_CAP = 4096;

struct reach_catalog_entry
{
    uint16_t name[REACH_SEARCH_RESULT_NAME_CAPACITY];
    uint16_t path[REACH_SEARCH_RESULT_PATH_CAPACITY];
    uint16_t arguments[REACH_SEARCH_RESULT_ARGUMENTS_CAPACITY];
    size_t alias_index;
    int32_t has_alias;
};

struct reach_windows_search_catalog
{
    std::vector<reach_catalog_entry> entries;
    int32_t built;
};

static uint16_t reach_catalog_lower(uint16_t ch)
{
    return ch >= 'A' && ch <= 'Z' ? (uint16_t)(ch - 'A' + 'a') : ch;
}

static int32_t reach_catalog_equals_ci(const uint16_t *a, const uint16_t *b)
{
    if (a == nullptr || b == nullptr)
    {
        return 0;
    }
    size_t index = 0;
    while (a[index] != 0 && b[index] != 0)
    {
        if (reach_catalog_lower(a[index]) != reach_catalog_lower(b[index]))
        {
            return 0;
        }
        ++index;
    }
    return a[index] == 0 && b[index] == 0;
}

static const uint16_t *reach_catalog_file_name(const uint16_t *path)
{
    const uint16_t *name = path;
    for (const uint16_t *scan = path; *scan != 0; ++scan)
    {
        if (*scan == '\\' || *scan == '/')
        {
            name = scan + 1;
        }
    }
    return name;
}

static int32_t reach_catalog_path_exists(const uint16_t *path)
{
    DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(path));
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static int32_t reach_catalog_is_packaged_path(const uint16_t *path)
{
    static const wchar_t fragment[] = L"\\windowsapps\\";
    for (size_t start = 0; path[start] != 0; ++start)
    {
        size_t index = 0;
        while (fragment[index] != 0 && path[start + index] != 0 &&
               reach_catalog_lower(path[start + index]) == (uint16_t)fragment[index])
        {
            ++index;
        }
        if (fragment[index] == 0)
        {
            return 1;
        }
    }
    return 0;
}

static void reach_catalog_append(reach_windows_search_catalog *catalog, const uint16_t *name,
                                 const uint16_t *path, const uint16_t *arguments,
                                 size_t alias_index, int32_t has_alias)
{
    if (catalog->entries.size() >= REACH_CATALOG_ENTRY_CAP)
    {
        return;
    }

    for (const reach_catalog_entry &existing : catalog->entries)
    {
        if (reach_catalog_equals_ci(existing.path, path) &&
            reach_catalog_equals_ci(existing.arguments, arguments))
        {
            return;
        }
    }

    reach_catalog_entry entry = {};
    reach_copy_utf16(entry.name, REACH_SEARCH_RESULT_NAME_CAPACITY, name);
    reach_copy_utf16(entry.path, REACH_SEARCH_RESULT_PATH_CAPACITY, path);
    reach_copy_utf16(entry.arguments, REACH_SEARCH_RESULT_ARGUMENTS_CAPACITY, arguments);
    entry.alias_index = alias_index;
    entry.has_alias = has_alias;
    catalog->entries.push_back(entry);
}

static void reach_catalog_build_aliases(reach_windows_search_catalog *catalog,
                                        const wchar_t *system_directory)
{
    size_t count = reach_search_alias_count();
    for (size_t index = 0; index < count; ++index)
    {
        const reach_search_alias_entry *alias = reach_search_alias_at(index);
        if (alias == nullptr || alias->target == nullptr)
        {
            continue;
        }

        uint16_t target[REACH_SEARCH_RESULT_PATH_CAPACITY] = {};
        reach_copy_ascii_to_utf16(target, REACH_SEARCH_RESULT_PATH_CAPACITY, alias->target);

        uint16_t path[REACH_SEARCH_RESULT_PATH_CAPACITY] = {};
        wchar_t combined[REACH_SEARCH_RESULT_PATH_CAPACITY] = {};
        if (_snwprintf_s(combined, REACH_SEARCH_RESULT_PATH_CAPACITY, _TRUNCATE, L"%s\\%s",
                         system_directory, reinterpret_cast<const wchar_t *>(target)) < 0)
        {
            continue;
        }
        reach_copy_utf16(path, REACH_SEARCH_RESULT_PATH_CAPACITY,
                         reinterpret_cast<const uint16_t *>(combined));
        if (!reach_catalog_path_exists(path))
        {
            wchar_t resolved[REACH_SEARCH_RESULT_PATH_CAPACITY] = {};
            DWORD length = SearchPathW(nullptr, reinterpret_cast<const wchar_t *>(target), nullptr,
                                       REACH_SEARCH_RESULT_PATH_CAPACITY, resolved, nullptr);
            if (length == 0 || length >= REACH_SEARCH_RESULT_PATH_CAPACITY)
            {
                continue;
            }
            reach_copy_utf16(path, REACH_SEARCH_RESULT_PATH_CAPACITY,
                             reinterpret_cast<const uint16_t *>(resolved));
            if (!reach_catalog_path_exists(path))
            {
                continue;
            }
        }

        uint16_t name[REACH_SEARCH_RESULT_NAME_CAPACITY] = {};
        uint16_t arguments[REACH_SEARCH_RESULT_ARGUMENTS_CAPACITY] = {};
        reach_copy_ascii_to_utf16(name, REACH_SEARCH_RESULT_NAME_CAPACITY, alias->display);
        reach_copy_ascii_to_utf16(arguments, REACH_SEARCH_RESULT_ARGUMENTS_CAPACITY,
                                  alias->arguments);
        reach_catalog_append(catalog, name, path, arguments, index, 1);
    }
}

static void reach_catalog_build_system_pattern(reach_windows_search_catalog *catalog,
                                               const wchar_t *system_directory,
                                               const wchar_t *pattern)
{
    wchar_t search[REACH_SEARCH_RESULT_PATH_CAPACITY] = {};
    if (_snwprintf_s(search, REACH_SEARCH_RESULT_PATH_CAPACITY, _TRUNCATE, L"%s\\%s",
                     system_directory, pattern) < 0)
    {
        return;
    }

    WIN32_FIND_DATAW found = {};
    HANDLE handle = FindFirstFileW(search, &found);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return;
    }

    do
    {
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            continue;
        }

        wchar_t combined[REACH_SEARCH_RESULT_PATH_CAPACITY] = {};
        if (_snwprintf_s(combined, REACH_SEARCH_RESULT_PATH_CAPACITY, _TRUNCATE, L"%s\\%s",
                         system_directory, found.cFileName) < 0)
        {
            continue;
        }
        reach_catalog_append(catalog, reinterpret_cast<const uint16_t *>(found.cFileName),
                             reinterpret_cast<const uint16_t *>(combined), nullptr, 0, 0);
    } while (FindNextFileW(handle, &found));

    FindClose(handle);
}

static void reach_catalog_build_app_paths(reach_windows_search_catalog *catalog, HKEY root)
{
    static const wchar_t key_path[] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths";

    HKEY key = nullptr;
    if (RegOpenKeyExW(root, key_path, 0, KEY_READ, &key) != ERROR_SUCCESS)
    {
        return;
    }

    for (DWORD index = 0;; ++index)
    {
        wchar_t sub_name[REACH_SEARCH_RESULT_NAME_CAPACITY] = {};
        DWORD sub_length = REACH_SEARCH_RESULT_NAME_CAPACITY;
        if (RegEnumKeyExW(key, index, sub_name, &sub_length, nullptr, nullptr, nullptr, nullptr) !=
            ERROR_SUCCESS)
        {
            break;
        }

        HKEY sub_key = nullptr;
        if (RegOpenKeyExW(key, sub_name, 0, KEY_READ, &sub_key) != ERROR_SUCCESS)
        {
            continue;
        }

        wchar_t value[REACH_SEARCH_RESULT_PATH_CAPACITY] = {};
        DWORD value_bytes = sizeof(value) - sizeof(wchar_t);
        DWORD value_type = 0;
        LSTATUS status = RegQueryValueExW(sub_key, nullptr, nullptr, &value_type,
                                          reinterpret_cast<LPBYTE>(value), &value_bytes);
        RegCloseKey(sub_key);
        if (status != ERROR_SUCCESS || (value_type != REG_SZ && value_type != REG_EXPAND_SZ))
        {
            continue;
        }

        wchar_t expanded[REACH_SEARCH_RESULT_PATH_CAPACITY] = {};
        const wchar_t *source = value;
        if (value_type == REG_EXPAND_SZ)
        {
            if (ExpandEnvironmentStringsW(value, expanded, REACH_SEARCH_RESULT_PATH_CAPACITY) == 0)
            {
                continue;
            }
            source = expanded;
        }

        wchar_t unquoted[REACH_SEARCH_RESULT_PATH_CAPACITY] = {};
        size_t write = 0;
        for (size_t read = 0; source[read] != 0 && write + 1 < REACH_SEARCH_RESULT_PATH_CAPACITY;
             ++read)
        {
            if (source[read] == L'"')
            {
                continue;
            }
            unquoted[write++] = source[read];
        }
        unquoted[write] = 0;
        while (write > 0 && (unquoted[write - 1] == L' ' || unquoted[write - 1] == L';'))
        {
            unquoted[--write] = 0;
        }

        const uint16_t *path = reinterpret_cast<const uint16_t *>(unquoted);
        if (write == 0 || reach_catalog_is_packaged_path(path) || !reach_catalog_path_exists(path))
        {
            continue;
        }

        reach_catalog_append(catalog, reach_catalog_file_name(path), path, nullptr, 0, 0);
    }

    RegCloseKey(key);
}

static void reach_catalog_build(reach_windows_search_catalog *catalog)
{
    if (catalog->built)
    {
        return;
    }
    catalog->built = 1;

    wchar_t system_directory[MAX_PATH] = {};
    if (GetSystemDirectoryW(system_directory, MAX_PATH) == 0)
    {
        return;
    }

    reach_catalog_build_aliases(catalog, system_directory);
    reach_catalog_build_system_pattern(catalog, system_directory, L"*.msc");
    reach_catalog_build_system_pattern(catalog, system_directory, L"*.cpl");
    reach_catalog_build_app_paths(catalog, HKEY_LOCAL_MACHINE);
    reach_catalog_build_app_paths(catalog, HKEY_CURRENT_USER);
}

reach_result reach_windows_search_catalog_create(reach_windows_search_catalog **out_catalog)
{
    if (out_catalog == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_windows_search_catalog *catalog = new (std::nothrow) reach_windows_search_catalog();
    if (catalog == nullptr)
    {
        return REACH_ERROR;
    }
    catalog->built = 0;
    *out_catalog = catalog;
    return REACH_OK;
}

void reach_windows_search_catalog_destroy(reach_windows_search_catalog *catalog)
{
    delete catalog;
}

size_t reach_windows_search_catalog_collect(reach_windows_search_catalog *catalog,
                                            const uint16_t *query,
                                            reach_search_candidate *out_candidates, size_t capacity)
{
    if (catalog == nullptr || query == nullptr || query[0] == 0 || out_candidates == nullptr ||
        capacity == 0)
    {
        return 0;
    }

    reach_catalog_build(catalog);

    size_t count = 0;
    for (const reach_catalog_entry &entry : catalog->entries)
    {
        if (count >= capacity)
        {
            break;
        }

        reach_search_match_tier tier = reach_search_match_name(entry.name, query);
        if (entry.has_alias)
        {
            reach_search_match_tier alias_tier =
                reach_search_alias_match(reach_search_alias_at(entry.alias_index), query);
            if (alias_tier > tier)
            {
                tier = alias_tier;
            }
        }
        if (tier == REACH_SEARCH_MATCH_NONE)
        {
            reach_search_match_tier name_tier =
                reach_search_match_name(reach_catalog_file_name(entry.path), query);
            if (name_tier == REACH_SEARCH_MATCH_NONE)
            {
                continue;
            }
            tier = name_tier;
        }

        reach_search_candidate *candidate = &out_candidates[count];
        *candidate = {};
        reach_copy_utf16(candidate->name, REACH_SEARCH_RESULT_NAME_CAPACITY, entry.name);
        reach_copy_utf16(candidate->path, REACH_SEARCH_RESULT_PATH_CAPACITY, entry.path);
        reach_copy_utf16(candidate->arguments, REACH_SEARCH_RESULT_ARGUMENTS_CAPACITY,
                         entry.arguments);
        candidate->kind = reach_search_classify_result(entry.path, 0);
        candidate->source = REACH_SEARCH_SOURCE_CATALOG;
        candidate->match_tier = (int32_t)tier;
        candidate->pinned = tier == REACH_SEARCH_MATCH_EXACT ? 1 : 0;
        candidate->is_directory = 0;
        ++count;
    }

    return count;
}
