#include "reach/support/util.h"
#include "reach/platform/windows_adapters.h"

#include <windows.h>
#include <shobjidl.h>

#include <atomic>
#include <cstring>
#include <memory>
#include <string>

static int expect(int condition)
{
    return condition ? 0 : 1;
}

static void count_change(void *user)
{
    static_cast<std::atomic<int> *>(user)->fetch_add(1);
}

static int wait_for_changes(const std::atomic<int> &changes, int at_least)
{
    for (int waited = 0; waited < 3000 && changes.load() < at_least; waited += 10)
    {
        Sleep(10);
    }
    return changes.load() >= at_least;
}

static int test_config_watch(const reach_config_snapshot *snapshot)
{
    int failed = 0;

    // A private directory keeps unrelated temp-file churn out of the notifications.
    wchar_t temp_directory[260] = {};
    failed += expect(GetTempPathW(260, temp_directory) != 0);
    std::wstring directory(temp_directory);
    directory.append(L"reach_watch_test_");
    directory.append(std::to_wstring(GetCurrentProcessId()));
    failed += expect(CreateDirectoryW(directory.c_str(), nullptr) != 0);
    std::wstring config_path = directory + L"\\reach.ini";
    std::wstring sibling_path = directory + L"\\other.ini";

    reach_config_store_port store = {};
    failed +=
        expect(reach_windows_create_config_store(
                   reinterpret_cast<const uint16_t *>(config_path.c_str()), &store) == REACH_OK);
    std::atomic<int> changes(0);
    reach_windows_config_watch *watch = nullptr;
    failed += expect(
        reach_windows_config_watch_create(reinterpret_cast<const uint16_t *>(config_path.c_str()),
                                          count_change, &changes, &watch) == REACH_OK);
    if (watch == nullptr || store.ops.save == nullptr)
    {
        return failed + 1;
    }

    // Saves replace the file through a temp file; that must read as one change to reach.ini.
    failed += expect(store.ops.save(store.store, snapshot) == REACH_OK);
    failed += expect(wait_for_changes(changes, 1));

    // The watcher is armed now. Let trailing notifications drain, then a sibling file in
    // the same directory must not count.
    Sleep(300);
    changes.store(0);
    failed += expect(WritePrivateProfileStringW(L"x", L"y", L"1", sibling_path.c_str()) != 0);
    Sleep(300);
    failed += expect(changes.load() == 0);

    // And it keeps working after the quiet period.
    failed += expect(store.ops.save(store.store, snapshot) == REACH_OK);
    failed += expect(wait_for_changes(changes, 1));

    reach_windows_config_watch_destroy(watch);
    if (store.ops.destroy != nullptr)
    {
        store.ops.destroy(store.store);
    }
    DeleteFileW(sibling_path.c_str());
    DeleteFileW(config_path.c_str());
    DeleteFileW((config_path + L".tmp").c_str());
    RemoveDirectoryW(directory.c_str());
    return failed;
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
    failed += expect(snapshot->top_bar_style == REACH_CONFIG_TOP_BAR_STYLE_TRANSPARENT);
    failed += expect(snapshot->top_bar_mode == REACH_CONFIG_TOP_BAR_MODE_STATIC);
    failed += expect(snapshot->top_bar_overlay == 0);
    failed += expect(snapshot->dock_auto_hide == 1);

    std::memset(snapshot.get(), 0, sizeof(*snapshot));
    snapshot->dock_height = 58.0f;
    snapshot->top_bar_style = REACH_CONFIG_TOP_BAR_STYLE_TRANSPARENT;
    snapshot->top_bar_mode = REACH_CONFIG_TOP_BAR_MODE_STATIC;
    snapshot->top_bar_overlay = 1;
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
    reach_application_identity_add_runtime_path(&snapshot->pinned_apps[0].application.identity,
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
    failed += expect(loaded->dock_auto_hide == 0);
    failed += expect(loaded->top_bar_style == REACH_CONFIG_TOP_BAR_STYLE_TRANSPARENT);
    failed += expect(loaded->top_bar_mode == REACH_CONFIG_TOP_BAR_MODE_STATIC);
    failed += expect(loaded->top_bar_overlay == 1);
    failed += expect(WritePrivateProfileStringW(L"top_bar", L"overlay", L"7", path) != 0);
    std::memset(loaded.get(), 0, sizeof(*loaded));
    failed += expect(store.ops.load(store.store, loaded.get()) == REACH_OK);
    failed += expect(loaded->top_bar_overlay == 1);
    failed += expect(loaded->power_sleep_minutes == 21);
    failed += expect(loaded->stage_animation_ms == 345);
    failed += expect(loaded->windows_system_theme == REACH_CONFIG_THEME_LIGHT);
    failed += expect(loaded->windows_app_theme == REACH_CONFIG_THEME_DARK);
    failed += expect(loaded->pinned_app_count == 2);
    failed += expect(loaded->pinned_apps[0].id == 4);
    failed += expect(loaded->pinned_apps[0].application.launch.arguments[0] == 0x03A9);
    failed += expect(reach_path_equals(loaded->pinned_apps[0].application.launch.path,
                                       snapshot->pinned_apps[0].application.launch.path));

    failed += expect(WritePrivateProfileStringW(L"widgets", L"enabled", L"1023", path) != 0);
    failed += expect(WritePrivateProfileStringW(L"widget_7", L"x", L"-1", path) != 0);
    failed += expect(WritePrivateProfileStringW(L"widget_7", L"width", L"280", path) != 0);
    failed += expect(WritePrivateProfileStringW(L"widget_4", L"width", L"480", path) != 0);
    failed += expect(store.ops.load(store.store, loaded.get()) == REACH_OK);
    failed += expect(loaded->desktop_widgets == REACH_DESKTOP_WIDGET_MASK);
    failed += expect(loaded->widget_placements[REACH_WIDGET_POMODORO].x == -1);
    failed += expect(loaded->widget_placements[REACH_WIDGET_POMODORO].width == 280);
    failed += expect(loaded->widget_placements[REACH_WIDGET_STOPWATCH].width == 0);
    snapshot->desktop_widgets = loaded->desktop_widgets;
    snapshot->widget_placements[REACH_WIDGET_POMODORO] =
        loaded->widget_placements[REACH_WIDGET_POMODORO];

    snapshot->pinned_app_count = 1;
    failed += expect(store.ops.save(store.store, snapshot.get()) == REACH_OK);
    std::memset(loaded.get(), 0, sizeof(*loaded));
    failed += expect(store.ops.load(store.store, loaded.get()) == REACH_OK);
    failed += expect(loaded->pinned_app_count == 1);
    failed += expect(loaded->desktop_widgets == REACH_DESKTOP_WIDGET_MASK);
    failed += expect(GetPrivateProfileIntW(L"widgets", L"enabled", 0, path) == 207);
    failed += expect(loaded->widget_placements[REACH_WIDGET_POMODORO].x == -1);
    failed += expect(loaded->widget_placements[REACH_WIDGET_POMODORO].width == 280);

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
    failed += expect(reach_path_equals(loaded->pinned_apps[0].application.launch.path,
                                       reinterpret_cast<const uint16_t *>(shortcut_path)));
    failed += expect(reach_path_equals(loaded->pinned_apps[0].application.identity.runtime_paths[0],
                                       reinterpret_cast<const uint16_t *>(shortcut_target)));
    failed += expect(reach_path_equals(loaded->pinned_apps[0].application.icon_ref,
                                       reinterpret_cast<const uint16_t *>(shortcut_path)));
    failed += expect(lstrcmpW(reinterpret_cast<const wchar_t *>(
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

    failed += test_config_watch(snapshot.get());

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
