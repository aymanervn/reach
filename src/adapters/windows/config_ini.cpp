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

static std::string reach_config_read_bytes(const wchar_t *path)
{
    std::string bytes;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return bytes;
    }

    char buffer[4096];
    DWORD read = 0;
    while (ReadFile(file, buffer, sizeof(buffer), &read, nullptr) && read > 0)
    {
        bytes.append(buffer, read);
    }
    CloseHandle(file);
    return bytes;
}

static void reach_config_write_utf16_file(const wchar_t *path, const std::wstring &text)
{
    HANDLE out =
        CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE)
    {
        return;
    }

    std::wstring output;
    output.push_back(reach_config_byte_order_mark);
    output.append(text);

    DWORD written = 0;
    WriteFile(out, output.data(), (DWORD)(output.size() * sizeof(wchar_t)), &written, nullptr);
    CloseHandle(out);
}

static int32_t reach_config_is_utf16(const std::string &bytes)
{
    return bytes.size() >= 2 && (unsigned char)bytes[0] == 0xFF && (unsigned char)bytes[1] == 0xFE;
}

static std::wstring reach_config_decode(const std::string &bytes)
{
    if (reach_config_is_utf16(bytes))
    {
        return std::wstring(reinterpret_cast<const wchar_t *>(bytes.data() + 2),
                            (bytes.size() - 2) / sizeof(wchar_t));
    }
    if (bytes.empty())
    {
        return std::wstring();
    }

    int needed = MultiByteToWideChar(CP_ACP, 0, bytes.data(), (int)bytes.size(), nullptr, 0);
    if (needed <= 0)
    {
        return std::wstring();
    }
    std::wstring text((size_t)needed, L'\0');
    MultiByteToWideChar(CP_ACP, 0, bytes.data(), (int)bytes.size(), &text[0], needed);
    return text;
}

// The Win32 profile APIs only read and write Unicode when the file already carries a UTF-16
// byte order mark; without one they fall back to the ANSI code page and drop every character
// it cannot represent. Converting the file up front keeps titles and paths intact.
static void reach_config_ensure_utf16_file(const wchar_t *path)
{
    std::string bytes = reach_config_read_bytes(path);
    if (bytes.empty() || reach_config_is_utf16(bytes))
    {
        return;
    }
    reach_config_write_utf16_file(path, reach_config_decode(bytes));
}

static void reach_config_write_version_header(const wchar_t *path, const wchar_t *version)
{
    wchar_t comment[64] = {};
    swprintf_s(comment, L"; reach v%s", version);

    std::wstring existing = reach_config_decode(reach_config_read_bytes(path));

    size_t body_start = 0;
    if (!existing.empty() && (existing[0] == L';' || existing[0] == L'#'))
    {
        size_t line_end = existing.find(L'\n');
        if (line_end != std::wstring::npos && existing.find(L"reach", 0) < line_end)
        {
            body_start = line_end + 1;
        }
    }

    reach_config_write_utf16_file(path,
                                  std::wstring(comment) + L"\r\n" + existing.substr(body_start));
}

struct reach_config_store
{
    uint16_t path[260];
};

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
    out_snapshot->power_sleep_minutes =
        (int32_t)GetPrivateProfileIntW(L"power", L"sleep_minutes", 30, path);
    out_snapshot->power_lock_minutes =
        (int32_t)GetPrivateProfileIntW(L"power", L"lock_minutes", 0, path);
    out_snapshot->power_shutdown_minutes =
        (int32_t)GetPrivateProfileIntW(L"power", L"shutdown_minutes", 0, path);
    out_snapshot->power_restart_minutes =
        (int32_t)GetPrivateProfileIntW(L"power", L"restart_minutes", 0, path);
    out_snapshot->power_sleep_wait_apps =
        (int32_t)GetPrivateProfileIntW(L"power", L"sleep_wait_apps", 0, path) != 0;
    out_snapshot->power_shutdown_wait_apps =
        (int32_t)GetPrivateProfileIntW(L"power", L"shutdown_wait_apps", 0, path) != 0;
    out_snapshot->power_restart_wait_apps =
        (int32_t)GetPrivateProfileIntW(L"power", L"restart_wait_apps", 0, path) != 0;
    out_snapshot->high_refresh_rate =
        (int32_t)GetPrivateProfileIntW(L"display", L"high_refresh_rate", 0, path) != 0;
    out_snapshot->bundled_font =
        (int32_t)GetPrivateProfileIntW(L"display", L"bundled_font", 0, path) != 0;
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
        wchar_t title[128] = {};
        GetPrivateProfileStringW(section, L"title", L"", title, 128, path);
        if (title[0] == 0)
        {
            continue;
        }

        reach_pinned_app_model *app = &out_snapshot->pinned_apps[out_snapshot->pinned_app_count];
        app->id = (uint32_t)GetPrivateProfileIntW(section, L"id",
                                                  (int)(out_snapshot->pinned_app_count + 1), path);
        reach_copy_utf16(app->title, 128, reinterpret_cast<const uint16_t *>(title));
        GetPrivateProfileStringW(section, L"path", L"", reinterpret_cast<wchar_t *>(app->path), 260,
                                 path);
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

    const wchar_t *path = reinterpret_cast<const wchar_t *>(store->path);
    reach_config_ensure_utf16_file(path);
    WritePrivateProfileStringW(L"reach", L"version",
                               REACH_CONFIG_VERSION_WIDE(REACH_VERSION_STRING), path);
    wchar_t value[32] = {};
    swprintf_s(value, L"%.0f", snapshot->dock_height);
    WritePrivateProfileStringW(L"dock", L"height", value, path);
    WritePrivateProfileStringW(L"dock", L"width", nullptr, path);
    WritePrivateProfileStringW(L"dock", L"icon_size", nullptr, path);
    swprintf_s(value, L"%d", snapshot->power_sleep_minutes);
    WritePrivateProfileStringW(L"power", L"sleep_minutes", value, path);
    swprintf_s(value, L"%d", snapshot->power_lock_minutes);
    WritePrivateProfileStringW(L"power", L"lock_minutes", value, path);
    swprintf_s(value, L"%d", snapshot->power_shutdown_minutes);
    WritePrivateProfileStringW(L"power", L"shutdown_minutes", value, path);
    swprintf_s(value, L"%d", snapshot->power_restart_minutes);
    WritePrivateProfileStringW(L"power", L"restart_minutes", value, path);
    swprintf_s(value, L"%d", snapshot->power_sleep_wait_apps ? 1 : 0);
    WritePrivateProfileStringW(L"power", L"sleep_wait_apps", value, path);
    swprintf_s(value, L"%d", snapshot->power_shutdown_wait_apps ? 1 : 0);
    WritePrivateProfileStringW(L"power", L"shutdown_wait_apps", value, path);
    swprintf_s(value, L"%d", snapshot->power_restart_wait_apps ? 1 : 0);
    WritePrivateProfileStringW(L"power", L"restart_wait_apps", value, path);
    swprintf_s(value, L"%d", snapshot->high_refresh_rate ? 1 : 0);
    WritePrivateProfileStringW(L"display", L"high_refresh_rate", value, path);
    swprintf_s(value, L"%d", snapshot->bundled_font ? 1 : 0);
    WritePrivateProfileStringW(L"display", L"bundled_font", value, path);
    swprintf_s(value, L"%d", snapshot->light_theme ? 1 : 0);
    WritePrivateProfileStringW(L"display", L"light_theme", value, path);
    WritePrivateProfileStringW(L"wallpaper", L"path",
                               reinterpret_cast<const wchar_t *>(snapshot->wallpaper_path), path);
    for (size_t index = 0; index < REACH_MAX_WALLPAPER_MONITORS; ++index)
    {
        wchar_t section[48] = {};
        swprintf_s(section, L"wallpaper.monitor.%u", (unsigned)(index + 1));
        if (snapshot->monitor_wallpaper_paths[index][0] != 0)
        {
            WritePrivateProfileStringW(
                section, L"path",
                reinterpret_cast<const wchar_t *>(snapshot->monitor_wallpaper_paths[index]), path);
        }
        else
        {
            WritePrivateProfileStringW(section, nullptr, nullptr, path);
        }
    }

    for (size_t index = 0; index < snapshot->pinned_app_count && index < REACH_MAX_PINNED_APPS;
         ++index)
    {
        wchar_t section[32] = {};
        swprintf_s(section, L"pinned.%u", (unsigned)index);
        const reach_pinned_app_model *app = &snapshot->pinned_apps[index];
        swprintf_s(value, L"%u", (unsigned)app->id);
        WritePrivateProfileStringW(section, L"id", value, path);
        WritePrivateProfileStringW(section, L"title", reinterpret_cast<const wchar_t *>(app->title),
                                   path);
        WritePrivateProfileStringW(section, L"path", reinterpret_cast<const wchar_t *>(app->path),
                                   path);
        WritePrivateProfileStringW(section, L"arguments",
                                   reinterpret_cast<const wchar_t *>(app->arguments), path);
        WritePrivateProfileStringW(section, L"icon",
                                   reinterpret_cast<const wchar_t *>(app->icon_ref), path);
        WritePrivateProfileStringW(section, L"app_user_model_id",
                                   reinterpret_cast<const wchar_t *>(app->app_user_model_id), path);
    }
    for (size_t index = snapshot->pinned_app_count; index < REACH_MAX_PINNED_APPS; ++index)
    {
        wchar_t section[32] = {};
        swprintf_s(section, L"pinned.%u", (unsigned)index);
        WritePrivateProfileStringW(section, nullptr, nullptr, path);
    }

    reach_config_write_version_header(path, REACH_CONFIG_VERSION_WIDE(REACH_VERSION_STRING));
    return REACH_OK;
}

static void reach_config_store_destroy(reach_config_store *store)
{
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
    out_port->store = store;
    out_port->ops.load = reach_config_store_load;
    out_port->ops.save = reach_config_store_save;
    out_port->ops.destroy = reach_config_store_destroy;
    return REACH_OK;
}
