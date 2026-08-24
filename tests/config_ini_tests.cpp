#include "reach/platform/windows_adapters.h"

#include <windows.h>

#include <string>

static int expect(int condition)
{
    return condition ? 0 : 1;
}

static void copy_ascii(uint16_t *dst, size_t dst_count, const char *src)
{
    size_t index = 0;
    while (index + 1 < dst_count && src[index] != 0)
    {
        dst[index] = (uint16_t)src[index];
        ++index;
    }
    dst[index] = 0;
}

int main()
{
    int failed = 0;
    wchar_t temp_directory[260] = {};
    wchar_t path[260] = {};
    failed += expect(GetTempPathW(260, temp_directory) != 0);
    failed += expect(GetTempFileNameW(temp_directory, L"rch", 0, path) != 0);

    reach_config_store_port store = {};
    failed += expect(reach_windows_create_config_store(
                         reinterpret_cast<const uint16_t *>(path), &store) == REACH_OK);

    reach_config_snapshot snapshot = {};
    snapshot.dock_height = 58.0f;
    snapshot.power_screen_off_minutes = 9;
    snapshot.power_sleep_minutes = 21;
    snapshot.power_lock_minutes = 7;
    snapshot.power_sleep_wait_apps = 1;
    snapshot.high_refresh_rate = 1;
    snapshot.bundled_font = 1;
    snapshot.light_theme = 1;
    snapshot.stage_animation_ms = 345;
    snapshot.pinned_app_count = 2;
    snapshot.pinned_apps[0].id = 4;
    copy_ascii(snapshot.pinned_apps[0].path, 260, "C:\\Apps\\one.exe");
    snapshot.pinned_apps[0].arguments[0] = 0x03A9;
    snapshot.pinned_apps[0].arguments[1] = 0;
    snapshot.pinned_apps[1].id = 9;
    copy_ascii(snapshot.pinned_apps[1].path, 260, "C:\\Apps\\two.exe");
    failed += expect(store.ops.save(store.store, &snapshot) == REACH_OK);

    reach_config_snapshot loaded = {};
    failed += expect(store.ops.load(store.store, &loaded) == REACH_OK);
    failed += expect(loaded.dock_height == 58.0f);
    failed += expect(loaded.power_sleep_minutes == 21);
    failed += expect(loaded.stage_animation_ms == 345);
    failed += expect(loaded.pinned_app_count == 2);
    failed += expect(loaded.pinned_apps[0].id == 4);
    failed += expect(loaded.pinned_apps[0].arguments[0] == 0x03A9);

    snapshot.pinned_app_count = 1;
    failed += expect(store.ops.save(store.store, &snapshot) == REACH_OK);
    loaded = {};
    failed += expect(store.ops.load(store.store, &loaded) == REACH_OK);
    failed += expect(loaded.pinned_app_count == 1);

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
