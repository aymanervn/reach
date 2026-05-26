#include "windows_adapters_internal.h"

#include "reach/ports/config_store.h"

#include <windows.h>
#include <shlwapi.h>

#include <stdio.h>
#include <new>

struct reach_config_store {
    uint16_t path[260];
};

static void reach_config_resolve_path(reach_config_store *store, uint16_t *path, size_t path_count)
{
    if (store == nullptr || path == nullptr || path_count == 0 || path[0] == 0) {
        return;
    }

    wchar_t *path_w = reinterpret_cast<wchar_t *>(path);
    if (!PathIsRelativeW(path_w)) {
        return;
    }

    wchar_t base[260] = {};
    reach_copy_utf16(reinterpret_cast<uint16_t *>(base), 260, store->path);
    if (!PathRemoveFileSpecW(base)) {
        return;
    }
    if (!PathAppendW(base, path_w)) {
        return;
    }
    reach_copy_utf16(path, path_count, reinterpret_cast<const uint16_t *>(base));
}

static void reach_config_resolve_wallpaper_paths(reach_config_store *store, reach_config_snapshot *snapshot)
{
    if (store == nullptr || snapshot == nullptr) {
        return;
    }

    reach_config_resolve_path(store, snapshot->wallpaper_path, 260);
    for (size_t index = 0; index < REACH_MAX_WALLPAPER_MONITORS; ++index) {
        reach_config_resolve_path(store, snapshot->monitor_wallpaper_paths[index], 260);
    }
}

static reach_result reach_config_store_load(reach_config_store *store, reach_config_snapshot *out_snapshot)
{
    REACH_ASSERT(store != nullptr);
    REACH_ASSERT(out_snapshot != nullptr);
    if (store == nullptr || out_snapshot == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_snapshot = {};
    const wchar_t *path = reinterpret_cast<const wchar_t *>(store->path);
    out_snapshot->dock_height = (float)GetPrivateProfileIntW(L"dock", L"height", 64, path);
    out_snapshot->dock_width = (float)GetPrivateProfileIntW(L"dock", L"width", 560, path);
    out_snapshot->dock_icon_size = (float)GetPrivateProfileIntW(L"dock", L"icon_size", 40, path);
    GetPrivateProfileStringW(L"wallpaper", L"path", L"", reinterpret_cast<wchar_t *>(out_snapshot->wallpaper_path), 260, path);
    for (size_t index = 0; index < REACH_MAX_WALLPAPER_MONITORS; ++index) {
        wchar_t section[48] = {};
        swprintf_s(section, L"wallpaper.monitor.%u", (unsigned)(index + 1));
        GetPrivateProfileStringW(section, L"path", L"", reinterpret_cast<wchar_t *>(out_snapshot->monitor_wallpaper_paths[index]), 260, path);
    }
    reach_config_resolve_wallpaper_paths(store, out_snapshot);

    for (size_t index = 0; index < REACH_MAX_PINNED_APPS; ++index) {
        wchar_t section[32] = {};
        swprintf_s(section, L"pinned.%u", (unsigned)index);
        wchar_t title[128] = {};
        GetPrivateProfileStringW(section, L"title", L"", title, 128, path);
        if (title[0] == 0) {
            continue;
        }

        reach_pinned_app_model *app = &out_snapshot->pinned_apps[out_snapshot->pinned_app_count];
        app->id = (uint32_t)(out_snapshot->pinned_app_count + 1);
        reach_copy_utf16(app->title, 128, reinterpret_cast<const uint16_t *>(title));
        GetPrivateProfileStringW(section, L"path", L"", reinterpret_cast<wchar_t *>(app->path), 260, path);
        GetPrivateProfileStringW(section, L"arguments", L"", reinterpret_cast<wchar_t *>(app->arguments), 260, path);
        GetPrivateProfileStringW(section, L"icon", L"", reinterpret_cast<wchar_t *>(app->icon_ref), 260, path);
        GetPrivateProfileStringW(section, L"app_user_model_id", L"", reinterpret_cast<wchar_t *>(app->app_user_model_id), 260, path);
        out_snapshot->pinned_app_count += 1;
    }

    return REACH_OK;
}

static reach_result reach_config_store_save(reach_config_store *store, const reach_config_snapshot *snapshot)
{
    REACH_ASSERT(store != nullptr);
    REACH_ASSERT(snapshot != nullptr);
    if (store == nullptr || snapshot == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    const wchar_t *path = reinterpret_cast<const wchar_t *>(store->path);
    wchar_t value[32] = {};
    swprintf_s(value, L"%.0f", snapshot->dock_height);
    WritePrivateProfileStringW(L"dock", L"height", value, path);
    swprintf_s(value, L"%.0f", snapshot->dock_width);
    WritePrivateProfileStringW(L"dock", L"width", value, path);
    swprintf_s(value, L"%.0f", snapshot->dock_icon_size);
    WritePrivateProfileStringW(L"dock", L"icon_size", value, path);
    WritePrivateProfileStringW(L"wallpaper", L"path", reinterpret_cast<const wchar_t *>(snapshot->wallpaper_path), path);
    for (size_t index = 0; index < REACH_MAX_WALLPAPER_MONITORS; ++index) {
        wchar_t section[48] = {};
        swprintf_s(section, L"wallpaper.monitor.%u", (unsigned)(index + 1));
        if (snapshot->monitor_wallpaper_paths[index][0] != 0) {
            WritePrivateProfileStringW(section, L"path", reinterpret_cast<const wchar_t *>(snapshot->monitor_wallpaper_paths[index]), path);
        } else {
            WritePrivateProfileStringW(section, nullptr, nullptr, path);
        }
    }

    for (size_t index = 0; index < snapshot->pinned_app_count && index < REACH_MAX_PINNED_APPS; ++index) {
        wchar_t section[32] = {};
        swprintf_s(section, L"pinned.%u", (unsigned)index);
        const reach_pinned_app_model *app = &snapshot->pinned_apps[index];
        WritePrivateProfileStringW(section, L"title", reinterpret_cast<const wchar_t *>(app->title), path);
        WritePrivateProfileStringW(section, L"path", reinterpret_cast<const wchar_t *>(app->path), path);
        WritePrivateProfileStringW(section, L"arguments", reinterpret_cast<const wchar_t *>(app->arguments), path);
        WritePrivateProfileStringW(section, L"icon", reinterpret_cast<const wchar_t *>(app->icon_ref), path);
        WritePrivateProfileStringW(section, L"app_user_model_id", reinterpret_cast<const wchar_t *>(app->app_user_model_id), path);
    }
    for (size_t index = snapshot->pinned_app_count; index < REACH_MAX_PINNED_APPS; ++index) {
        wchar_t section[32] = {};
        swprintf_s(section, L"pinned.%u", (unsigned)index);
        WritePrivateProfileStringW(section, nullptr, nullptr, path);
    }

    return REACH_OK;
}

static void reach_config_store_destroy(reach_config_store *store)
{
    delete store;
}

reach_result reach_windows_create_config_store(const uint16_t *path, reach_config_store_port *out_port)
{
    REACH_ASSERT(path != nullptr);
    REACH_ASSERT(out_port != nullptr);
    if (path == nullptr || out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_config_store *store = new (std::nothrow) reach_config_store();
    if (store == nullptr) {
        return REACH_ERROR;
    }

    reach_copy_utf16(store->path, 260, path);
    out_port->store = store;
    out_port->ops.load = reach_config_store_load;
    out_port->ops.save = reach_config_store_save;
    out_port->ops.destroy = reach_config_store_destroy;
    return REACH_OK;
}
