#include "reachctl_context_menu.h"

#include "reach/ports/config_store.h"

#include <windows.h>
#include <cwchar>
#include <stddef.h>

static const size_t REACHCTL_WALLPAPER_CONTEXT_MONITOR_COUNT = 4;

static reach_result reachctl_write_string_value(HKEY root, const wchar_t *key, const wchar_t *name,
                                                const wchar_t *value)
{
    HKEY handle = nullptr;
    LONG status =
        RegCreateKeyExW(root, key, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &handle, nullptr);
    if (status != ERROR_SUCCESS)
    {
        return REACH_ERROR;
    }

    status = RegSetValueExW(handle, name, 0, REG_SZ, reinterpret_cast<const BYTE *>(value),
                            (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));

    RegCloseKey(handle);
    return status == ERROR_SUCCESS ? REACH_OK : REACH_ERROR;
}

static reach_result reachctl_delete_tree(HKEY root, const wchar_t *key)
{
    LONG status = RegDeleteTreeW(root, key);
    return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND ? REACH_OK : REACH_ERROR;
}

static void reachctl_escape_powershell_single_quoted(wchar_t *dst, size_t dst_count,
                                                     const wchar_t *src)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }

    dst[0] = 0;
    if (src == nullptr)
    {
        return;
    }

    size_t write = 0;
    for (size_t read = 0; src[read] != 0 && write + 1 < dst_count; ++read)
    {
        if (src[read] == L'\'')
        {
            if (write + 2 >= dst_count)
            {
                break;
            }

            dst[write++] = L'\'';
            dst[write++] = L'\'';
        }
        else
        {
            dst[write++] = src[read];
        }
    }

    dst[write] = 0;
}

static reach_result reachctl_install_pin_context_menu_for_class(const wchar_t *class_name,
                                                                const wchar_t *reachctl_path,
                                                                const wchar_t *icon_path)
{
    if (class_name == nullptr || reachctl_path == nullptr || icon_path == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    wchar_t escaped_reachctl_path[1024] = {};
    reachctl_escape_powershell_single_quoted(escaped_reachctl_path, 1024, reachctl_path);

    wchar_t verb_key[512] = {};
    swprintf_s(verb_key, L"Software\\Classes\\%ls\\shell\\ReachPin", class_name);

    wchar_t command_key[512] = {};
    swprintf_s(command_key, L"Software\\Classes\\%ls\\shell\\ReachPin\\command", class_name);

    wchar_t command[1536] = {};
    swprintf_s(command,
               L"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden "
               L"-Command \"Start-Process -WindowStyle Hidden -FilePath '%ls' "
               L"-ArgumentList '--pin \\\"%%1\\\"'\"",
               escaped_reachctl_path);

    reach_result result =
        reachctl_write_string_value(HKEY_CURRENT_USER, verb_key, L"MUIVerb", L"Pin to Reach dock");

    if (result != REACH_OK)
    {
        return result;
    }

    result = reachctl_write_string_value(HKEY_CURRENT_USER, verb_key, L"Icon", icon_path);

    if (result != REACH_OK)
    {
        return result;
    }

    return reachctl_write_string_value(HKEY_CURRENT_USER, command_key, L"", command);
}

static reach_result reachctl_install_pin_context_menu(const wchar_t *reachctl_path,
                                                      const wchar_t *icon_path)
{
    reach_result exe_result =
        reachctl_install_pin_context_menu_for_class(L"exefile", reachctl_path, icon_path);

    reach_result lnk_result =
        reachctl_install_pin_context_menu_for_class(L"lnkfile", reachctl_path, icon_path);

    return exe_result == REACH_OK && lnk_result == REACH_OK ? REACH_OK : REACH_ERROR;
}

static reach_result reachctl_remove_pin_context_menu(void)
{
    reach_result exe_result =
        reachctl_delete_tree(HKEY_CURRENT_USER, L"Software\\Classes\\exefile\\shell\\ReachPin");

    reach_result lnk_result =
        reachctl_delete_tree(HKEY_CURRENT_USER, L"Software\\Classes\\lnkfile\\shell\\ReachPin");

    return exe_result == REACH_OK && lnk_result == REACH_OK ? REACH_OK : REACH_ERROR;
}

static reach_result reachctl_install_wallpaper_monitor_context_menu_at_key(
    const wchar_t *root_key, const wchar_t *reachctl_path, const wchar_t *icon_path)
{
    if (root_key == nullptr || reachctl_path == nullptr || icon_path == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    wchar_t escaped_reachctl_path[1024] = {};
    reachctl_escape_powershell_single_quoted(escaped_reachctl_path, 1024, reachctl_path);

    reach_result result = reachctl_write_string_value(HKEY_CURRENT_USER, root_key, L"MUIVerb",
                                                      L"Set as Reach wallpaper for monitor");

    if (result != REACH_OK)
    {
        return result;
    }

    result = reachctl_write_string_value(HKEY_CURRENT_USER, root_key, L"Icon", icon_path);

    if (result != REACH_OK)
    {
        return result;
    }

    result = reachctl_write_string_value(HKEY_CURRENT_USER, root_key, L"SubCommands", L"");

    if (result != REACH_OK)
    {
        return result;
    }

    size_t monitor_count = REACHCTL_WALLPAPER_CONTEXT_MONITOR_COUNT;
    if (monitor_count > REACH_MAX_WALLPAPER_MONITORS)
    {
        monitor_count = REACH_MAX_WALLPAPER_MONITORS;
    }

    for (size_t monitor_index = 0; monitor_index < monitor_count; ++monitor_index)
    {
        wchar_t monitor_key[512] = {};
        swprintf_s(monitor_key, L"%ls\\shell\\Monitor%zu", root_key, monitor_index + 1);

        wchar_t label[64] = {};
        swprintf_s(label, L"Monitor %zu", monitor_index + 1);

        result = reachctl_write_string_value(HKEY_CURRENT_USER, monitor_key, L"MUIVerb", label);

        if (result != REACH_OK)
        {
            return result;
        }

        wchar_t command_key[512] = {};
        swprintf_s(command_key, L"%ls\\shell\\Monitor%zu\\command", root_key, monitor_index + 1);

        wchar_t command[1536] = {};
        swprintf_s(command,
                   L"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden "
                   L"-Command \"Start-Process -WindowStyle Hidden -FilePath '%ls' "
                   L"-ArgumentList '--wallpaper-monitor %zu \\\"%%1\\\"'\"",
                   escaped_reachctl_path, monitor_index);

        result = reachctl_write_string_value(HKEY_CURRENT_USER, command_key, L"", command);

        if (result != REACH_OK)
        {
            return result;
        }
    }

    return REACH_OK;
}

static reach_result reachctl_install_wallpaper_monitor_context_menu(const wchar_t *reachctl_path,
                                                                    const wchar_t *icon_path)
{
    return reachctl_install_wallpaper_monitor_context_menu_at_key(
        L"Software\\Classes\\SystemFileAssociations\\image\\shell\\ReachWallpaperMonitor",
        reachctl_path, icon_path);
}

static reach_result reachctl_remove_wallpaper_monitor_context_menu(void)
{
    reach_result image_result = reachctl_delete_tree(
        HKEY_CURRENT_USER,
        L"Software\\Classes\\SystemFileAssociations\\image\\shell\\ReachWallpaperMonitor");

    const wchar_t *old_extensions[] = {L".jpg", L".jpeg", L".png", L".bmp", L".webp"};

    reach_result final_result = image_result;

    for (size_t index = 0; index < sizeof(old_extensions) / sizeof(old_extensions[0]); ++index)
    {
        wchar_t key[512] = {};
        swprintf_s(key, L"Software\\Classes\\%ls\\shell\\ReachWallpaperMonitor",
                   old_extensions[index]);

        reach_result result = reachctl_delete_tree(HKEY_CURRENT_USER, key);
        if (result != REACH_OK)
        {
            final_result = REACH_ERROR;
        }
    }

    return final_result;
}

reach_result reachctl_install_context_menus(const uint16_t *reachctl_path,
                                            const uint16_t *reach_icon_path)
{
    if (reachctl_path == nullptr || reachctl_path[0] == 0 || reach_icon_path == nullptr ||
        reach_icon_path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const wchar_t *reachctl_path_w = reinterpret_cast<const wchar_t *>(reachctl_path);
    const wchar_t *reach_icon_path_w = reinterpret_cast<const wchar_t *>(reach_icon_path);

    reach_result pin_result = reachctl_install_pin_context_menu(reachctl_path_w, reach_icon_path_w);

    if (pin_result != REACH_OK)
    {
        return pin_result;
    }

    return reachctl_install_wallpaper_monitor_context_menu(reachctl_path_w, reach_icon_path_w);
}

reach_result reachctl_remove_context_menus(void)
{
    reach_result pin_result = reachctl_remove_pin_context_menu();
    reach_result wallpaper_result = reachctl_remove_wallpaper_monitor_context_menu();

    return pin_result == REACH_OK && wallpaper_result == REACH_OK ? REACH_OK : REACH_ERROR;
}
