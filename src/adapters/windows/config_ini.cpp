#include "windows_adapters_internal.h"

#include "reach/ports/config_store.h"
#include "reach/protocol/version.h"

#include <windows.h>
#include <shlwapi.h>

#include <stdio.h>
#include <new>
#include <string>

#define REACH_CONFIG_VERSION_WIDE_INNER(text) L##text
#define REACH_CONFIG_VERSION_WIDE(text) REACH_CONFIG_VERSION_WIDE_INNER(text)

static const wchar_t reach_config_byte_order_mark = 0xFEFF;

static reach_result reach_config_write_utf16_atomic(const wchar_t *path, const std::wstring &text)
{
    std::wstring temp_path(path);
    temp_path.append(L".tmp");
    HANDLE file = CreateFileW(temp_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return REACH_ERROR;
    }

    std::wstring output;
    output.push_back(reach_config_byte_order_mark);
    output.append(text);

    DWORD written = 0;
    DWORD byte_count = (DWORD)(output.size() * sizeof(wchar_t));
    int32_t ok = WriteFile(file, output.data(), byte_count, &written, nullptr) &&
                 written == byte_count && FlushFileBuffers(file);
    CloseHandle(file);
    if (!ok || !MoveFileExW(temp_path.c_str(), path,
                            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(temp_path.c_str());
        return REACH_ERROR;
    }
    return REACH_OK;
}

struct reach_config_store
{
    uint16_t path[260];
    HANDLE transaction_mutex;
};

static uint64_t reach_config_path_hash(const uint16_t *path)
{
    uint64_t hash = 1469598103934665603ull;
    for (size_t index = 0; path != nullptr && path[index] != 0; ++index)
    {
        uint16_t value = path[index];
        if (value >= 'A' && value <= 'Z')
        {
            value = (uint16_t)(value + ('a' - 'A'));
        }
        hash ^= value;
        hash *= 1099511628211ull;
    }
    return hash;
}

static reach_result reach_config_store_begin_transaction(reach_config_store *store)
{
    if (store == nullptr || store->transaction_mutex == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    DWORD wait = WaitForSingleObject(store->transaction_mutex, INFINITE);
    return wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED ? REACH_OK : REACH_ERROR;
}

static void reach_config_store_end_transaction(reach_config_store *store)
{
    if (store != nullptr && store->transaction_mutex != nullptr)
    {
        ReleaseMutex(store->transaction_mutex);
    }
}

static void reach_config_resolve_path(reach_config_store *store, uint16_t *path, size_t path_count)
{
    if (store == nullptr || path == nullptr || path_count == 0 || path[0] == 0)
    {
        return;
    }

    wchar_t *path_w = reinterpret_cast<wchar_t *>(path);
    if (!PathIsRelativeW(path_w))
    {
        return;
    }

    wchar_t base[260] = {};
    reach_copy_utf16(reinterpret_cast<uint16_t *>(base), 260, store->path);
    if (!PathRemoveFileSpecW(base))
    {
        return;
    }
    if (!PathAppendW(base, path_w))
    {
        return;
    }
    reach_copy_utf16(path, path_count, reinterpret_cast<const uint16_t *>(base));
}

static void reach_config_resolve_wallpaper_paths(reach_config_store *store,
                                                 reach_config_snapshot *snapshot)
{
    if (store == nullptr || snapshot == nullptr)
    {
        return;
    }

    reach_config_resolve_path(store, snapshot->wallpaper_path, 260);
    for (size_t index = 0; index < REACH_MAX_WALLPAPER_MONITORS; ++index)
    {
        reach_config_resolve_path(store, snapshot->monitor_wallpaper_paths[index], 260);
    }
}

static reach_result reach_config_store_load(reach_config_store *store,
                                            reach_config_snapshot *out_snapshot)
{
    REACH_ASSERT(store != nullptr);
    REACH_ASSERT(out_snapshot != nullptr);
    if (store == nullptr || out_snapshot == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_snapshot = {};
    const wchar_t *path = reinterpret_cast<const wchar_t *>(store->path);
    GetPrivateProfileStringW(L"reach", L"version", L"",
                             reinterpret_cast<wchar_t *>(out_snapshot->version), 32, path);
    out_snapshot->dock_height = (float)GetPrivateProfileIntW(L"dock", L"height", 64, path);
    out_snapshot->power_screen_off_minutes =
        (int32_t)GetPrivateProfileIntW(L"power", L"screen_off_minutes", 10, path);
    out_snapshot->power_sleep_minutes =
        (int32_t)GetPrivateProfileIntW(L"power", L"sleep_minutes", 30, path);
    out_snapshot->power_lock_minutes =
        (int32_t)GetPrivateProfileIntW(L"power", L"lock_minutes", 15, path);
    out_snapshot->power_shutdown_minutes =
        (int32_t)GetPrivateProfileIntW(L"power", L"shutdown_minutes", 0, path);
    out_snapshot->power_restart_minutes =
        (int32_t)GetPrivateProfileIntW(L"power", L"restart_minutes", 0, path);
    out_snapshot->power_sleep_wait_apps =
        (int32_t)GetPrivateProfileIntW(L"power", L"sleep_wait_apps", 1, path) != 0;
    out_snapshot->power_shutdown_wait_apps =
        (int32_t)GetPrivateProfileIntW(L"power", L"shutdown_wait_apps", 0, path) != 0;
    out_snapshot->power_restart_wait_apps =
        (int32_t)GetPrivateProfileIntW(L"power", L"restart_wait_apps", 1, path) != 0;
    out_snapshot->high_refresh_rate =
        (int32_t)GetPrivateProfileIntW(L"display", L"high_refresh_rate", 1, path) != 0;
    out_snapshot->bundled_font =
        (int32_t)GetPrivateProfileIntW(L"display", L"bundled_font", 1, path) != 0;
    out_snapshot->light_theme =
        (int32_t)GetPrivateProfileIntW(L"display", L"light_theme", 0, path) != 0;
    out_snapshot->stage_animation_ms =
        (int32_t)GetPrivateProfileIntW(L"stage", L"animation_ms", 280, path);
    GetPrivateProfileStringW(L"wallpaper", L"path", L"",
                             reinterpret_cast<wchar_t *>(out_snapshot->wallpaper_path), 260, path);
    for (size_t index = 0; index < REACH_MAX_WALLPAPER_MONITORS; ++index)
    {
        wchar_t section[48] = {};
        swprintf_s(section, L"wallpaper.monitor.%u", (unsigned)(index + 1));
        GetPrivateProfileStringW(
            section, L"path", L"",
            reinterpret_cast<wchar_t *>(out_snapshot->monitor_wallpaper_paths[index]), 260, path);
    }
    reach_config_resolve_wallpaper_paths(store, out_snapshot);

    for (size_t index = 0; index < REACH_MAX_PINNED_APPS; ++index)
    {
        wchar_t section[32] = {};
        swprintf_s(section, L"pinned.%u", (unsigned)index);
        wchar_t app_path[260] = {};
        GetPrivateProfileStringW(section, L"path", L"", app_path, 260, path);
        if (app_path[0] == 0)
        {
            continue;
        }

        reach_pinned_app_model *app = &out_snapshot->pinned_apps[out_snapshot->pinned_app_count];
        app->id = (uint32_t)GetPrivateProfileIntW(section, L"id",
                                                  (int)(out_snapshot->pinned_app_count + 1), path);
        reach_copy_utf16(app->path, 260, reinterpret_cast<const uint16_t *>(app_path));
        GetPrivateProfileStringW(section, L"arguments", L"",
                                 reinterpret_cast<wchar_t *>(app->arguments), 260, path);
        GetPrivateProfileStringW(section, L"icon", L"", reinterpret_cast<wchar_t *>(app->icon_ref),
                                 260, path);
        GetPrivateProfileStringW(section, L"app_user_model_id", L"",
                                 reinterpret_cast<wchar_t *>(app->app_user_model_id), 260, path);
        out_snapshot->pinned_app_count += 1;
    }

    return REACH_OK;
}

static reach_result reach_config_store_save(reach_config_store *store,
                                            const reach_config_snapshot *snapshot)
{
    REACH_ASSERT(store != nullptr);
    REACH_ASSERT(snapshot != nullptr);
    if (store == nullptr || snapshot == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    std::wstring text;
    text.append(L"; reach v");
    text.append(REACH_CONFIG_VERSION_WIDE(REACH_VERSION_STRING));
    text.append(L"\r\n[reach]\r\nversion=");
    text.append(REACH_CONFIG_VERSION_WIDE(REACH_VERSION_STRING));
    text.append(L"\r\n\r\n[dock]\r\nheight=");
    text.append(std::to_wstring((int32_t)snapshot->dock_height));
    text.append(L"\r\n\r\n[power]\r\nscreen_off_minutes=");
    text.append(std::to_wstring(snapshot->power_screen_off_minutes));
    text.append(L"\r\nsleep_minutes=");
    text.append(std::to_wstring(snapshot->power_sleep_minutes));
    text.append(L"\r\nlock_minutes=");
    text.append(std::to_wstring(snapshot->power_lock_minutes));
    text.append(L"\r\nshutdown_minutes=");
    text.append(std::to_wstring(snapshot->power_shutdown_minutes));
    text.append(L"\r\nrestart_minutes=");
    text.append(std::to_wstring(snapshot->power_restart_minutes));
    text.append(L"\r\nsleep_wait_apps=");
    text.append(std::to_wstring(snapshot->power_sleep_wait_apps ? 1 : 0));
    text.append(L"\r\nshutdown_wait_apps=");
    text.append(std::to_wstring(snapshot->power_shutdown_wait_apps ? 1 : 0));
    text.append(L"\r\nrestart_wait_apps=");
    text.append(std::to_wstring(snapshot->power_restart_wait_apps ? 1 : 0));
    text.append(L"\r\n\r\n[display]\r\nhigh_refresh_rate=");
    text.append(std::to_wstring(snapshot->high_refresh_rate ? 1 : 0));
    text.append(L"\r\nbundled_font=");
    text.append(std::to_wstring(snapshot->bundled_font ? 1 : 0));
    text.append(L"\r\nlight_theme=");
    text.append(std::to_wstring(snapshot->light_theme ? 1 : 0));
    text.append(L"\r\n\r\n[stage]\r\nanimation_ms=");
    text.append(std::to_wstring(snapshot->stage_animation_ms));
    text.append(L"\r\n\r\n[wallpaper]\r\npath=");
    text.append(reinterpret_cast<const wchar_t *>(snapshot->wallpaper_path));
    text.append(L"\r\n");
    for (size_t index = 0; index < REACH_MAX_WALLPAPER_MONITORS; ++index)
    {
        if (snapshot->monitor_wallpaper_paths[index][0] != 0)
        {
            text.append(L"\r\n[wallpaper.monitor.");
            text.append(std::to_wstring(index + 1));
            text.append(L"]\r\npath=");
            text.append(reinterpret_cast<const wchar_t *>(
                snapshot->monitor_wallpaper_paths[index]));
            text.append(L"\r\n");
        }
    }

    for (size_t index = 0; index < snapshot->pinned_app_count && index < REACH_MAX_PINNED_APPS;
         ++index)
    {
        const reach_pinned_app_model *app = &snapshot->pinned_apps[index];
        text.append(L"\r\n[pinned.");
        text.append(std::to_wstring(index));
        text.append(L"]\r\nid=");
        text.append(std::to_wstring(app->id));
        text.append(L"\r\npath=");
        text.append(reinterpret_cast<const wchar_t *>(app->path));
        text.append(L"\r\narguments=");
        text.append(reinterpret_cast<const wchar_t *>(app->arguments));
        text.append(L"\r\nicon=");
        text.append(reinterpret_cast<const wchar_t *>(app->icon_ref));
        text.append(L"\r\napp_user_model_id=");
        text.append(reinterpret_cast<const wchar_t *>(app->app_user_model_id));
        text.append(L"\r\n");
    }

    return reach_config_write_utf16_atomic(reinterpret_cast<const wchar_t *>(store->path), text);
}

static void reach_config_store_destroy(reach_config_store *store)
{
    if (store != nullptr && store->transaction_mutex != nullptr)
    {
        CloseHandle(store->transaction_mutex);
    }
    delete store;
}

static BOOL CALLBACK reach_config_notify_window_proc(HWND hwnd, LPARAM param)
{
    int32_t *posted = reinterpret_cast<int32_t *>(param);
    if (posted == nullptr || hwnd == nullptr || !IsWindow(hwnd))
    {
        return TRUE;
    }

    wchar_t class_name[64] = {};
    GetClassNameW(hwnd, class_name, 64);
    if (lstrcmpiW(class_name, L"ReachPlatformWindow") != 0)
    {
        return TRUE;
    }

    if (PostMessageW(hwnd, REACH_WM_CONFIG_CHANGED, 0, 0))
    {
        *posted = 1;
        return FALSE;
    }
    return TRUE;
}

reach_result reach_windows_notify_config_changed(void)
{
    int32_t posted = 0;
    EnumWindows(reach_config_notify_window_proc, reinterpret_cast<LPARAM>(&posted));
    return posted ? REACH_OK : REACH_ERROR;
}

reach_result reach_windows_create_config_store(const uint16_t *path,
                                               reach_config_store_port *out_port)
{
    REACH_ASSERT(path != nullptr);
    REACH_ASSERT(out_port != nullptr);
    if (path == nullptr || out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_config_store *store = new (std::nothrow) reach_config_store();
    if (store == nullptr)
    {
        return REACH_ERROR;
    }

    reach_copy_utf16(store->path, 260, path);
    wchar_t mutex_name[64] = {};
    swprintf_s(mutex_name, L"Local\\ReachConfig-%016llX",
               (unsigned long long)reach_config_path_hash(path));
    store->transaction_mutex = CreateMutexW(nullptr, FALSE, mutex_name);
    if (store->transaction_mutex == nullptr)
    {
        delete store;
        return REACH_ERROR;
    }
    out_port->store = store;
    out_port->ops.begin_transaction = reach_config_store_begin_transaction;
    out_port->ops.end_transaction = reach_config_store_end_transaction;
    out_port->ops.load = reach_config_store_load;
    out_port->ops.save = reach_config_store_save;
    out_port->ops.destroy = reach_config_store_destroy;
    return REACH_OK;
}
