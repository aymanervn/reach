#include "reach/support/util.h"
#include "reach/platform/windows_adapters.h"

#include <windows.h>
#include <shobjidl.h>

#include <cstring>
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

    std::memset(snapshot.get(), 0, sizeof(*snapshot));
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
    snapshot->pinned_apps[0].application.launch.kind = REACH_APPLICATION_LAUNCH_SHORTCUT;
    reach_copy_ascii_to_utf16(snapshot->pinned_apps[0].application.launch.path, 260,
                              "C:\\Pins\\one.lnk");
    reach_application_identity_add_runtime_path(
        &snapshot->pinned_apps[0].application.identity,
        (const uint16_t *)L"C:\\Apps\\one.exe");
    snapshot->pinned_apps[0].application.launch.arguments[0] = 0x03A9;
    snapshot->pinned_apps[0].application.launch.arguments[1] = 0;
    snapshot->pinned_apps[1].id = 9;
    snapshot->pinned_apps[1].application.launch.kind = REACH_APPLICATION_LAUNCH_EXECUTABLE;
    reach_copy_ascii_to_utf16(snapshot->pinned_apps[1].application.launch.path, 260,
                              "C:\\Apps\\two.exe");
    failed += expect(store.ops.save(store.store, snapshot.get()) == REACH_OK);

    failed += expect(store.ops.load(store.store, loaded.get()) == REACH_OK);
    failed += expect(loaded->dock_height == 58.0f);
    failed += expect(loaded->power_sleep_minutes == 21);
    failed += expect(loaded->stage_animation_ms == 345);
    failed += expect(loaded->windows_system_theme == REACH_CONFIG_THEME_LIGHT);
    failed += expect(loaded->windows_app_theme == REACH_CONFIG_THEME_DARK);
    failed += expect(loaded->pinned_app_count == 2);
    failed += expect(loaded->pinned_apps[0].id == 4);
    failed += expect(loaded->pinned_apps[0].application.launch.arguments[0] == 0x03A9);
    failed += expect(
        reach_path_equals(loaded->pinned_apps[0].application.launch.path,
                          snapshot->pinned_apps[0].application.launch.path));

    snapshot->pinned_app_count = 1;
    failed += expect(store.ops.save(store.store, snapshot.get()) == REACH_OK);
    std::memset(loaded.get(), 0, sizeof(*loaded));
    failed += expect(store.ops.load(store.store, loaded.get()) == REACH_OK);
    failed += expect(loaded->pinned_app_count == 1);

    wchar_t shortcut_path[260] = {};
    wchar_t shortcut_target[260] = {};
    wcscpy_s(shortcut_path, path);
    wcscat_s(shortcut_path, L".lnk");
    failed += expect(GetSystemDirectoryW(shortcut_target, 260) != 0);
    wcscat_s(shortcut_target, L"\\notepad.exe");

    HRESULT initialize = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IShellLinkW *link = nullptr;
    HRESULT link_result =
        CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link));
    if (SUCCEEDED(link_result) && link != nullptr)
    {
        link_result = link->SetPath(shortcut_target);
        if (SUCCEEDED(link_result))
        {
            link_result = link->SetArguments(L"--reach-shortcut-test");
        }
        if (SUCCEEDED(link_result))
        {
            link_result = link->SetIconLocation(shortcut_target, 0);
        }
        IPersistFile *persist = nullptr;
        if (SUCCEEDED(link_result))
        {
            link_result = link->QueryInterface(IID_PPV_ARGS(&persist));
        }
        if (SUCCEEDED(link_result) && persist != nullptr)
        {
            link_result = persist->Save(shortcut_path, TRUE);
        }
        if (persist != nullptr)
        {
            persist->Release();
        }
        link->Release();
    }
    failed += expect(SUCCEEDED(link_result));

    std::memset(snapshot.get(), 0, sizeof(*snapshot));
    failed += expect(store.ops.save(store.store, snapshot.get()) == REACH_OK);
    failed += expect(WritePrivateProfileStringW(L"pinned.0", L"path", shortcut_path, path) != 0);
    std::memset(loaded.get(), 0, sizeof(*loaded));
    failed += expect(store.ops.load(store.store, loaded.get()) == REACH_OK);
    failed += expect(loaded->pinned_app_count == 1);
    failed += expect(reach_path_equals(
        loaded->pinned_apps[0].application.launch.path,
        reinterpret_cast<const uint16_t *>(shortcut_path)));
    failed += expect(reach_path_equals(
        loaded->pinned_apps[0].application.identity.runtime_paths[0],
        reinterpret_cast<const uint16_t *>(shortcut_target)));
    failed += expect(reach_path_equals(
        loaded->pinned_apps[0].application.icon_ref,
        reinterpret_cast<const uint16_t *>(shortcut_path)));
    failed += expect(
        lstrcmpW(reinterpret_cast<const wchar_t *>(
                     loaded->pinned_apps[0].application.launch.arguments),
                 L"--reach-shortcut-test") == 0);
    DeleteFileW(shortcut_path);
    if (SUCCEEDED(initialize))
    {
        CoUninitialize();
    }

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
