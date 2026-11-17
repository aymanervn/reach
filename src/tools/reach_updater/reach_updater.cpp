#include "reach/adapters/windows/diagnostics_log.h"
#include "reach/support/util.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <stdarg.h>
#include <stdio.h>

#include <string>
#include <vector>

static const wchar_t *const reach_updater_backup_name = L"previous";
static const wchar_t *const reach_updater_stale_backup_prefix = L"previous.stale.";
static const DWORD reach_updater_exit_timeout_ms = 5000;

static void reach_updater_log(int32_t error, const char *format, ...)
{
    char message[512] = {};
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    if (error)
    {
        reach_log_error(message);
    }
    else
    {
        reach_log_info(message);
    }
}

static std::string reach_updater_narrow(const std::wstring &text)
{
    if (text.empty())
    {
        return std::string();
    }
    int needed = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0,
                                     nullptr, nullptr);
    if (needed <= 0)
    {
        return std::string();
    }
    std::string result((size_t)needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &result[0], needed, nullptr,
                        nullptr);
    return result;
}

static int32_t reach_updater_is_elevated(void)
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        return 0;
    }
    TOKEN_ELEVATION elevation = {};
    DWORD size = sizeof(elevation);
    BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
    CloseHandle(token);
    return ok && elevation.TokenIsElevated;
}

static DWORD reach_updater_run_wait(const wchar_t *application, const wchar_t *arguments)
{
    std::wstring command;
    command += L"\"";
    command += application;
    command += L"\"";
    if (arguments != nullptr && arguments[0] != 0)
    {
        command += L" ";
        command += arguments;
    }

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process = {};
    std::wstring buffer = command;
    if (!CreateProcessW(nullptr, &buffer[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        nullptr, &startup, &process))
    {
        return (DWORD)-1;
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = (DWORD)-1;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    return exit_code;
}

static void reach_updater_terminate(const wchar_t *image_name)
{
    reach_updater_run_wait(L"C:\\Windows\\System32\\taskkill.exe",
                           (std::wstring(L"/F /IM ") + image_name).c_str());
}

static DWORD reach_updater_find_session_process(const wchar_t *const *names, size_t name_count)
{
    DWORD session = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session))
    {
        return 0;
    }
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    DWORD found = 0;
    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    for (BOOL more = Process32FirstW(snapshot, &entry); more && found == 0;
         more = Process32NextW(snapshot, &entry))
    {
        DWORD process_session = 0;
        if (!ProcessIdToSessionId(entry.th32ProcessID, &process_session) ||
            process_session != session)
        {
            continue;
        }
        for (size_t index = 0; index < name_count; ++index)
        {
            if (lstrcmpiW(entry.szExeFile, names[index]) == 0)
            {
                found = entry.th32ProcessID;
                break;
            }
        }
    }
    CloseHandle(snapshot);
    return found;
}

static void reach_updater_wait_for_exit(const wchar_t *const *names, size_t name_count)
{
    ULONGLONG deadline = GetTickCount64() + reach_updater_exit_timeout_ms;
    for (;;)
    {
        DWORD process_id = reach_updater_find_session_process(names, name_count);
        if (process_id == 0)
        {
            return;
        }
        ULONGLONG now = GetTickCount64();
        if (now >= deadline)
        {
            reach_updater_log(0, "update wait_timeout pid=%lu", (unsigned long)process_id);
            return;
        }
        DWORD remaining = (DWORD)(deadline - now);
        HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, process_id);
        if (process != nullptr)
        {
            WaitForSingleObject(process, remaining);
            CloseHandle(process);
        }
        else
        {
            Sleep(remaining < 50 ? remaining : 50);
        }
    }
}

static void reach_updater_collect_files(const std::wstring &root, const std::wstring &relative,
                                        std::vector<std::wstring> *out_files)
{
    std::wstring directory = relative.empty() ? root : root + L"\\" + relative;
    WIN32_FIND_DATAW find = {};
    HANDLE handle = FindFirstFileW((directory + L"\\*").c_str(), &find);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return;
    }
    do
    {
        if (lstrcmpW(find.cFileName, L".") == 0 || lstrcmpW(find.cFileName, L"..") == 0)
        {
            continue;
        }
        std::wstring child = relative.empty() ? find.cFileName : relative + L"\\" + find.cFileName;
        if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            reach_updater_collect_files(root, child, out_files);
        }
        else
        {
            out_files->push_back(child);
        }
    } while (FindNextFileW(handle, &find));
    FindClose(handle);
}

static std::wstring reach_updater_temp_dir(void)
{
    wchar_t temp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, temp);
    return std::wstring(temp) + L"reach_update_extract";
}

static void reach_updater_remove_tree(const std::wstring &path)
{
    std::wstring pattern = path + L"\\*";
    WIN32_FIND_DATAW find = {};
    HANDLE handle = FindFirstFileW(pattern.c_str(), &find);
    if (handle != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (lstrcmpW(find.cFileName, L".") == 0 || lstrcmpW(find.cFileName, L"..") == 0)
            {
                continue;
            }
            std::wstring child = path + L"\\" + find.cFileName;
            if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                reach_updater_remove_tree(child);
            }
            else
            {
                DeleteFileW(child.c_str());
            }
        } while (FindNextFileW(handle, &find));
        FindClose(handle);
    }
    RemoveDirectoryW(path.c_str());
}

static int32_t reach_updater_path_exists(const std::wstring &path)
{
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static void reach_updater_ensure_parent(const std::wstring &path)
{
    size_t separator = path.find_last_of(L'\\');
    if (separator != std::wstring::npos)
    {
        SHCreateDirectoryExW(nullptr, path.substr(0, separator).c_str(), nullptr);
    }
}

static DWORD reach_updater_probe_write(const std::wstring &target_dir)
{
    std::wstring probe = target_dir + L"\\.reach_update_probe";
    HANDLE file = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return GetLastError();
    }
    CloseHandle(file);
    return ERROR_SUCCESS;
}

static void reach_updater_remove_stale_backups(const std::wstring &target_dir)
{
    WIN32_FIND_DATAW find = {};
    HANDLE handle = FindFirstFileW(
        (target_dir + L"\\" + reach_updater_stale_backup_prefix + L"*").c_str(), &find);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return;
    }
    do
    {
        if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            reach_updater_remove_tree(target_dir + L"\\" + find.cFileName);
        }
    } while (FindNextFileW(handle, &find));
    FindClose(handle);
}

static void reach_updater_prepare_backup(const std::wstring &target_dir,
                                         const std::wstring &backup_dir)
{
    reach_updater_remove_stale_backups(target_dir);
    reach_updater_remove_tree(backup_dir);
    if (reach_updater_path_exists(backup_dir))
    {
        std::wstring stale = target_dir + L"\\" + reach_updater_stale_backup_prefix +
                             std::to_wstring(GetTickCount64());
        if (!MoveFileExW(backup_dir.c_str(), stale.c_str(), 0))
        {
            reach_updater_log(1, "update backup_clear_failed error=%lu",
                              (unsigned long)GetLastError());
        }
    }
    CreateDirectoryW(backup_dir.c_str(), nullptr);
}

static size_t reach_updater_install_files(const std::wstring &source_dir,
                                          const std::wstring &target_dir,
                                          const std::wstring &backup_dir,
                                          const std::vector<std::wstring> &files)
{
    size_t failures = 0;
    for (const std::wstring &relative : files)
    {
        std::wstring source = source_dir + L"\\" + relative;
        std::wstring target = target_dir + L"\\" + relative;
        std::wstring backup = backup_dir + L"\\" + relative;
        if (reach_updater_path_exists(target))
        {
            reach_updater_ensure_parent(backup);
            if (!MoveFileExW(target.c_str(), backup.c_str(), MOVEFILE_REPLACE_EXISTING))
            {
                reach_updater_log(1, "update backup_failed file=%s error=%lu",
                                  reach_updater_narrow(relative).c_str(),
                                  (unsigned long)GetLastError());
            }
        }
        reach_updater_ensure_parent(target);
        if (!CopyFileW(source.c_str(), target.c_str(), FALSE))
        {
            ++failures;
            reach_updater_log(1, "update copy_failed file=%s error=%lu",
                              reach_updater_narrow(relative).c_str(),
                              (unsigned long)GetLastError());
        }
    }
    return failures;
}

static int32_t reach_updater_relaunch_elevated(const std::wstring &zip_path,
                                               const std::wstring &target_dir,
                                               int32_t reopen_settings)
{
    wchar_t self[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring parameters = L"\"" + zip_path + L"\" \"" + target_dir + L"\" " +
                              (reopen_settings ? L"1" : L"0") + L" --elevated";
    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas";
    info.lpFile = self;
    info.lpParameters = parameters.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info) || info.hProcess == nullptr)
    {
        return 0;
    }
    WaitForSingleObject(info.hProcess, INFINITE);
    CloseHandle(info.hProcess);
    return 1;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, wchar_t *, int)
{
    reach_diagnostics_log_install("updater");

    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr || argc < 4)
    {
        if (argv != nullptr)
        {
            LocalFree(argv);
        }
        reach_updater_log(1, "update refused reason=arguments");
        return 2;
    }

    std::wstring zip_path = argv[1];
    std::wstring target_dir = argv[2];
    int32_t reopen_settings = lstrcmpW(argv[3], L"1") == 0;
    int32_t already_elevated = argc >= 5 && lstrcmpW(argv[4], L"--elevated") == 0;
    LocalFree(argv);

    reach_updater_log(0, "update start target=%s elevated=%d",
                      reach_updater_narrow(target_dir).c_str(),
                      already_elevated || reach_updater_is_elevated());

    DWORD write_error = reach_updater_probe_write(target_dir);
    if (write_error != ERROR_SUCCESS)
    {
        if (write_error == ERROR_ACCESS_DENIED && !already_elevated && !reach_updater_is_elevated())
        {
            reach_updater_log(0, "update elevate reason=access_denied");
            if (!reach_updater_relaunch_elevated(zip_path, target_dir, reopen_settings))
            {
                reach_updater_log(1, "update stop reason=elevation_declined");
            }
            return 0;
        }
        reach_updater_log(1, "update stop reason=target_not_writable error=%lu",
                          (unsigned long)write_error);
        return 4;
    }

    std::wstring extract_dir = reach_updater_temp_dir();
    reach_updater_remove_tree(extract_dir);
    CreateDirectoryW(extract_dir.c_str(), nullptr);

    std::wstring tar_args = L"-xf \"" + zip_path + L"\" -C \"" + extract_dir + L"\"";
    DWORD tar_result = reach_updater_run_wait(L"C:\\Windows\\System32\\tar.exe", tar_args.c_str());
    if (tar_result != 0)
    {
        reach_updater_log(1, "update stop reason=extract_failed code=%lu",
                          (unsigned long)tar_result);
        return 3;
    }

    std::wstring source_dir = extract_dir + L"\\reach";
    if (!reach_updater_path_exists(source_dir))
    {
        source_dir = extract_dir;
    }
    std::vector<std::wstring> files;
    reach_updater_collect_files(source_dir, std::wstring(), &files);
    if (files.empty())
    {
        reach_updater_log(1, "update stop reason=empty_package");
        return 3;
    }

    reach_updater_run_wait(L"C:\\Windows\\System32\\schtasks.exe", L"/End /TN \"ReachWatchdog\"");
    reach_updater_terminate(L"reachSetting.exe");
    reach_updater_terminate(L"reach.exe");
    reach_updater_terminate(L"reachService.exe");
    const wchar_t *const exiting[] = {L"reachWatchdog.exe", L"reachService.exe",
                                      L"reachSetting.exe"};
    reach_updater_wait_for_exit(exiting, ARRAYSIZE(exiting));

    std::wstring backup_dir = target_dir + L"\\" + reach_updater_backup_name;
    reach_updater_prepare_backup(target_dir, backup_dir);
    size_t failures = reach_updater_install_files(source_dir, target_dir, backup_dir, files);
    reach_updater_log(failures != 0, "update installed files=%zu failed=%zu", files.size(),
                      failures);

    reach_updater_remove_tree(extract_dir);

    std::wstring reachctl = target_dir + L"\\reachctl.exe";
    DWORD start_result = reach_updater_run_wait(reachctl.c_str(), L"--start");
    reach_updater_log(start_result != 0, "update restart code=%lu", (unsigned long)start_result);

    if (reopen_settings)
    {
        std::wstring settings = target_dir + L"\\reachSetting.exe";
        STARTUPINFOW startup = {};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process = {};
        std::wstring command = L"\"" + settings + L"\"";
        std::wstring buffer = command;
        if (CreateProcessW(nullptr, &buffer[0], nullptr, nullptr, FALSE, 0, nullptr,
                           target_dir.c_str(), &startup, &process))
        {
            CloseHandle(process.hProcess);
            CloseHandle(process.hThread);
        }
    }

    return failures == 0 ? 0 : 5;
}
