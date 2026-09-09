#include "windows_adapters_internal.h"

#include "reach/core/installed_apps.h"

#include <windows.h>
#include <propsys.h>
#include <propkey.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Management.Deployment.h>
#include <winrt/base.h>

#include <algorithm>
#include <cwctype>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

struct reach_installed_apps
{
};

struct reach_windows_package_info
{
    std::wstring family_name;
    std::wstring full_name;
    std::wstring publisher;
    std::wstring version;
};

struct reach_windows_installed_apps_com_scope
{
    HRESULT result;
    reach_windows_installed_apps_com_scope() : result(CoInitializeEx(nullptr, COINIT_MULTITHREADED))
    {
    }
    ~reach_windows_installed_apps_com_scope()
    {
        if (SUCCEEDED(result))
        {
            CoUninitialize();
        }
    }
};

static std::wstring reach_windows_lower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t ch) { return (wchar_t)towlower(ch); });
    return value;
}

static std::wstring
reach_windows_package_version(const winrt::Windows::ApplicationModel::PackageVersion &version)
{
    wchar_t text[64] = {};
    _snwprintf_s(text, 64, _TRUNCATE, L"%u.%u.%u.%u", version.Major, version.Minor, version.Build,
                 version.Revision);
    return text;
}

static std::unordered_map<std::wstring, reach_windows_package_info>
reach_windows_read_packages(void)
{
    std::unordered_map<std::wstring, reach_windows_package_info> packages;
    try
    {
        winrt::Windows::Management::Deployment::PackageManager manager;
        for (const auto &package : manager.FindPackagesForUser(L""))
        {
            const auto id = package.Id();
            reach_windows_package_info info = {};
            info.family_name = id.FamilyName().c_str();
            info.full_name = id.FullName().c_str();
            info.publisher = package.PublisherDisplayName().c_str();
            info.version = reach_windows_package_version(id.Version());
            packages.emplace(reach_windows_lower(info.family_name), std::move(info));
        }
    }
    catch (...)
    {
    }
    return packages;
}

static int32_t reach_windows_shell_item_string(IShellItem2 *item, REFPROPERTYKEY key,
                                               uint16_t *out_value, size_t capacity)
{
    PWSTR value = nullptr;
    HRESULT hr = item->GetString(key, &value);
    if (FAILED(hr) || value == nullptr || value[0] == 0)
    {
        CoTaskMemFree(value);
        return 0;
    }
    reach_copy_utf16(out_value, capacity, reinterpret_cast<const uint16_t *>(value));
    CoTaskMemFree(value);
    return 1;
}

static int32_t reach_windows_shell_item_display_name(IShellItem *item, SIGDN format,
                                                     uint16_t *out_value, size_t capacity)
{
    PWSTR value = nullptr;
    HRESULT hr = item->GetDisplayName(format, &value);
    if (FAILED(hr) || value == nullptr || value[0] == 0)
    {
        CoTaskMemFree(value);
        return 0;
    }
    reach_copy_utf16(out_value, capacity, reinterpret_cast<const uint16_t *>(value));
    CoTaskMemFree(value);
    return 1;
}

static int32_t reach_windows_shell_item_has_verb(IShellItem *item, const wchar_t *expected)
{
    if (item == nullptr || expected == nullptr)
    {
        return 0;
    }

    PIDLIST_ABSOLUTE item_id_list = nullptr;
    if (FAILED(SHGetIDListFromObject(item, &item_id_list)) || item_id_list == nullptr)
    {
        return 0;
    }

    IShellFolder *parent = nullptr;
    PCUITEMID_CHILD child = nullptr;
    HRESULT hr = SHBindToParent(item_id_list, IID_PPV_ARGS(&parent), &child);
    IContextMenu *context_menu = nullptr;
    if (SUCCEEDED(hr) && parent != nullptr && child != nullptr)
    {
        hr = parent->GetUIObjectOf(nullptr, 1, &child, IID_IContextMenu, nullptr,
                                   reinterpret_cast<void **>(&context_menu));
    }
    if (parent != nullptr)
    {
        parent->Release();
    }
    CoTaskMemFree(item_id_list);
    if (FAILED(hr) || context_menu == nullptr)
    {
        return 0;
    }

    HMENU menu = CreatePopupMenu();
    if (menu == nullptr)
    {
        context_menu->Release();
        return 0;
    }

    int32_t found = 0;
    hr = context_menu->QueryContextMenu(menu, 0, 1, 0x7fff, CMF_NORMAL);
    if (SUCCEEDED(hr))
    {
        int item_count = GetMenuItemCount(menu);
        for (int position = 0; position < item_count; ++position)
        {
            MENUITEMINFOW item = {};
            item.cbSize = sizeof(item);
            item.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STATE;
            if (!GetMenuItemInfoW(menu, static_cast<UINT>(position), TRUE, &item) ||
                (item.fType & MFT_SEPARATOR) != 0 || (item.fState & MFS_DISABLED) != 0 ||
                item.wID < 1)
            {
                continue;
            }
            wchar_t verb[128] = {};
            if (SUCCEEDED(context_menu->GetCommandString(item.wID - 1, GCS_VERBW, nullptr,
                                                         reinterpret_cast<char *>(verb),
                                                         static_cast<UINT>(_countof(verb)))) &&
                _wcsicmp(verb, expected) == 0)
            {
                found = 1;
                break;
            }
        }
    }
    DestroyMenu(menu);
    context_menu->Release();
    return found;
}

static void reach_windows_fill_package_fields(
    reach_installed_app *entry,
    const std::unordered_map<std::wstring, reach_windows_package_info> &packages)
{
    const wchar_t *aumid = reinterpret_cast<const wchar_t *>(entry->app_user_model_id);
    const wchar_t *separator = wcschr(aumid, L'!');
    if (separator == nullptr)
    {
        return;
    }
    std::wstring family(aumid, separator);
    auto found = packages.find(reach_windows_lower(family));
    if (found == packages.end())
    {
        return;
    }
    const reach_windows_package_info &package = found->second;
    reach_copy_utf16(entry->package_family_name, REACH_INSTALLED_APP_TEXT_CAPACITY,
                     reinterpret_cast<const uint16_t *>(package.family_name.c_str()));
    reach_copy_utf16(entry->package_full_name, REACH_INSTALLED_APP_TEXT_CAPACITY,
                     reinterpret_cast<const uint16_t *>(package.full_name.c_str()));
    reach_copy_utf16(entry->publisher, REACH_INSTALLED_APP_NAME_CAPACITY,
                     reinterpret_cast<const uint16_t *>(package.publisher.c_str()));
    reach_copy_utf16(entry->version, 64,
                     reinterpret_cast<const uint16_t *>(package.version.c_str()));
    entry->kind = REACH_INSTALLED_APP_PACKAGED;
    entry->can_manage = 1;
}

reach_result reach_windows_collect_installed_apps(reach_installed_app_list *out_list)
{
    if (out_list == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    memset(out_list, 0, sizeof(*out_list));
    reach_windows_installed_apps_com_scope com_scope;
    if (FAILED(com_scope.result) && com_scope.result != RPC_E_CHANGED_MODE)
    {
        return REACH_ERROR;
    }

    IShellItem *folder = nullptr;
    HRESULT hr = SHCreateItemFromParsingName(L"shell:AppsFolder", nullptr, IID_PPV_ARGS(&folder));
    if (FAILED(hr) || folder == nullptr)
    {
        return REACH_ERROR;
    }

    IEnumShellItems *items = nullptr;
    hr = folder->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&items));
    folder->Release();
    if (FAILED(hr) || items == nullptr)
    {
        return REACH_ERROR;
    }

    const auto packages = reach_windows_read_packages();
    std::vector<reach_installed_app> found;
    IShellItem *item = nullptr;
    while (items->Next(1, &item, nullptr) == S_OK)
    {
        IShellItem2 *item2 = nullptr;
        if (SUCCEEDED(item->QueryInterface(IID_PPV_ARGS(&item2))) && item2 != nullptr)
        {
            reach_installed_app entry = {};
            entry.kind = REACH_INSTALLED_APP_DESKTOP;
            if (reach_windows_shell_item_display_name(item, SIGDN_NORMALDISPLAY, entry.display_name,
                                                      REACH_INSTALLED_APP_NAME_CAPACITY) &&
                reach_windows_shell_item_string(item2, PKEY_AppUserModel_ID,
                                                entry.app_user_model_id,
                                                REACH_INSTALLED_APP_TEXT_CAPACITY))
            {
                _snwprintf_s(reinterpret_cast<wchar_t *>(entry.launch_path),
                             REACH_INSTALLED_APP_TEXT_CAPACITY, _TRUNCATE, L"shell:AppsFolder\\%s",
                             reinterpret_cast<const wchar_t *>(entry.app_user_model_id));
                reach_copy_utf16(entry.icon_ref, REACH_INSTALLED_APP_TEXT_CAPACITY,
                                 entry.launch_path);
                entry.can_open = 1;
                reach_windows_fill_package_fields(&entry, packages);
                if (entry.kind == REACH_INSTALLED_APP_PACKAGED)
                {
                    entry.can_uninstall = reach_windows_shell_item_has_verb(item, L"uninstall");
                }
                found.push_back(entry);
            }
            item2->Release();
        }
        item->Release();
        item = nullptr;
    }
    items->Release();

    std::sort(found.begin(), found.end(),
              [](const reach_installed_app &a, const reach_installed_app &b)
              {
                  return _wcsicmp(reinterpret_cast<const wchar_t *>(a.display_name),
                                  reinterpret_cast<const wchar_t *>(b.display_name)) < 0;
              });
    out_list->count = found.size() < REACH_INSTALLED_APP_MAX_ENTRIES
                          ? found.size()
                          : REACH_INSTALLED_APP_MAX_ENTRIES;
    for (size_t index = 0; index < out_list->count; ++index)
    {
        out_list->entries[index] = found[index];
    }
    return REACH_OK;
}

static reach_result reach_windows_installed_apps_enumerate(reach_installed_apps *,
                                                           reach_installed_app_list *out_list)
{
    return reach_windows_collect_installed_apps(out_list);
}

static reach_result reach_windows_installed_apps_uninstall(reach_installed_apps *,
                                                           const reach_installed_app *entry)
{
    if (entry == nullptr || entry->package_full_name[0] == 0)
    {
        return REACH_NOT_IMPLEMENTED;
    }
    try
    {
        winrt::Windows::Management::Deployment::PackageManager manager;
        auto result =
            manager.RemovePackageAsync(reinterpret_cast<const wchar_t *>(entry->package_full_name))
                .get();
        return result.ExtendedErrorCode().value < 0 ? REACH_ERROR : REACH_OK;
    }
    catch (...)
    {
        return REACH_ERROR;
    }
}

static reach_result reach_windows_installed_apps_manage(reach_installed_apps *,
                                                        const reach_installed_app *entry)
{
    if (entry == nullptr || entry->package_family_name[0] == 0)
    {
        return REACH_NOT_IMPLEMENTED;
    }
    wchar_t uri[REACH_INSTALLED_APP_TEXT_CAPACITY + 40] = {};
    _snwprintf_s(uri, _countof(uri), _TRUNCATE, L"ms-settings:appsfeatures-app?%s",
                 reinterpret_cast<const wchar_t *>(entry->package_family_name));
    HINSTANCE result = ShellExecuteW(nullptr, L"open", uri, nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32 ? REACH_OK : REACH_ERROR;
}

static void reach_windows_installed_apps_thread_attach(reach_installed_apps *)
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
}

static void reach_windows_installed_apps_thread_detach(reach_installed_apps *)
{
    winrt::uninit_apartment();
}

static void reach_windows_installed_apps_destroy(reach_installed_apps *apps)
{
    delete apps;
}

reach_result reach_windows_create_installed_apps(reach_installed_apps_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_port = {};
    reach_installed_apps *apps = new (std::nothrow) reach_installed_apps();
    if (apps == nullptr)
    {
        return REACH_ERROR;
    }
    out_port->apps = apps;
    out_port->ops.enumerate = reach_windows_installed_apps_enumerate;
    out_port->ops.uninstall = reach_windows_installed_apps_uninstall;
    out_port->ops.manage = reach_windows_installed_apps_manage;
    out_port->ops.thread_attach = reach_windows_installed_apps_thread_attach;
    out_port->ops.thread_detach = reach_windows_installed_apps_thread_detach;
    out_port->ops.destroy = reach_windows_installed_apps_destroy;
    return REACH_OK;
}
