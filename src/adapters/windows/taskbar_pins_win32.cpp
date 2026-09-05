#include "windows_adapters_internal.h"
#include "shortcut_win32.h"
#include "taskbar_pin_blob.h"

#include <windows.h>
#include <propsys.h>
#include <propkey.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <cstring>
#include <new>
#include <vector>

static int32_t reach_taskbar_pin_same(const reach_pinned_app_model *a,
                                      const reach_pinned_app_model *b)
{
    return reach_application_identity_matches(&a->application.identity,
                                              &b->application.identity);
}

static void reach_taskbar_copy_shell_string(IShellItem2 *item, REFPROPERTYKEY key,
                                             uint16_t *out_value, size_t capacity)
{
    PWSTR value = nullptr;
    if (SUCCEEDED(item->GetString(key, &value)) && value != nullptr)
    {
        (void)reach_copy_utf16(out_value, capacity, reinterpret_cast<const uint16_t *>(value));
    }
    CoTaskMemFree(value);
}

static reach_result reach_taskbar_pin_from_item(IShellItem2 *item, reach_pinned_app_model *out_pin)
{
    if (item == nullptr || out_pin == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_pin = {};
    reach_application *application = &out_pin->application;
    reach_taskbar_copy_shell_string(item, PKEY_AppUserModel_ID,
                                    application->identity.app_user_model_id, 260);
    reach_taskbar_copy_shell_string(item, PKEY_Link_Arguments,
                                    application->launch.arguments, 260);

    PWSTR parsing_path = nullptr;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &parsing_path)) &&
        parsing_path != nullptr && application->identity.app_user_model_id[0] == 0)
    {
        const wchar_t *candidate = wcsrchr(parsing_path, L'\\');
        candidate = candidate != nullptr ? candidate + 1 : parsing_path;
        if (wcschr(candidate, L'!') != nullptr)
        {
            (void)reach_copy_utf16(application->identity.app_user_model_id, 260,
                                   reinterpret_cast<const uint16_t *>(candidate));
        }
    }
    CoTaskMemFree(parsing_path);

    PWSTR file_path = nullptr;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &file_path)) && file_path != nullptr)
    {
        (void)reach_copy_utf16(application->icon_ref, 260,
                               reinterpret_cast<const uint16_t *>(file_path));
        if (lstrcmpiW(PathFindExtensionW(file_path), L".lnk") == 0)
        {
            application->launch.kind = REACH_APPLICATION_LAUNCH_SHORTCUT;
            (void)reach_copy_utf16(application->launch.path, 260,
                                   reinterpret_cast<const uint16_t *>(file_path));
            reach_windows_shortcut_info shortcut = {};
            if (reach_windows_read_shortcut(file_path, &shortcut) &&
                shortcut.target_path[0] != 0)
            {
                (void)reach_application_identity_add_runtime_path(
                    &application->identity,
                    reinterpret_cast<const uint16_t *>(shortcut.target_path));
                if (application->launch.arguments[0] == 0)
                {
                    (void)reach_copy_utf16(
                        application->launch.arguments, 260,
                        reinterpret_cast<const uint16_t *>(shortcut.arguments));
                }
            }
        }
        else
        {
            application->launch.kind = REACH_APPLICATION_LAUNCH_EXECUTABLE;
            (void)reach_copy_utf16(application->launch.path, 260,
                                   reinterpret_cast<const uint16_t *>(file_path));
            (void)reach_application_identity_add_runtime_path(
                &application->identity, reinterpret_cast<const uint16_t *>(file_path));
        }
    }
    CoTaskMemFree(file_path);

    if (application->launch.path[0] == 0 &&
        application->identity.app_user_model_id[0] != 0)
    {
        application->launch.kind = REACH_APPLICATION_LAUNCH_SHELL;
        int written = swprintf_s(reinterpret_cast<wchar_t *>(application->launch.path), 260,
                                 L"shell:AppsFolder\\%ls",
                                 reinterpret_cast<const wchar_t *>(
                                     application->identity.app_user_model_id));
        if (written <= 0)
        {
            *out_pin = {};
            return REACH_ERROR;
        }
        (void)reach_copy_utf16(application->icon_ref, 260, application->launch.path);
    }

    if (application->launch.path[0] == 0)
    {
        return REACH_ERROR;
    }
    return REACH_OK;
}

static int32_t reach_taskbar_add_item(IShellItem2 *item, reach_pinned_app_model *pins,
                                      size_t capacity, size_t *count)
{
    reach_pinned_app_model pin = {};
    if (reach_taskbar_pin_from_item(item, &pin) != REACH_OK)
    {
        return 0;
    }
    for (size_t index = 0; index < *count; ++index)
    {
        if (reach_taskbar_pin_same(&pins[index], &pin))
        {
            return 1;
        }
    }
    if (*count >= capacity)
    {
        return 1;
    }
    pin.id = (uint32_t)(*count + 1);
    pins[*count] = pin;
    *count += 1;
    return 1;
}

static int32_t reach_taskbar_aumid_character(BYTE value)
{
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
           (value >= '0' && value <= '9') || value == '.' || value == '_' || value == '-' ||
           value == '!';
}

static int32_t reach_taskbar_find_embedded_aumid(const BYTE *data, size_t size,
                                                 uint16_t *out_aumid, size_t capacity)
{
    for (size_t offset = 0; offset + 1 < size; ++offset)
    {
        if (data[offset + 1] != 0 || !reach_taskbar_aumid_character(data[offset]))
        {
            continue;
        }
        if (offset >= 2 && data[offset - 1] == 0 &&
            reach_taskbar_aumid_character(data[offset - 2]))
        {
            continue;
        }

        size_t length = 0;
        size_t separator = 0;
        int32_t family_separator = 0;
        while (offset + length * 2 + 1 < size && data[offset + length * 2 + 1] == 0 &&
               reach_taskbar_aumid_character(data[offset + length * 2]))
        {
            if (data[offset + length * 2] == '_' && separator == 0)
            {
                family_separator = 1;
            }
            if (data[offset + length * 2] == '!')
            {
                separator = length + 1;
            }
            length += 1;
        }
        if (!family_separator || separator == 0 || separator >= length || length >= capacity)
        {
            continue;
        }
        for (size_t index = 0; index < length; ++index)
        {
            out_aumid[index] = data[offset + index * 2];
        }
        out_aumid[length] = 0;
        return 1;
    }
    return 0;
}

static int32_t reach_taskbar_add_embedded_aumid(const BYTE *data, size_t size,
                                                reach_pinned_app_model *pins, size_t capacity,
                                                size_t *count)
{
    reach_pinned_app_model pin = {};
    if (!reach_taskbar_find_embedded_aumid(
            data, size, pin.application.identity.app_user_model_id, 260))
    {
        return 0;
    }
    pin.application.launch.kind = REACH_APPLICATION_LAUNCH_PACKAGED;
    if (swprintf_s(reinterpret_cast<wchar_t *>(pin.application.launch.path), 260,
                   L"shell:AppsFolder\\%ls",
                   reinterpret_cast<const wchar_t *>(
                       pin.application.identity.app_user_model_id)) <= 0)
    {
        return 0;
    }
    (void)reach_copy_utf16(pin.application.icon_ref, 260, pin.application.launch.path);
    for (size_t index = 0; index < *count; ++index)
    {
        if (reach_taskbar_pin_same(&pins[index], &pin))
        {
            return 1;
        }
    }
    if (*count < capacity)
    {
        pin.id = (uint32_t)(*count + 1);
        pins[*count] = pin;
        *count += 1;
    }
    return 1;
}

static reach_result reach_taskbar_read_favorites(std::vector<BYTE> *out_data)
{
    HKEY key = nullptr;
    LONG status = RegOpenKeyExW(HKEY_CURRENT_USER,
                                L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Taskband",
                                0, KEY_QUERY_VALUE, &key);
    if (status != ERROR_SUCCESS)
    {
        return REACH_ERROR;
    }

    DWORD type = 0;
    DWORD size = 0;
    status = RegQueryValueExW(key, L"Favorites", nullptr, &type, nullptr, &size);
    if (status != ERROR_SUCCESS || type != REG_BINARY || size == 0 || size > 1024 * 1024)
    {
        RegCloseKey(key);
        return REACH_ERROR;
    }

    try
    {
        out_data->resize(size);
    }
    catch (const std::bad_alloc &)
    {
        RegCloseKey(key);
        return REACH_ERROR;
    }

    status = RegQueryValueExW(key, L"Favorites", nullptr, &type, out_data->data(), &size);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS || type != REG_BINARY)
    {
        out_data->clear();
        return REACH_ERROR;
    }
    out_data->resize(size);
    return REACH_OK;
}

struct reach_taskbar_favorites_decode
{
    reach_pinned_app_model *pins;
    size_t capacity;
    size_t count;
};

static int32_t reach_taskbar_decode_favorite(const uint8_t *pidl, size_t size, void *user)
{
    reach_taskbar_favorites_decode *decode =
        static_cast<reach_taskbar_favorites_decode *>(user);
    int32_t added = reach_taskbar_add_embedded_aumid(
        pidl, size, decode->pins, decode->capacity, &decode->count);
    if (added)
    {
        return 1;
    }

    IShellItem2 *item = nullptr;
    HRESULT hr = SHCreateItemFromIDList(reinterpret_cast<PCIDLIST_ABSOLUTE>(pidl),
                                        IID_PPV_ARGS(&item));
    if (SUCCEEDED(hr) && item != nullptr)
    {
        (void)reach_taskbar_add_item(item, decode->pins, decode->capacity, &decode->count);
        item->Release();
    }
    return 1;
}

static reach_result reach_taskbar_collect_favorites(reach_pinned_app_model *pins, size_t capacity,
                                                    size_t *out_count)
{
    std::vector<BYTE> data;
    if (reach_taskbar_read_favorites(&data) != REACH_OK || data.size() < 2)
    {
        return REACH_ERROR;
    }

    reach_pinned_app_model decoded[REACH_MAX_PINNED_APPS] = {};
    reach_taskbar_favorites_decode decode = {};
    decode.pins = decoded;
    decode.capacity = REACH_MAX_PINNED_APPS;
    reach_result result =
        reach_taskbar_pin_blob_visit(data.data(), data.size(), reach_taskbar_decode_favorite,
                                     &decode);
    if (result != REACH_OK)
    {
        return result;
    }
    size_t copy_count = decode.count < capacity ? decode.count : capacity;
    for (size_t index = 0; index < copy_count; ++index)
    {
        pins[index] = decoded[index];
    }
    *out_count = copy_count;
    return REACH_OK;
}

static reach_result reach_taskbar_collect_folder(reach_pinned_app_model *pins, size_t capacity,
                                                 size_t *out_count)
{
    *out_count = 0;
    PWSTR user_pinned = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_UserPinned, KF_FLAG_DEFAULT, nullptr, &user_pinned);
    if (FAILED(hr) || user_pinned == nullptr)
    {
        CoTaskMemFree(user_pinned);
        return REACH_ERROR;
    }

    wchar_t pattern[MAX_PATH] = {};
    wcscpy_s(pattern, user_pinned);
    CoTaskMemFree(user_pinned);
    if (!PathAppendW(pattern, L"TaskBar") || !PathAppendW(pattern, L"*.lnk"))
    {
        return REACH_ERROR;
    }

    WIN32_FIND_DATAW found = {};
    HANDLE find = FindFirstFileW(pattern, &found);
    if (find == INVALID_HANDLE_VALUE)
    {
        return GetLastError() == ERROR_FILE_NOT_FOUND ? REACH_OK : REACH_ERROR;
    }

    PathRemoveFileSpecW(pattern);
    do
    {
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            continue;
        }
        wchar_t path[MAX_PATH] = {};
        wcscpy_s(path, pattern);
        if (!PathAppendW(path, found.cFileName))
        {
            continue;
        }
        IShellItem2 *item = nullptr;
        hr = SHCreateItemFromParsingName(path, nullptr, IID_PPV_ARGS(&item));
        if (SUCCEEDED(hr) && item != nullptr)
        {
            (void)reach_taskbar_add_item(item, pins, capacity, out_count);
            item->Release();
        }
    } while (FindNextFileW(find, &found));
    FindClose(find);
    return REACH_OK;
}

reach_result reach_windows_collect_taskbar_pins(reach_pinned_app_model *out_pins,
                                                size_t capacity, size_t *out_count)
{
    if (out_pins == nullptr || capacity == 0 || out_count == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_count = 0;
    HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com_result) && com_result != RPC_E_CHANGED_MODE)
    {
        return REACH_ERROR;
    }

    reach_result result = reach_taskbar_collect_favorites(out_pins, capacity, out_count);
    if (result != REACH_OK)
    {
        result = reach_taskbar_collect_folder(out_pins, capacity, out_count);
    }
    if (SUCCEEDED(com_result))
    {
        CoUninitialize();
    }
    return result;
}
