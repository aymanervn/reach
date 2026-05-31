#include "windows_adapters_internal.h"

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>

#include <new>

struct reach_app_launcher {
    int unused;
};

struct reach_launch_window_search {
    DWORD process_id;
    const uint16_t *path;
    HWND window;
};

struct reach_launch_activation_context {
    HANDLE process;
    uint16_t path[260];
};

static const wchar_t *reach_win32_file_name(const wchar_t *path)
{
    if (path == nullptr) {
        return nullptr;
    }

    const wchar_t *file_name = path;
    for (const wchar_t *scan = path; *scan != 0; ++scan) {
        if (*scan == L'\\' || *scan == L'/') {
            file_name = scan + 1;
        }
    }
    return file_name;
}

static int32_t reach_window_process_is_explorer(HWND hwnd)
{
    DWORD process_id = 0;
    GetWindowThreadProcessId(hwnd, &process_id);
    if (process_id == 0 || process_id == GetCurrentProcessId()) {
        return 0;
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr) {
        return 0;
    }

    wchar_t image_path[260] = {};
    DWORD image_path_count = 260;
    BOOL ok = QueryFullProcessImageNameW(process, 0, image_path, &image_path_count);
    CloseHandle(process);
    if (!ok || image_path[0] == 0) {
        return 0;
    }

    const wchar_t *file_name = reach_win32_file_name(image_path);
    return file_name != nullptr && lstrcmpiW(file_name, L"explorer.exe") == 0;
}

static int32_t reach_window_is_minimized(HWND hwnd)
{
    if (IsIconic(hwnd)) {
        return 1;
    }

    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    return GetWindowPlacement(hwnd, &placement) &&
        placement.showCmd == SW_SHOWMINIMIZED;
}

static int32_t reach_window_process_path_equals(HWND hwnd, const uint16_t *path)
{
    if (hwnd == nullptr || path == nullptr || path[0] == 0) {
        return 0;
    }

    DWORD process_id = 0;
    GetWindowThreadProcessId(hwnd, &process_id);
    if (process_id == 0 || process_id == GetCurrentProcessId()) {
        return 0;
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr) {
        return 0;
    }

    wchar_t image_path[260] = {};
    DWORD image_path_count = 260;
    BOOL ok = QueryFullProcessImageNameW(process, 0, image_path, &image_path_count);
    CloseHandle(process);
    if (!ok || image_path[0] == 0) {
        return 0;
    }

    return lstrcmpiW(image_path, reinterpret_cast<const wchar_t *>(path)) == 0;
}

static int32_t reach_launched_window_matches(HWND hwnd, const reach_launch_window_search *search)
{
    if (hwnd == nullptr || search == nullptr) {
        return 0;
    }

    DWORD window_process_id = 0;
    GetWindowThreadProcessId(hwnd, &window_process_id);
    if (search->process_id != 0 && window_process_id == search->process_id) {
        return 1;
    }

    return reach_window_process_path_equals(hwnd, search->path);
}

static int32_t reach_is_usable_launched_window(HWND hwnd, const reach_launch_window_search *search)
{
    if (hwnd == nullptr || !IsWindow(hwnd) || search == nullptr) {
        return 0;
    }

    if (!reach_launched_window_matches(hwnd, search)) {
        return 0;
    }

    if (GetAncestor(hwnd, GA_ROOT) != hwnd) {
        return 0;
    }

    if (!IsWindowVisible(hwnd) && !reach_window_is_minimized(hwnd)) {
        return 0;
    }

    if (GetWindow(hwnd, GW_OWNER) != nullptr) {
        return 0;
    }

    wchar_t class_name[64] = {};
    GetClassNameW(hwnd, class_name, 64);
    if (lstrcmpiW(class_name, L"ApplicationFrameWindow") == 0 ||
        lstrcmpiW(class_name, L"CabinetWClass") == 0 ||
        lstrcmpiW(class_name, L"Chrome_WidgetWin_1") == 0 ||
        lstrcmpiW(class_name, L"MozillaWindowClass") == 0 ||
        lstrcmpiW(class_name, L"Notepad") == 0) {
        return 1;
    }

    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    return (style & WS_CAPTION) != 0;
}

static BOOL CALLBACK reach_find_launched_window_proc(HWND hwnd, LPARAM param)
{
    reach_launch_window_search *search =
        reinterpret_cast<reach_launch_window_search *>(param);
    if (search == nullptr) {
        return FALSE;
    }

    if (reach_is_usable_launched_window(hwnd, search)) {
        search->window = hwnd;
        return FALSE;
    }

    return TRUE;
}

static HWND reach_find_launched_window(DWORD process_id, const uint16_t *path)
{
    reach_launch_window_search search = {};
    search.process_id = process_id;
    search.path = path;
    EnumWindows(reach_find_launched_window_proc,
                reinterpret_cast<LPARAM>(&search));
    return search.window;
}

static void reach_activate_launched_process_window(reach_launch_activation_context *context)
{
    if (context == nullptr) {
        return;
    }

    DWORD process_id = context->process != nullptr ? GetProcessId(context->process) : 0;
    if (process_id == 0 && context->path[0] == 0) {
        return;
    }

    if (process_id != 0) {
        AllowSetForegroundWindow(process_id);
    }
    if (context->process != nullptr) {
        WaitForInputIdle(context->process, 3000);
    }

    HWND hwnd = nullptr;
    for (int attempt = 0; attempt < 50 && hwnd == nullptr; ++attempt) {
        hwnd = reach_find_launched_window(process_id, context->path);
        if (hwnd == nullptr) {
            Sleep(100);
        }
    }

    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return;
    }

    if (reach_window_is_minimized(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    } else {
        ShowWindow(hwnd, SW_SHOWNORMAL);
    }

    HWND foreground = GetForegroundWindow();
    DWORD foreground_thread =
        foreground != nullptr ? GetWindowThreadProcessId(foreground, nullptr) : 0;
    DWORD window_thread = GetWindowThreadProcessId(hwnd, nullptr);
    DWORD current_thread = GetCurrentThreadId();

    bool attached_foreground = false;
    bool attached_window = false;

    if (foreground_thread != 0 && foreground_thread != current_thread) {
        attached_foreground =
            AttachThreadInput(current_thread, foreground_thread, TRUE) != FALSE;
    }
    if (window_thread != 0 && window_thread != current_thread &&
        window_thread != foreground_thread) {
        attached_window =
            AttachThreadInput(current_thread, window_thread, TRUE) != FALSE;
    }

    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    BringWindowToTop(hwnd);
    (void)SetForegroundWindow(hwnd);
    SwitchToThisWindow(hwnd, TRUE);

    if (attached_window) {
        AttachThreadInput(current_thread, window_thread, FALSE);
    }
    if (attached_foreground) {
        AttachThreadInput(current_thread, foreground_thread, FALSE);
    }
}

static DWORD WINAPI reach_activate_launched_process_window_thread(void *param)
{
    reach_launch_activation_context *context =
        static_cast<reach_launch_activation_context *>(param);
    reach_activate_launched_process_window(context);
    if (context != nullptr) {
        if (context->process != nullptr) {
            CloseHandle(context->process);
        }
        delete context;
    }
    return 0;
}

static void reach_activate_launched_process_window_async(HANDLE process, const uint16_t *path)
{
    if (process == nullptr && (path == nullptr || path[0] == 0)) {
        return;
    }

    reach_launch_activation_context *context = new (std::nothrow) reach_launch_activation_context();
    if (context == nullptr) {
        if (process != nullptr) {
            CloseHandle(process);
        }
        return;
    }

    context->process = process;
    if (path != nullptr && path[0] != 0) {
        (void)reach_copy_utf16(context->path, 260, path);
    }

    HANDLE thread = CreateThread(
        nullptr,
        0,
        reach_activate_launched_process_window_thread,
        context,
        0,
        nullptr);
    if (thread != nullptr) {
        CloseHandle(thread);
        return;
    }

    if (context->process != nullptr) {
        CloseHandle(context->process);
    }
    delete context;
}

static int32_t reach_is_usable_explorer_folder_window(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return 0;
    }
    if (GetAncestor(hwnd, GA_ROOT) != hwnd) {
        return 0;
    }
    if (!IsWindowVisible(hwnd) && !reach_window_is_minimized(hwnd)) {
        return 0;
    }

    wchar_t class_name[64] = {};
    GetClassNameW(hwnd, class_name, 64);
    if (lstrcmpiW(class_name, L"CabinetWClass") != 0 &&
        lstrcmpiW(class_name, L"ExploreWClass") != 0) {
        return 0;
    }

    return reach_window_process_is_explorer(hwnd);
}

static BOOL CALLBACK reach_find_usable_explorer_folder_window_proc(HWND hwnd, LPARAM param)
{
    HWND *out_window = reinterpret_cast<HWND *>(param);
    if (out_window == nullptr) {
        return FALSE;
    }

    if (reach_is_usable_explorer_folder_window(hwnd)) {
        *out_window = hwnd;
        return FALSE;
    }

    return TRUE;
}

static HWND reach_find_usable_explorer_folder_window(void)
{
    HWND window = nullptr;
    EnumWindows(
        reach_find_usable_explorer_folder_window_proc,
        reinterpret_cast<LPARAM>(&window)
    );
    return window;
}

static reach_result reach_activate_explorer_folder_window(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return REACH_INVALID_ARGUMENT;
    }

    if (reach_window_is_minimized(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    } else if (!IsWindowVisible(hwnd)) {
        ShowWindow(hwnd, SW_SHOW);
    }

    SetWindowPos(
        hwnd,
        HWND_TOP,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW
    );

    (void)SetForegroundWindow(hwnd);
    return REACH_OK;
}

static int32_t reach_is_default_explorer_launch_request(const reach_app_launch_request *request)
{
    if (request == nullptr ||
        request->path[0] == 0 ||
        request->arguments[0] != 0 ||
        request->force_new_instance||
        request->run_as_admin) {
        return 0;
    }

    const wchar_t *path = reinterpret_cast<const wchar_t *>(request->path);
    const wchar_t *file_name = reach_win32_file_name(path);
    return file_name != nullptr && lstrcmpiW(file_name, L"explorer.exe") == 0;
}

static reach_result reach_launch_default_explorer_folder(void)
{
    SHELLEXECUTEINFOW execute = {};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_NOCLOSEPROCESS;
    execute.lpFile = L"shell:MyComputerFolder";
    execute.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&execute)) {
        return REACH_ERROR;
    }

    reach_activate_launched_process_window_async(execute.hProcess, nullptr);
    return REACH_OK;
}

static int32_t reach_parent_directory(const uint16_t *path, uint16_t *out_directory, DWORD out_count)
{
    if (path == nullptr || path[0] == 0 || out_directory == nullptr || out_count == 0) {
        return 0;
    }

    if (reach_copy_utf16(out_directory, out_count, path) != REACH_OK) {
        return 0;
    }

    wchar_t *directory = reinterpret_cast<wchar_t *>(out_directory);
    return PathRemoveFileSpecW(directory) && directory[0] != 0;
}

static reach_result reach_app_launcher_launch(reach_app_launcher *launcher, const reach_app_launch_request *request)
{
    (void)launcher;
    if (request == nullptr || request->path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    if (reach_is_default_explorer_launch_request(request)) {
        HWND existing = reach_find_usable_explorer_folder_window();
        if (existing != nullptr) {
            return reach_activate_explorer_folder_window(existing);
        }
        return reach_launch_default_explorer_folder();
    }

    uint16_t working_directory[260] = {};
    int32_t has_working_directory = reach_parent_directory(
        request->path,
        working_directory,
        260);

    SHELLEXECUTEINFOW execute = {};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_NOCLOSEPROCESS;
    execute.lpVerb = request->run_as_admin ? L"runas" : L"open";
    execute.lpFile = reinterpret_cast<LPCWSTR>(request->path);
    execute.lpParameters = request->arguments[0] != 0 ? reinterpret_cast<LPCWSTR>(request->arguments) : nullptr;
    execute.lpDirectory = has_working_directory
        ? reinterpret_cast<LPCWSTR>(working_directory)
        : nullptr;
    execute.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&execute)) {
        return REACH_ERROR;
    }

    reach_activate_launched_process_window_async(execute.hProcess, request->path);
    return REACH_OK;
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
