#include <windows.h>
#include <shlwapi.h>

#include <cwchar>

#include "reach/config_path.h"
#include "reach/pin_config.h"
#include "reach/platform/windows_adapters.h"
#include "reach/platform/windows_messages.h"
#include "reach/platform/shell_registration.h"

static void reachctl_print(const wchar_t *message)
{
    if (message == nullptr) {
        return;
    }

    DWORD written = 0;
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!WriteConsoleW(output, message, (DWORD)wcslen(message), &written, nullptr)) {
        char utf8[1024] = {};
        int bytes = WideCharToMultiByte(CP_UTF8, 0, message, -1, utf8, (int)sizeof(utf8) - 3, nullptr, nullptr);
        if (bytes > 0) {
            utf8[bytes - 1] = '\r';
            utf8[bytes] = '\n';
            WriteFile(output, utf8, (DWORD)(bytes + 1), &written, nullptr);
        }
        return;
    }
    WriteConsoleW(output, L"\r\n", 2, &written, nullptr);
}

static reach_result reachctl_target_exe(uint16_t *path, DWORD path_count)
{
    if (path == nullptr || path_count == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD length = GetModuleFileNameW(nullptr, reinterpret_cast<wchar_t *>(path), path_count);
    if (length == 0 || length >= path_count) {
        return REACH_ERROR;
    }

    wchar_t *path_w = reinterpret_cast<wchar_t *>(path);
    if (!PathRemoveFileSpecW(path_w)) {
        return REACH_ERROR;
    }
    if (!PathAppendW(path_w, L"reach.exe")) {
        return REACH_ERROR;
    }
    return REACH_OK;
}

static reach_result reachctl_current_exe(uint16_t *path, DWORD path_count)
{
    if (path == nullptr || path_count == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD length = GetModuleFileNameW(nullptr, reinterpret_cast<wchar_t *>(path), path_count);
    return length > 0 && length < path_count ? REACH_OK : REACH_ERROR;
}

static reach_result reachctl_open_config_store(reach_config_store_port *out_store)
{
    uint16_t config_path[260] = {};
    reach_result result = reach_default_config_path(config_path, 260);
    if (result != REACH_OK) {
        return result;
    }
    return reach_windows_create_config_store(config_path, out_store);
}

static reach_result reachctl_write_string_value(HKEY root, const wchar_t *key, const wchar_t *name, const wchar_t *value)
{
    HKEY handle = nullptr;
    LONG status = RegCreateKeyExW(root, key, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &handle, nullptr);
    if (status != ERROR_SUCCESS) {
        return REACH_ERROR;
    }
    status = RegSetValueExW(handle, name, 0, REG_SZ, reinterpret_cast<const BYTE *>(value), (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
    RegCloseKey(handle);
    return status == ERROR_SUCCESS ? REACH_OK : REACH_ERROR;
}

static reach_result reachctl_delete_tree(HKEY root, const wchar_t *key)
{
    LONG status = RegDeleteTreeW(root, key);
    return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND ? REACH_OK : REACH_ERROR;
}

static reach_result reachctl_install_context_menu(void)
{
    uint16_t ctl_path[260] = {};
    if (reachctl_current_exe(ctl_path, 260) != REACH_OK) {
        return REACH_ERROR;
    }

    wchar_t command[640] = {};
    swprintf_s(command, L"\"%ls\" --pin-path \"%%1\"", reinterpret_cast<const wchar_t *>(ctl_path));
    const wchar_t *targets[] = {
        L"Software\\Classes\\*\\shell\\Reach.PinToDock",
        L"Software\\Classes\\Directory\\shell\\Reach.PinToDock",
        L"Software\\Classes\\exefile\\shell\\Reach.PinToDock",
        L"Software\\Classes\\lnkfile\\shell\\Reach.PinToDock"
    };

    for (size_t index = 0; index < sizeof(targets) / sizeof(targets[0]); ++index) {
        wchar_t command_key[260] = {};
        swprintf_s(command_key, L"%ls\\command", targets[index]);
        if (reachctl_write_string_value(HKEY_CURRENT_USER, targets[index], nullptr, L"Pin app to dock") != REACH_OK ||
            reachctl_write_string_value(HKEY_CURRENT_USER, targets[index], L"Icon", reinterpret_cast<const wchar_t *>(ctl_path)) != REACH_OK ||
            reachctl_write_string_value(HKEY_CURRENT_USER, command_key, nullptr, command) != REACH_OK) {
            return REACH_ERROR;
        }
    }

    return REACH_OK;
}

static reach_result reachctl_remove_context_menu(void)
{
    const wchar_t *targets[] = {
        L"Software\\Classes\\*\\shell\\Reach.PinToDock",
        L"Software\\Classes\\Directory\\shell\\Reach.PinToDock",
        L"Software\\Classes\\exefile\\shell\\Reach.PinToDock",
        L"Software\\Classes\\lnkfile\\shell\\Reach.PinToDock"
    };

    reach_result result = REACH_OK;
    for (size_t index = 0; index < sizeof(targets) / sizeof(targets[0]); ++index) {
        if (reachctl_delete_tree(HKEY_CURRENT_USER, targets[index]) != REACH_OK) {
            result = REACH_ERROR;
        }
    }
    return result;
}

static reach_result reachctl_absolute_path(const uint16_t *path, uint16_t *out_path, DWORD out_path_count)
{
    if (path == nullptr || path[0] == 0 || out_path == nullptr || out_path_count == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    wchar_t full_path[260] = {};
    DWORD length = GetFullPathNameW(reinterpret_cast<const wchar_t *>(path), 260, full_path, nullptr);
    if (length == 0 || length >= out_path_count) {
        return REACH_ERROR;
    }
    return reach_copy_utf16(out_path, out_path_count, reinterpret_cast<const uint16_t *>(full_path));
}

static void reachctl_notify_wallpaper_changed(void)
{
    HWND hwnd = nullptr;
    while ((hwnd = FindWindowExW(nullptr, hwnd, L"ReachPlatformWindow", nullptr)) != nullptr) {
        PostMessageW(hwnd, REACH_WM_WALLPAPER_CHANGED, 0, 0);
    }
}

static reach_result reachctl_set_wallpaper(const uint16_t *path)
{
    if (path == nullptr || path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }
    uint16_t full_path[260] = {};
    reach_result result = reachctl_absolute_path(path, full_path, 260);
    if (result != REACH_OK) {
        return result;
    }

    DWORD attributes = GetFileAttributesW(reinterpret_cast<const wchar_t *>(full_path));
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_config_store_port store = {};
    result = reachctl_open_config_store(&store);
    reach_config_snapshot snapshot = {};
    if (result == REACH_OK) {
        result = store.ops.load(store.store, &snapshot);
    }
    if (result == REACH_OK) {
        result = reach_copy_utf16(snapshot.wallpaper_path, 260, full_path);
    }
    if (result == REACH_OK) {
        result = store.ops.save(store.store, &snapshot);
    }
    reach_wallpaper_service_port wallpaper = {};
    if (result == REACH_OK) {
        result = reach_windows_create_wallpaper_service(&wallpaper);
    }
    if (result == REACH_OK) {
        result = wallpaper.ops.set_wallpaper(wallpaper.service, full_path);
    }
    if (wallpaper.ops.destroy != nullptr) {
        wallpaper.ops.destroy(wallpaper.service);
    }
    if (store.ops.destroy != nullptr) {
        store.ops.destroy(store.store);
    }
    if (result == REACH_OK) {
        reachctl_notify_wallpaper_changed();
    }
    return result;
}

static reach_result reachctl_clear_wallpaper(void)
{
    reach_config_store_port store = {};
    reach_result result = reachctl_open_config_store(&store);
    reach_config_snapshot snapshot = {};
    if (result == REACH_OK) {
        result = store.ops.load(store.store, &snapshot);
    }
    if (result == REACH_OK) {
        snapshot.wallpaper_path[0] = 0;
        result = store.ops.save(store.store, &snapshot);
    }
    reach_wallpaper_service_port wallpaper = {};
    if (result == REACH_OK) {
        result = reach_windows_create_wallpaper_service(&wallpaper);
    }
    if (result == REACH_OK) {
        result = wallpaper.ops.clear_wallpaper(wallpaper.service);
    }
    if (wallpaper.ops.destroy != nullptr) {
        wallpaper.ops.destroy(wallpaper.service);
    }
    if (store.ops.destroy != nullptr) {
        store.ops.destroy(store.store);
    }
    if (result == REACH_OK) {
        reachctl_notify_wallpaper_changed();
    }
    return result;
}

int wmain(int argc, wchar_t **argv)
{
    uint16_t reach_exe[260] = {};
    if (reachctl_target_exe(reach_exe, 260) != REACH_OK) {
        reachctl_print(L"Could not locate sibling reach.exe.");
        return 1;
    }

    for (int index = 1; index < argc; ++index) {
        if (lstrcmpiW(argv[index], L"--install-shell") == 0) {
            int ok = reach_windows_shell_install_current_user(reach_exe) == REACH_OK;
            reachctl_print(ok ? L"Reach shell installed for current user." : L"Reach shell install failed.");
            return ok ? 0 : 1;
        }
        if (lstrcmpiW(argv[index], L"--restore-shell") == 0) {
            int ok = reach_windows_shell_restore_current_user() == REACH_OK;
            reachctl_print(ok ? L"Windows shell restored for current user." : L"Windows shell restore failed.");
            return ok ? 0 : 1;
        }
        if (lstrcmpiW(argv[index], L"--print-shell-status") == 0) {
            reach_shell_registration_status status = {};
            if (reach_windows_shell_query_current_user(reach_exe, &status) != REACH_OK) {
                reachctl_print(L"Reach shell status query failed.");
                return 1;
            }

            wchar_t line[640] = {};
            swprintf_s(
                line,
                L"CurrentShell=%ls PreviousShell=%ls ReachIsShell=%d StartupAttemptCount=%u",
                reinterpret_cast<const wchar_t *>(status.current_shell),
                reinterpret_cast<const wchar_t *>(status.previous_shell),
                status.reach_is_shell,
                status.startup_attempt_count);
            reachctl_print(line);
            return 0;
        }
        if (lstrcmpiW(argv[index], L"--pin-path") == 0) {
            if (index + 1 >= argc) {
                reachctl_print(L"--pin-path requires a path.");
                return 2;
            }
            reach_config_store_port store = {};
            int ok = reachctl_open_config_store(&store) == REACH_OK &&
                reach_pin_config_pin_path(&store, reinterpret_cast<const uint16_t *>(argv[index + 1])) == REACH_OK;
            if (store.ops.destroy != nullptr) {
                store.ops.destroy(store.store);
            }
            reachctl_print(ok ? L"Pinned to Reach dock." : L"Pin to Reach dock failed.");
            return ok ? 0 : 1;
        }
        if (lstrcmpiW(argv[index], L"--unpin-path") == 0) {
            if (index + 1 >= argc) {
                reachctl_print(L"--unpin-path requires a path.");
                return 2;
            }
            reach_config_store_port store = {};
            int ok = reachctl_open_config_store(&store) == REACH_OK &&
                reach_pin_config_unpin_path(&store, reinterpret_cast<const uint16_t *>(argv[index + 1])) == REACH_OK;
            if (store.ops.destroy != nullptr) {
                store.ops.destroy(store.store);
            }
            reachctl_print(ok ? L"Unpinned from Reach dock." : L"Unpin from Reach dock failed.");
            return ok ? 0 : 1;
        }
        if (lstrcmpiW(argv[index], L"--unpin-id") == 0) {
            if (index + 1 >= argc) {
                reachctl_print(L"--unpin-id requires an id.");
                return 2;
            }
            reach_config_store_port store = {};
            uint32_t id = (uint32_t)wcstoul(argv[index + 1], nullptr, 10);
            int ok = reachctl_open_config_store(&store) == REACH_OK &&
                reach_pin_config_unpin_id(&store, id) == REACH_OK;
            if (store.ops.destroy != nullptr) {
                store.ops.destroy(store.store);
            }
            reachctl_print(ok ? L"Unpinned from Reach dock." : L"Unpin from Reach dock failed.");
            return ok ? 0 : 1;
        }
        if (lstrcmpiW(argv[index], L"--install-context-menu") == 0) {
            int ok = reachctl_install_context_menu() == REACH_OK;
            reachctl_print(ok ? L"Reach Explorer context menu installed." : L"Reach Explorer context menu install failed.");
            return ok ? 0 : 1;
        }
        if (lstrcmpiW(argv[index], L"--remove-context-menu") == 0) {
            int ok = reachctl_remove_context_menu() == REACH_OK;
            reachctl_print(ok ? L"Reach Explorer context menu removed." : L"Reach Explorer context menu removal failed.");
            return ok ? 0 : 1;
        }
        if (lstrcmpiW(argv[index], L"--set-wallpaper") == 0) {
            if (index + 1 >= argc) {
                reachctl_print(L"--set-wallpaper requires a path.");
                return 2;
            }
            int ok = reachctl_set_wallpaper(reinterpret_cast<const uint16_t *>(argv[index + 1])) == REACH_OK;
            reachctl_print(ok ? L"Reach wallpaper set." : L"Reach wallpaper set failed.");
            return ok ? 0 : 1;
        }
        if (lstrcmpiW(argv[index], L"--clear-wallpaper") == 0) {
            int ok = reachctl_clear_wallpaper() == REACH_OK;
            reachctl_print(ok ? L"Reach wallpaper cleared." : L"Reach wallpaper clear failed.");
            return ok ? 0 : 1;
        }
    }

    reachctl_print(L"Usage: reachctl.exe --install-shell | --restore-shell | --print-shell-status | --pin-path <path> | --unpin-path <path> | --unpin-id <id> | --install-context-menu | --remove-context-menu | --set-wallpaper <path> | --clear-wallpaper");
    return 2;
}
