#include "windows_adapters_internal.h"

#include "reach/ports/app_launcher.h"
#include "reach/ports/terminal_launcher.h"

#include <windows.h>
#include <wincrypt.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <cstring>
#include <cwchar>
#include <new>

struct reach_app_launcher
{
    int32_t reserved;
};

struct reach_terminal_launcher
{
    wchar_t executable[MAX_PATH];
    int32_t windows_terminal;
};

static reach_result reach_windows_shell_launch(const wchar_t *verb, const wchar_t *path,
                                               const wchar_t *arguments,
                                               const wchar_t *working_directory)
{
    SHELLEXECUTEINFOW execute = {};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_ASYNCOK | SEE_MASK_FLAG_NO_UI;
    execute.lpVerb = verb;
    execute.lpFile = path;
    execute.lpParameters = arguments;
    execute.lpDirectory = working_directory;
    execute.nShow = SW_SHOWNORMAL;

    (void)AllowSetForegroundWindow(ASFW_ANY);

    HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    BOOL launched = ShellExecuteExW(&execute);
    if (SUCCEEDED(com_result))
    {
        CoUninitialize();
    }

    return launched ? REACH_OK : REACH_ERROR;
}

static reach_result reach_app_launcher_launch(reach_app_launcher *launcher,
                                              const reach_app_launch_request *request)
{
    (void)launcher;
    if (request == nullptr || request->path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    wchar_t working_directory[MAX_PATH] = {};
    lstrcpynW(working_directory, reinterpret_cast<const wchar_t *>(request->path), MAX_PATH);
    const wchar_t *launch_directory =
        PathRemoveFileSpecW(working_directory) ? working_directory : nullptr;

    // Reach surfaces are mostly WS_EX_NOACTIVATE, so the launched process is not
    // "started by the foreground process" and Windows denies it foreground; the new
    // window then flashes and stays behind. Hand over our foreground rights (granted
    // by the user's click/keypress on reach) before launching.
    return reach_windows_shell_launch(request->run_as_admin ? L"runas" : nullptr,
                                      reinterpret_cast<const wchar_t *>(request->path),
                                      request->arguments[0] != 0
                                          ? reinterpret_cast<const wchar_t *>(request->arguments)
                                          : nullptr,
                                      launch_directory);
}

static reach_result reach_terminal_build_windows_terminal_arguments(const uint16_t *command,
                                                                    const wchar_t *home_directory,
                                                                    const wchar_t *default_profile,
                                                                    wchar_t *out_arguments,
                                                                    size_t arguments_capacity)
{
    if (command == nullptr || home_directory == nullptr || home_directory[0] == 0 ||
        out_arguments == nullptr || arguments_capacity == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    size_t command_length = 0;
    while (command[command_length] != 0 && command_length < REACH_TERMINAL_COMMAND_CAPACITY - 1)
    {
        ++command_length;
    }
    if (command_length == 0)
    {
        int written = swprintf_s(out_arguments, arguments_capacity,
                                 L"new-tab --startingDirectory \"%ls\"", home_directory);
        return written > 0 ? REACH_OK : REACH_ERROR;
    }

    wchar_t encoded[1024] = {};
    DWORD encoded_capacity = (DWORD)(sizeof(encoded) / sizeof(encoded[0]));
    if (!CryptBinaryToStringW(
            reinterpret_cast<const BYTE *>(command), (DWORD)(command_length * sizeof(uint16_t)),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, encoded, &encoded_capacity))
    {
        return REACH_ERROR;
    }

    if (default_profile == nullptr || default_profile[0] == 0)
    {
        return REACH_ERROR;
    }

    int written = swprintf_s(out_arguments, arguments_capacity,
                             L"new-tab --startingDirectory \"%ls\" --profile %ls "
                             L"--appendCommandLine -- -NoExit -EncodedCommand %ls",
                             home_directory, default_profile, encoded);
    return written > 0 ? REACH_OK : REACH_ERROR;
}

static int32_t reach_terminal_json_whitespace(char value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

static reach_result reach_terminal_read_default_profile(const wchar_t *settings_path,
                                                        wchar_t *out_profile,
                                                        size_t profile_capacity)
{
    if (settings_path == nullptr || out_profile == nullptr || profile_capacity == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    out_profile[0] = 0;
    HANDLE file = CreateFileW(settings_path, GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return REACH_ERROR;
    }

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 1024 * 1024)
    {
        CloseHandle(file);
        return REACH_ERROR;
    }

    char *content = new (std::nothrow) char[(size_t)size.QuadPart + 1];
    if (content == nullptr)
    {
        CloseHandle(file);
        return REACH_ERROR;
    }

    DWORD bytes_read = 0;
    BOOL read = ReadFile(file, content, (DWORD)size.QuadPart, &bytes_read, nullptr);
    CloseHandle(file);
    if (!read || bytes_read != (DWORD)size.QuadPart)
    {
        delete[] content;
        return REACH_ERROR;
    }
    content[bytes_read] = 0;

    const char key[] = "\"defaultProfile\"";
    char *cursor = std::strstr(content, key);
    if (cursor == nullptr)
    {
        delete[] content;
        return REACH_ERROR;
    }

    cursor += sizeof(key) - 1;
    while (reach_terminal_json_whitespace(*cursor))
    {
        ++cursor;
    }
    if (*cursor != ':')
    {
        delete[] content;
        return REACH_ERROR;
    }

    ++cursor;
    while (reach_terminal_json_whitespace(*cursor))
    {
        ++cursor;
    }
    if (*cursor != '"')
    {
        delete[] content;
        return REACH_ERROR;
    }

    ++cursor;
    wchar_t profile_value[64] = {};
    size_t profile_length = 0;
    while (*cursor != 0 && *cursor != '"' && profile_length + 1 < 64)
    {
        unsigned char value = (unsigned char)*cursor++;
        if (value > 0x7f)
        {
            delete[] content;
            return REACH_ERROR;
        }
        profile_value[profile_length++] = (wchar_t)value;
    }
    int32_t profile_closed = *cursor == '"';
    delete[] content;
    if (!profile_closed)
    {
        return REACH_ERROR;
    }

    GUID profile_guid = {};
    if (FAILED(CLSIDFromString(profile_value, &profile_guid)) ||
        StringFromGUID2(profile_guid, out_profile, (int)profile_capacity) == 0)
    {
        out_profile[0] = 0;
        return REACH_ERROR;
    }
    return REACH_OK;
}

static int32_t reach_terminal_settings_candidate(const wchar_t *local_app_data,
                                                 const wchar_t *relative_path, wchar_t *out_path)
{
    if (wcscpy_s(out_path, MAX_PATH, local_app_data) != 0 || !PathAppendW(out_path, relative_path))
    {
        return 0;
    }
    DWORD attributes = GetFileAttributesW(out_path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static reach_result reach_terminal_find_settings(const wchar_t *terminal_executable,
                                                 wchar_t *out_path)
{
    if (terminal_executable == nullptr || out_path == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    PWSTR local_app_data = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr,
                                    &local_app_data)) ||
        local_app_data == nullptr)
    {
        return REACH_ERROR;
    }

    const wchar_t *unpackaged = L"Microsoft\\Windows Terminal\\settings.json";
    const wchar_t *stable =
        L"Packages\\Microsoft.WindowsTerminal_8wekyb3d8bbwe\\LocalState\\settings.json";
    const wchar_t *preview =
        L"Packages\\Microsoft.WindowsTerminalPreview_8wekyb3d8bbwe\\LocalState\\settings.json";
    const wchar_t *canary =
        L"Packages\\Microsoft.WindowsTerminalCanary_8wekyb3d8bbwe\\LocalState\\settings.json";
    int32_t packaged = StrStrIW(terminal_executable, L"\\Microsoft\\WindowsApps\\") != nullptr;
    const wchar_t *candidates[] = {packaged ? stable : unpackaged, preview, canary,
                                   packaged ? unpackaged : stable};

    reach_result result = REACH_ERROR;
    for (const wchar_t *candidate : candidates)
    {
        if (reach_terminal_settings_candidate(local_app_data, candidate, out_path))
        {
            result = REACH_OK;
            break;
        }
    }
    CoTaskMemFree(local_app_data);
    return result;
}

static reach_result reach_terminal_home_directory(wchar_t *out_path)
{
    if (out_path == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    PWSTR home_directory = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Profile, KF_FLAG_DEFAULT, nullptr, &home_directory)) ||
        home_directory == nullptr)
    {
        return REACH_ERROR;
    }

    errno_t copied = wcscpy_s(out_path, MAX_PATH, home_directory);
    CoTaskMemFree(home_directory);
    return copied == 0 ? REACH_OK : REACH_ERROR;
}

static reach_result reach_terminal_build_cmd_arguments(const uint16_t *command,
                                                       wchar_t *out_arguments,
                                                       size_t arguments_capacity)
{
    if (command == nullptr || out_arguments == nullptr || arguments_capacity == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (command[0] == 0)
    {
        out_arguments[0] = 0;
        return REACH_OK;
    }

    int written = swprintf_s(out_arguments, arguments_capacity, L"/D /K %ls",
                             reinterpret_cast<const wchar_t *>(command));
    return written > 0 ? REACH_OK : REACH_ERROR;
}

static reach_result reach_terminal_launcher_launch(reach_terminal_launcher *launcher,
                                                   const reach_terminal_launch_request *request)
{
    if (launcher == nullptr || request == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    wchar_t home_directory[MAX_PATH] = {};
    reach_result result = reach_terminal_home_directory(home_directory);
    if (result != REACH_OK)
    {
        return result;
    }

    wchar_t arguments[1200] = {};
    if (launcher->windows_terminal)
    {
        wchar_t settings_path[MAX_PATH] = {};
        wchar_t default_profile[64] = {};
        if (request->command[0] != 0 &&
            (reach_terminal_find_settings(launcher->executable, settings_path) != REACH_OK ||
             reach_terminal_read_default_profile(
                 settings_path, default_profile,
                 sizeof(default_profile) / sizeof(default_profile[0])) != REACH_OK))
        {
            return REACH_ERROR;
        }
        result = reach_terminal_build_windows_terminal_arguments(
            request->command, home_directory, default_profile, arguments,
            sizeof(arguments) / sizeof(arguments[0]));
    }
    else
    {
        result = reach_terminal_build_cmd_arguments(request->command, arguments,
                                                    sizeof(arguments) / sizeof(arguments[0]));
    }
    if (result != REACH_OK)
    {
        return result;
    }

    return reach_windows_shell_launch(nullptr, launcher->executable,
                                      arguments[0] != 0 ? arguments : nullptr, home_directory);
}

static reach_result reach_terminal_launcher_icon_ref(reach_terminal_launcher *launcher,
                                                     uint16_t *out_ref, size_t ref_capacity)
{
    if (launcher == nullptr || out_ref == nullptr || ref_capacity == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_copy_utf16(out_ref, ref_capacity,
                     reinterpret_cast<const uint16_t *>(launcher->executable));
    return REACH_OK;
}

static void reach_terminal_launcher_destroy(reach_terminal_launcher *launcher)
{
    delete launcher;
}

static void reach_app_launcher_destroy(reach_app_launcher *launcher)
{
    delete launcher;
}

reach_result reach_windows_create_app_launcher(reach_app_launcher_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_app_launcher *launcher = new (std::nothrow) reach_app_launcher();
    if (launcher == nullptr)
    {
        return REACH_ERROR;
    }

    out_port->launcher = launcher;
    out_port->ops.launch = reach_app_launcher_launch;
    out_port->ops.destroy = reach_app_launcher_destroy;
    return REACH_OK;
}

reach_result reach_windows_create_terminal_launcher(reach_terminal_launcher_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_terminal_launcher *launcher = new (std::nothrow) reach_terminal_launcher();
    if (launcher == nullptr)
    {
        return REACH_ERROR;
    }

    DWORD resolved =
        SearchPathW(nullptr, L"wt.exe", nullptr, MAX_PATH, launcher->executable, nullptr);
    launcher->windows_terminal = resolved > 0 && resolved < MAX_PATH;
    if (!launcher->windows_terminal)
    {
        UINT system_length = GetSystemDirectoryW(launcher->executable, MAX_PATH);
        if (system_length == 0 || system_length >= MAX_PATH ||
            !PathAppendW(launcher->executable, L"cmd.exe"))
        {
            delete launcher;
            return REACH_ERROR;
        }
    }

    out_port->launcher = launcher;
    out_port->ops.launch = reach_terminal_launcher_launch;
    out_port->ops.icon_ref = reach_terminal_launcher_icon_ref;
    out_port->ops.destroy = reach_terminal_launcher_destroy;
    return REACH_OK;
}
