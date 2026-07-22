#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>

#include <string>

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

static int32_t reach_updater_copy_tree(const std::wstring &source, const std::wstring &destination,
                                       int32_t *out_access_denied)
{
    CreateDirectoryW(destination.c_str(), nullptr);

    std::wstring pattern = source + L"\\*";
    WIN32_FIND_DATAW find = {};
    HANDLE handle = FindFirstFileW(pattern.c_str(), &find);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return 1;
    }

    int32_t ok = 1;
    do
    {
        if (lstrcmpW(find.cFileName, L".") == 0 || lstrcmpW(find.cFileName, L"..") == 0)
        {
            continue;
        }
        std::wstring child_source = source + L"\\" + find.cFileName;
        std::wstring child_destination = destination + L"\\" + find.cFileName;
        if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (!reach_updater_copy_tree(child_source, child_destination, out_access_denied))
            {
                ok = 0;
            }
        }
        else
        {
            if (!CopyFileW(child_source.c_str(), child_destination.c_str(), FALSE))
            {
                DWORD error = GetLastError();
                if (error == ERROR_ACCESS_DENIED)
                {
                    *out_access_denied = 1;
                }
                ok = 0;
            }
        }
    } while (FindNextFileW(handle, &find));
    FindClose(handle);
    return ok;
}

static std::wstring reach_updater_temp_dir(void)
{
    wchar_t temp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, temp);
    std::wstring dir = std::wstring(temp) + L"reach_update_extract";
    return dir;
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

int WINAPI wWinMain(HINSTANCE, HINSTANCE, wchar_t *, int)
{
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr || argc < 4)
    {
        if (argv != nullptr)
        {
            LocalFree(argv);
        }
        return 2;
    }

    std::wstring zip_path = argv[1];
    std::wstring target_dir = argv[2];
    int32_t reopen_settings = lstrcmpW(argv[3], L"1") == 0;
    int32_t already_elevated = argc >= 5 && lstrcmpW(argv[4], L"--elevated") == 0;
    LocalFree(argv);

    std::wstring extract_dir = reach_updater_temp_dir();
    reach_updater_remove_tree(extract_dir);
    CreateDirectoryW(extract_dir.c_str(), nullptr);

    std::wstring tar_args = L"-xf \"" + zip_path + L"\" -C \"" + extract_dir + L"\"";
    if (reach_updater_run_wait(L"C:\\Windows\\System32\\tar.exe", tar_args.c_str()) != 0)
    {
        return 3;
    }

    std::wstring source_dir = extract_dir + L"\\reach";
    if (GetFileAttributesW(source_dir.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        source_dir = extract_dir;
    }

    reach_updater_run_wait(L"C:\\Windows\\System32\\schtasks.exe", L"/End /TN \"ReachWatchdog\"");
    reach_updater_terminate(L"reachSetting.exe");
    reach_updater_terminate(L"reach.exe");
    reach_updater_terminate(L"reachService.exe");
    Sleep(1500);

    int32_t access_denied = 0;
    reach_updater_copy_tree(source_dir, target_dir, &access_denied);

    if (access_denied && !already_elevated && !reach_updater_is_elevated())
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
        if (ShellExecuteExW(&info) && info.hProcess != nullptr)
        {
            WaitForSingleObject(info.hProcess, INFINITE);
            CloseHandle(info.hProcess);
        }
        return 0;
    }

    reach_updater_remove_tree(extract_dir);

    std::wstring reachctl = target_dir + L"\\reachctl.exe";
    reach_updater_run_wait(reachctl.c_str(), L"--start");

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

    return 0;
}
