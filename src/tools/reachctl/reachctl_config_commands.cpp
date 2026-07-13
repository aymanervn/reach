#include "reachctl_config_commands.h"

#include "reachctl_common.h"

#include "reach/services/pin_config.h"
#include "reach/ports/config_store.h"

#include <windows.h>
#include <shlwapi.h>
#include <cwchar>

static int32_t reachctl_is_supported_pin_path(const uint16_t *path)
{
    if (path == nullptr || path[0] == 0)
    {
        return 0;
    }

    const wchar_t *path_w = reinterpret_cast<const wchar_t *>(path);
    const wchar_t *extension = PathFindExtensionW(path_w);

    return lstrcmpiW(extension, L".exe") == 0 || lstrcmpiW(extension, L".lnk") == 0;
}

static int32_t reachctl_is_supported_wallpaper_path(const uint16_t *path)
{
    if (path == nullptr || path[0] == 0)
    {
        return 0;
    }

    const wchar_t *extension = PathFindExtensionW(reinterpret_cast<const wchar_t *>(path));

    return lstrcmpiW(extension, L".jpg") == 0 || lstrcmpiW(extension, L".jpeg") == 0 ||
           lstrcmpiW(extension, L".png") == 0 || lstrcmpiW(extension, L".bmp") == 0 ||
           lstrcmpiW(extension, L".webp") == 0;
}

static reach_result reachctl_path_is_already_pinned(reach_config_store_port *store,
                                                    const uint16_t *path, int32_t *out_pinned)
{
    if (store == nullptr || store->ops.load == nullptr || path == nullptr || path[0] == 0 ||
        out_pinned == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_pinned = 0;

    reach_config_snapshot snapshot = {};
    reach_result result = store->ops.load(store->store, &snapshot);
    if (result != REACH_OK)
    {
        return result;
    }

    for (size_t index = 0; index < snapshot.pinned_app_count; ++index)
    {
        if (reachctl_path_equals_ci(snapshot.pinned_apps[index].path, path))
        {
            *out_pinned = 1;
            return REACH_OK;
        }
    }

    return REACH_OK;
}

int reachctl_pin_command(const wchar_t *path)
{
    if (path == nullptr || path[0] == 0)
    {
        reachctl_print(L"--pin requires a path.");
        return 2;
    }

    uint16_t absolute_path[260] = {};
    reach_result path_result =
        reachctl_absolute_path(reinterpret_cast<const uint16_t *>(path), absolute_path, 260);

    if (path_result != REACH_OK || absolute_path[0] == 0)
    {
        reachctl_print(L"Could not resolve path.");
        return 1;
    }

    if (!reachctl_is_supported_pin_path(absolute_path))
    {
        reachctl_print(L"Only .exe and .lnk files can be pinned to Reach.");
        return 1;
    }

    reach_config_store_port store = {};
    reach_result store_result = reachctl_open_config_store(&store);
    if (store_result != REACH_OK)
    {
        reachctl_print(L"Could not open Reach config.");
        return 1;
    }
    int32_t already_pinned = 0;
    reach_result already_result =
        reachctl_path_is_already_pinned(&store, absolute_path, &already_pinned);

    if (already_result != REACH_OK)
    {
        if (store.ops.destroy != nullptr)
        {
            store.ops.destroy(store.store);
        }
        return 1;
    }

    if (already_pinned)
    {
        if (store.ops.destroy != nullptr)
        {
            store.ops.destroy(store.store);
        }
        return 0;
    }
    reach_result pin_result = reach_pin_config_pin_path(&store, absolute_path);

    if (store.ops.destroy != nullptr)
    {
        store.ops.destroy(store.store);
    }

    if (pin_result != REACH_OK)
    {
        reachctl_print(L"Could not pin app to Reach dock.");
        return 1;
    }

    (void)reachctl_notify_config_changed();

    reachctl_print(L"App pinned to Reach dock.");
    return 0;
}

int reachctl_wallpaper_monitor_command(const wchar_t *index_text, const wchar_t *path)
{
    if (index_text == nullptr || index_text[0] == 0 || path == nullptr || path[0] == 0)
    {
        reachctl_print(L"--wallpaper-monitor requires an index and a path.");
        return 2;
    }

    wchar_t *end = nullptr;
    unsigned long monitor_index = wcstoul(index_text, &end, 10);
    if (end == index_text || *end != 0 || monitor_index >= REACH_MAX_WALLPAPER_MONITORS)
    {
        reachctl_print(L"Invalid monitor index.");
        return 1;
    }

    uint16_t absolute_path[260] = {};
    reach_result path_result =
        reachctl_absolute_path(reinterpret_cast<const uint16_t *>(path), absolute_path, 260);

    if (path_result != REACH_OK || absolute_path[0] == 0)
    {
        reachctl_print(L"Could not resolve wallpaper path.");
        return 1;
    }

    if (!reachctl_is_supported_wallpaper_path(absolute_path))
    {
        reachctl_print(L"Only image files can be set as Reach wallpaper.");
        return 1;
    }

    reach_config_store_port store = {};
    reach_result store_result = reachctl_open_config_store(&store);
    if (store_result != REACH_OK || store.ops.load == nullptr || store.ops.save == nullptr)
    {
        reachctl_print(L"Could not open Reach config.");
        return 1;
    }

    reach_config_snapshot snapshot = {};
    reach_result load_result = store.ops.load(store.store, &snapshot);
    if (load_result != REACH_OK)
    {
        if (store.ops.destroy != nullptr)
        {
            store.ops.destroy(store.store);
        }

        reachctl_print(L"Could not read Reach config.");
        return 1;
    }

    uint16_t *target_path = snapshot.monitor_wallpaper_paths[monitor_index];

    if (reachctl_path_equals_ci(target_path, absolute_path))
    {
        if (store.ops.destroy != nullptr)
        {
            store.ops.destroy(store.store);
        }

        reachctl_print(L"Monitor wallpaper is already set in Reach.");
        return 0;
    }

    reach_result copy_result = reach_copy_utf16(target_path, 260, absolute_path);
    if (copy_result != REACH_OK)
    {
        if (store.ops.destroy != nullptr)
        {
            store.ops.destroy(store.store);
        }

        reachctl_print(L"Could not copy wallpaper path.");
        return 1;
    }

    reach_result save_result = store.ops.save(store.store, &snapshot);

    if (store.ops.destroy != nullptr)
    {
        store.ops.destroy(store.store);
    }

    if (save_result != REACH_OK)
    {
        reachctl_print(L"Could not save Reach monitor wallpaper.");
        return 1;
    }

    (void)reachctl_notify_config_changed();

    reachctl_print(L"Reach monitor wallpaper updated.");
    return 0;
}
