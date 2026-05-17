#include "reach/platform/windows_adapters.h"

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>

#include <new>

struct reach_app_launcher {
    int unused;
};

struct reach_explorer_window_search {
    HWND window;
};

static BOOL CALLBACK reach_find_explorer_window_proc(HWND hwnd, LPARAM param)
{
    reach_explorer_window_search *search = reinterpret_cast<reach_explorer_window_search *>(param);
    if (search == nullptr || hwnd == nullptr || !IsWindowVisible(hwnd)) {
        return TRUE;
    }

    wchar_t class_name[64] = {};
    GetClassNameW(hwnd, class_name, 64);
    if (lstrcmpiW(class_name, L"CabinetWClass") == 0 || lstrcmpiW(class_name, L"ExploreWClass") == 0) {
        search->window = hwnd;
        return FALSE;
    }

    return TRUE;
}

static HWND reach_find_explorer_window(void)
{
    reach_explorer_window_search search = {};
    EnumWindows(reach_find_explorer_window_proc, reinterpret_cast<LPARAM>(&search));
    return search.window;
}

static reach_result reach_activate_explorer_window(HWND window)
{
    if (window == nullptr || !IsWindow(window)) {
        return REACH_INVALID_ARGUMENT;
    }

    if (IsIconic(window)) {
        ShowWindow(window, SW_RESTORE);
    } else {
        ShowWindow(window, SW_SHOWNORMAL);
    }
    return SetForegroundWindow(window) ? REACH_OK : REACH_ERROR;
}

static int32_t reach_launch_request_is_default_explorer(const reach_app_launch_request *request)
{
    if (request == nullptr || request->path[0] == 0 || request->arguments[0] != 0) {
        return 0;
    }

    const wchar_t *path = reinterpret_cast<const wchar_t *>(request->path);
    const wchar_t *file_name = PathFindFileNameW(path);
    return file_name != nullptr && lstrcmpiW(file_name, L"explorer.exe") == 0;
}

static reach_result reach_launch_default_explorer(void)
{
    HWND existing = reach_find_explorer_window();
    if (existing != nullptr) {
        return reach_activate_explorer_window(existing);
    }

    SHELLEXECUTEINFOW execute = {};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_DEFAULT;
    execute.lpFile = L"explorer.exe";
    execute.lpParameters = L"shell:MyComputerFolder";
    execute.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&execute) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_app_launcher_launch(reach_app_launcher *launcher, const reach_app_launch_request *request)
{
    (void)launcher;
    if (request == nullptr || request->path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    if (reach_launch_request_is_default_explorer(request)) {
        return reach_launch_default_explorer();
    }

    SHELLEXECUTEINFOW execute = {};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_DEFAULT;
    execute.lpFile = reinterpret_cast<LPCWSTR>(request->path);
    execute.lpParameters = request->arguments[0] != 0 ? reinterpret_cast<LPCWSTR>(request->arguments) : nullptr;
    execute.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&execute) ? REACH_OK : REACH_ERROR;
}

static void reach_app_launcher_destroy(reach_app_launcher *launcher)
{
    delete launcher;
}

reach_result reach_windows_create_app_launcher(reach_app_launcher_port *out_port)
{
    if (out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_app_launcher *launcher = new (std::nothrow) reach_app_launcher();
    if (launcher == nullptr) {
        return REACH_ERROR;
    }

    out_port->launcher = launcher;
    out_port->ops.launch = reach_app_launcher_launch;
    out_port->ops.destroy = reach_app_launcher_destroy;
    return REACH_OK;
}
