#include "package_store_path.h"

#include <wchar.h>

static const wchar_t REACH_PACKAGE_STORE_SEGMENT[] = L"\\windowsapps\\";

static wchar_t reach_package_store_lower(wchar_t value)
{
    return value >= L'A' && value <= L'Z' ? (wchar_t)(value - L'A' + L'a') : value;
}

static const wchar_t *reach_package_store_find_segment(const wchar_t *path)
{
    size_t segment_length = wcslen(REACH_PACKAGE_STORE_SEGMENT);
    for (const wchar_t *cursor = path; *cursor != 0; ++cursor)
    {
        size_t index = 0;
        while (index < segment_length && cursor[index] != 0 &&
               reach_package_store_lower(cursor[index]) == REACH_PACKAGE_STORE_SEGMENT[index])
        {
            ++index;
        }
        if (index == segment_length)
        {
            return cursor + segment_length;
        }
    }
    return nullptr;
}

int32_t reach_package_store_path_split(const wchar_t *path, wchar_t *out_family_name,
                                       size_t family_name_count, const wchar_t **out_relative)
{
    if (path == nullptr || out_family_name == nullptr || family_name_count == 0 ||
        out_relative == nullptr)
    {
        return 0;
    }
    out_family_name[0] = 0;
    *out_relative = nullptr;

    const wchar_t *folder = reach_package_store_find_segment(path);
    if (folder == nullptr)
    {
        return 0;
    }
    const wchar_t *folder_end = wcschr(folder, L'\\');
    if (folder_end == nullptr || folder_end[1] == 0)
    {
        return 0;
    }

    const wchar_t *separators[4] = {};
    size_t separator_count = 0;
    for (const wchar_t *cursor = folder; cursor < folder_end; ++cursor)
    {
        if (*cursor != L'_')
        {
            continue;
        }
        if (separator_count == 4)
        {
            return 0;
        }
        separators[separator_count++] = cursor;
    }
    if (separator_count != 4)
    {
        return 0;
    }

    size_t name_length = (size_t)(separators[0] - folder);
    size_t publisher_length = (size_t)(folder_end - (separators[3] + 1));
    if (name_length == 0 || publisher_length == 0 ||
        name_length + 1 + publisher_length >= family_name_count)
    {
        return 0;
    }

    wmemcpy(out_family_name, folder, name_length);
    out_family_name[name_length] = L'_';
    wmemcpy(out_family_name + name_length + 1, separators[3] + 1, publisher_length);
    out_family_name[name_length + 1 + publisher_length] = 0;
    *out_relative = folder_end + 1;
    return 1;
}
