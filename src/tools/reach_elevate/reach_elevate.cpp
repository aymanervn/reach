#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>

#include <stdint.h>
#include <string.h>

static const wchar_t *reach_elevate_run_path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t *reach_elevate_approved_run_path =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
static const wchar_t *reach_elevate_approved_folder_path =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\StartupFolder";

static int32_t reach_elevate_name_valid(const wchar_t *name)
{
    if (name == nullptr || name[0] == 0 || lstrlenW(name) > 255)
    {
        return 0;
    }
    return wcschr(name, L'\\') == nullptr && wcschr(name, L'/') == nullptr;
}

static int32_t reach_elevate_run_value_exists(const wchar_t *name)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, reach_elevate_run_path, 0, KEY_READ, &key) !=
        ERROR_SUCCESS)
    {
        return 0;
    }
    LONG result = RegQueryValueExW(key, name, nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

static int32_t reach_elevate_startup_file_exists(const wchar_t *name)
{
    PWSTR folder_path = nullptr;
    if (FAILED(
            SHGetKnownFolderPath(FOLDERID_CommonStartup, KF_FLAG_DEFAULT, nullptr, &folder_path)) ||
        folder_path == nullptr)
    {
        return 0;
    }

    wchar_t full_path[MAX_PATH] = {};
    int32_t exists = 0;
    if (SUCCEEDED(StringCchPrintfW(full_path, MAX_PATH, L"%ls\\%ls", folder_path, name)))
    {
        exists = GetFileAttributesW(full_path) != INVALID_FILE_ATTRIBUTES;
    }
    CoTaskMemFree(folder_path);
    return exists;
}

static int reach_elevate_startup_set_enabled(const wchar_t *scope, const wchar_t *state,
                                             const wchar_t *name)
{
    int32_t is_run = lstrcmpW(scope, L"machine-run") == 0;
    int32_t is_folder = lstrcmpW(scope, L"machine-folder") == 0;
    if ((!is_run && !is_folder) || !reach_elevate_name_valid(name))
    {
        return 2;
    }

    int32_t enabled = lstrcmpW(state, L"1") == 0;
    if (!enabled && lstrcmpW(state, L"0") != 0)
    {
        return 2;
    }

    if (is_run ? !reach_elevate_run_value_exists(name) : !reach_elevate_startup_file_exists(name))
    {
        return 4;
    }

    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE,
                        is_run ? reach_elevate_approved_run_path
                               : reach_elevate_approved_folder_path,
                        0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key,
                        nullptr) != ERROR_SUCCESS)
    {
        return 5;
    }

    BYTE data[12] = {};
    data[0] = enabled ? 0x02 : 0x03;
    if (!enabled)
    {
        FILETIME now = {};
        GetSystemTimeAsFileTime(&now);
        memcpy(&data[4], &now.dwLowDateTime, sizeof(now.dwLowDateTime));
        memcpy(&data[8], &now.dwHighDateTime, sizeof(now.dwHighDateTime));
    }

    LONG result = RegSetValueExW(key, name, 0, REG_BINARY, data, sizeof(data));
    RegCloseKey(key);
    return result == ERROR_SUCCESS ? 0 : 5;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, wchar_t *, int)
{
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr)
    {
        return 2;
    }

    int result = 2;
    if (argc == 5 && lstrcmpW(argv[1], L"startup-set-enabled") == 0)
    {
        result = reach_elevate_startup_set_enabled(argv[2], argv[3], argv[4]);
    }

    LocalFree(argv);
    return result;
}
