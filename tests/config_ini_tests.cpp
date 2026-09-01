#include "reach/support/util.h"
#include "reach/platform/windows_adapters.h"

#include <windows.h>

#include <memory>
#include <string>

static int expect(int condition)
{
    return condition ? 0 : 1;
}

int main()
{
    int failed = 0;
    wchar_t temp_directory[260] = {};
    wchar_t path[260] = {};
    failed += expect(GetTempPathW(260, temp_directory) != 0);
    failed += expect(GetTempFileNameW(temp_directory, L"rch", 0, path) != 0);
    failed += expect(DeleteFileW(path) != 0);

    reach_config_store_port store = {};
    failed += expect(reach_windows_create_config_store(reinterpret_cast<const uint16_t *>(path),
                                                       &store) == REACH_OK);

    std::unique_ptr<reach_config_snapshot> snapshot(new reach_config_snapshot());
    std::unique_ptr<reach_config_snapshot> loaded(new reach_config_snapshot());
    failed += expect(store.ops.load(store.store, snapshot.get()) == REACH_OK);
    failed += expect(snapshot->high_refresh_rate == 1);
    failed += expect(snapshot->bundled_font == 1);
    failed += expect(snapshot->light_theme == 0);
    failed += expect(snapshot->windows_system_theme == REACH_CONFIG_THEME_FOLLOW_REACH);
    failed += expect(snapshot->windows_app_theme == REACH_CONFIG_THEME_FOLLOW_REACH);

    *snapshot = {};
    snapshot->dock_height = 58.0f;
    snapshot->power_screen_off_minutes = 9;
    snapshot->power_sleep_minutes = 21;
    snapshot->power_lock_minutes = 7;
    snapshot->power_sleep_wait_apps = 1;
    snapshot->high_refresh_rate = 1;
    snapshot->bundled_font = 1;
    snapshot->light_theme = 1;
    snapshot->windows_system_theme = REACH_CONFIG_THEME_LIGHT;
    snapshot->windows_app_theme = REACH_CONFIG_THEME_DARK;
    snapshot->stage_animation_ms = 345;
    snapshot->pinned_app_count = 2;
    snapshot->pinned_apps[0].id = 4;
    reach_copy_ascii_to_utf16(snapshot->pinned_apps[0].path, 260, "C:\\Apps\\one.exe");
    snapshot->pinned_apps[0].arguments[0] = 0x03A9;
    snapshot->pinned_apps[0].arguments[1] = 0;
    snapshot->pinned_apps[1].id = 9;
    reach_copy_ascii_to_utf16(snapshot->pinned_apps[1].path, 260, "C:\\Apps\\two.exe");
    failed += expect(store.ops.save(store.store, snapshot.get()) == REACH_OK);

    failed += expect(store.ops.load(store.store, loaded.get()) == REACH_OK);
    failed += expect(loaded->dock_height == 58.0f);
    failed += expect(loaded->power_sleep_minutes == 21);
    failed += expect(loaded->stage_animation_ms == 345);
    failed += expect(loaded->windows_system_theme == REACH_CONFIG_THEME_LIGHT);
    failed += expect(loaded->windows_app_theme == REACH_CONFIG_THEME_DARK);
    failed += expect(loaded->pinned_app_count == 2);
    failed += expect(loaded->pinned_apps[0].id == 4);
    failed += expect(loaded->pinned_apps[0].arguments[0] == 0x03A9);

    snapshot->pinned_app_count = 1;
    failed += expect(store.ops.save(store.store, snapshot.get()) == REACH_OK);
    *loaded = {};
    failed += expect(store.ops.load(store.store, loaded.get()) == REACH_OK);
    failed += expect(loaded->pinned_app_count == 1);

    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    wchar_t bom = 0;
    DWORD read = 0;
    failed += expect(file != INVALID_HANDLE_VALUE);
    if (file != INVALID_HANDLE_VALUE)
    {
        failed += expect(ReadFile(file, &bom, sizeof(bom), &read, nullptr) != 0);
        failed += expect(read == sizeof(bom));
        failed += expect(bom == 0xFEFF);
        CloseHandle(file);
    }

    if (store.ops.destroy != nullptr)
    {
        store.ops.destroy(store.store);
    }
    DeleteFileW(path);
    std::wstring temp_path(path);
    temp_path.append(L".tmp");
    DeleteFileW(temp_path.c_str());
    return failed == 0 ? 0 : 1;
}
