#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>

#include <cwchar>
#include <cstdint>

static const wchar_t *REACH_PROBE_OWNER_CLASS = L"ReachTrayProbeOwner";
static const UINT REACH_PROBE_CALLBACK = WM_APP + 77;
static const UINT REACH_PROBE_UID = 0x2345;

static const GUID REACH_PROBE_GUID = {
    0x15fd0d3a, 0x7275, 0x4d86, {0x96, 0x40, 0xb4, 0x40, 0x82, 0x35, 0xa1, 0x91}};

static void reach_probe_print(const wchar_t *message)
{
    if (message == nullptr)
    {
        return;
    }

    DWORD written = 0;
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!WriteConsoleW(output, message, (DWORD)wcslen(message), &written, nullptr))
    {
        char utf8[2048] = {};
        int bytes = WideCharToMultiByte(CP_UTF8, 0, message, -1, utf8, (int)sizeof(utf8) - 3,
                                        nullptr, nullptr);
        if (bytes > 0)
        {
            utf8[bytes - 1] = '\r';
            utf8[bytes] = '\n';
            WriteFile(output, utf8, (DWORD)(bytes + 1), &written, nullptr);
        }
        return;
    }

    WriteConsoleW(output, L"\r\n", 2, &written, nullptr);
}

static void reach_probe_printf(const wchar_t *format, uintptr_t a = 0, uintptr_t b = 0,
                               uintptr_t c = 0, uintptr_t d = 0, uintptr_t e = 0)
{
    wchar_t buffer[1024] = {};
    swprintf_s(buffer, format, a, b, c, d, e);
    reach_probe_print(buffer);
}

static const wchar_t *reach_probe_message_name(UINT message)
{
    switch (message)
    {
    case WM_LBUTTONDOWN:
        return L"WM_LBUTTONDOWN";
    case WM_LBUTTONUP:
        return L"WM_LBUTTONUP";
    case WM_LBUTTONDBLCLK:
        return L"WM_LBUTTONDBLCLK";
    case WM_RBUTTONDOWN:
        return L"WM_RBUTTONDOWN";
    case WM_RBUTTONUP:
        return L"WM_RBUTTONUP";
    case WM_CONTEXTMENU:
        return L"WM_CONTEXTMENU";
    case NIN_SELECT:
        return L"NIN_SELECT";
    case NIN_KEYSELECT:
        return L"NIN_KEYSELECT";
    default:
        return L"UNKNOWN";
    }
}

static void reach_probe_print_legacy_callback(WPARAM wparam, LPARAM lparam)
{
    reach_probe_printf(L"legacy callback uid=0x%llX event=%s raw_wParam=0x%llX raw_lParam=0x%llX",
                       static_cast<uintptr_t>(wparam),
                       reinterpret_cast<uintptr_t>(reach_probe_message_name((UINT)lparam)),
                       static_cast<uintptr_t>(wparam), static_cast<uintptr_t>(lparam));
}

static void reach_probe_print_v4_callback(WPARAM wparam, LPARAM lparam)
{
    int x = GET_X_LPARAM(wparam);
    int y = GET_Y_LPARAM(wparam);
    UINT event = LOWORD(lparam);
    UINT uid = HIWORD(lparam);

    reach_probe_printf(L"v4 callback event=%s uid=0x%llX x=%lld y=%lld raw_lParam=0x%llX",
                       reinterpret_cast<uintptr_t>(reach_probe_message_name(event)), uid, x, y,
                       static_cast<uintptr_t>(lparam));
}

static LRESULT CALLBACK reach_probe_owner_proc(HWND hwnd, UINT message, WPARAM wparam,
                                               LPARAM lparam)
{
    if (message == REACH_PROBE_CALLBACK)
    {

        UINT v4_uid = HIWORD(lparam);
        UINT v4_event = LOWORD(lparam);

        if (v4_uid == REACH_PROBE_UID &&
            (v4_event == WM_LBUTTONDOWN || v4_event == WM_LBUTTONUP || v4_event == WM_RBUTTONDOWN ||
             v4_event == WM_RBUTTONUP || v4_event == WM_CONTEXTMENU || v4_event == NIN_SELECT ||
             v4_event == NIN_KEYSELECT))
        {
            reach_probe_print_v4_callback(wparam, lparam);
        }
        else
        {
            reach_probe_print_legacy_callback(wparam, lparam);
        }

        return 0;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static HWND reach_probe_create_owner_window(void)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = reach_probe_owner_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = REACH_PROBE_OWNER_CLASS;

    ATOM atom = RegisterClassExW(&wc);
    if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return nullptr;
    }

    return CreateWindowExW(0, REACH_PROBE_OWNER_CLASS, L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr,
                           GetModuleHandleW(nullptr), nullptr);
}

static int reach_probe_has_arg(int argc, wchar_t **argv, const wchar_t *needle)
{
    for (int index = 1; index < argc; ++index)
    {
        if (_wcsicmp(argv[index], needle) == 0)
        {
            return 1;
        }
    }

    return 0;
}

static void reach_probe_fill_notify(NOTIFYICONDATAW *data, HWND owner, int use_guid)
{
    *data = {};
    data->cbSize = sizeof(*data);
    data->hWnd = owner;
    data->uID = REACH_PROBE_UID;
    data->uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data->uCallbackMessage = REACH_PROBE_CALLBACK;
    data->hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(data->szTip, L"ReachProbeAdd");

    if (use_guid)
    {
        data->uFlags |= NIF_GUID;
        data->guidItem = REACH_PROBE_GUID;
    }
}

static void reach_probe_shell_notify(const wchar_t *label, DWORD message, NOTIFYICONDATAW *data)
{
    SetLastError(0);
    BOOL ok = Shell_NotifyIconW(message, data);
    reach_probe_printf(L"%s result=%llu last_error=%llu", reinterpret_cast<uintptr_t>(label),
                       ok ? 1 : 0, GetLastError());
}

static void reach_probe_pump_for_seconds(double seconds)
{
    DWORD end = GetTickCount() + (DWORD)(seconds * 1000.0);

    while (GetTickCount() < end)
    {
        MSG msg = {};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        Sleep(10);
    }
}

int wmain(int argc, wchar_t **argv)
{
    int use_guid =
        reach_probe_has_arg(argc, argv, L"guid") || reach_probe_has_arg(argc, argv, L"guid-v4");

    int use_v4 =
        reach_probe_has_arg(argc, argv, L"v4") || reach_probe_has_arg(argc, argv, L"guid-v4");

    int test_hidden = reach_probe_has_arg(argc, argv, L"hidden");

    HWND shell_tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    reach_probe_printf(L"FindWindow(Shell_TrayWnd)=0x%llX",
                       reinterpret_cast<uintptr_t>(shell_tray));

    if (shell_tray == nullptr)
    {
        reach_probe_print(L"No Shell_TrayWnd found. Start Reach first.");
        return 2;
    }

    HWND owner = reach_probe_create_owner_window();
    if (owner == nullptr)
    {
        reach_probe_print(L"failed to create owner window");
        return 1;
    }

    reach_probe_printf(L"owner=0x%llX mode=%s%s", reinterpret_cast<uintptr_t>(owner),
                       reinterpret_cast<uintptr_t>(use_guid ? L"guid" : L"hwnd-uid"),
                       reinterpret_cast<uintptr_t>(use_v4 ? L" v4" : L" legacy"));

    NOTIFYICONDATAW data = {};
    reach_probe_fill_notify(&data, owner, use_guid);

    reach_probe_shell_notify(L"NIM_ADD", NIM_ADD, &data);
    if (reach_probe_has_arg(argc, argv, L"duplicate"))
    {
        reach_probe_shell_notify(L"NIM_ADD duplicate", NIM_ADD, &data);
    }
    if (use_v4)
    {
        data.uVersion = NOTIFYICON_VERSION_4;
        reach_probe_shell_notify(L"NIM_SETVERSION", NIM_SETVERSION, &data);
    }

    wcscpy_s(data.szTip, L"ReachProbeModify");
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    if (use_guid)
    {
        data.uFlags |= NIF_GUID;
    }
    reach_probe_shell_notify(L"NIM_MODIFY tip", NIM_MODIFY, &data);

    if (test_hidden)
    {
        data.uFlags = NIF_STATE;
        if (use_guid)
        {
            data.uFlags |= NIF_GUID;
        }
        data.dwState = NIS_HIDDEN;
        data.dwStateMask = NIS_HIDDEN;
        reach_probe_shell_notify(L"NIM_MODIFY hidden", NIM_MODIFY, &data);

        reach_probe_print(L"hidden for 2 seconds");
        reach_probe_pump_for_seconds(2.0);

        data.dwState = 0;
        data.dwStateMask = NIS_HIDDEN;
        reach_probe_shell_notify(L"NIM_MODIFY visible", NIM_MODIFY, &data);
    }

    reach_probe_print(L"Probe icon should now be visible in Reach.");
    reach_probe_print(L"Click it in Reach: left click should print callbacks; right click should "
                      L"print callbacks.");
    reach_probe_print(L"Waiting 30 seconds before NIM_DELETE.");

    reach_probe_pump_for_seconds(10.0);

    data.uFlags = 0;
    if (use_guid)
    {
        data.uFlags |= NIF_GUID;
    }

    reach_probe_shell_notify(L"NIM_DELETE", NIM_DELETE, &data);

    DestroyWindow(owner);
    return 0;
}
