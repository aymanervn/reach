#include <windows.h>
#include <shlwapi.h>

#include <cwchar>

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
                L"CurrentShell=%ls PreviousShell=%ls ReachIsShell=%d",
                reinterpret_cast<const wchar_t *>(status.current_shell),
                reinterpret_cast<const wchar_t *>(status.previous_shell),
                status.reach_is_shell);
            reachctl_print(line);
            return 0;
        }
    }

    reachctl_print(L"Usage: reachctl.exe --install-shell | --restore-shell | --print-shell-status");
    return 2;
}
