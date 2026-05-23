#include <windows.h>
#include <shellapi.h>

#include <cwchar>
#include <cstdint>

static const wchar_t *REACH_PROBE_TRAY_CLASS = L"Shell_TrayWnd";
static const wchar_t *REACH_PROBE_OWNER_CLASS = L"ReachTrayProbeOwner";

static void reach_probe_print(const wchar_t *message)
{
    if (message == nullptr) {
        return;
    }

    DWORD written = 0;
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!WriteConsoleW(output, message, (DWORD)wcslen(message), &written, nullptr)) {
        char utf8[2048] = {};
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

static void reach_probe_printf(const wchar_t *format, uintptr_t a = 0, uintptr_t b = 0, uintptr_t c = 0, uintptr_t d = 0)
{
    wchar_t buffer[1024] = {};
    swprintf_s(buffer, format, a, b, c, d);
    reach_probe_print(buffer);
}

static void reach_probe_dump_bytes(const BYTE *bytes, DWORD count)
{
    if (bytes == nullptr || count == 0) {
        return;
    }

    DWORD limit = count < 256 ? count : 256;
    for (DWORD offset = 0; offset < limit; offset += 16) {
        wchar_t line[256] = {};
        wchar_t *cursor = line;
        size_t remaining = sizeof(line) / sizeof(line[0]);
        int written = swprintf_s(cursor, remaining, L"%04X:", offset);
        cursor += written;
        remaining -= (size_t)written;

        for (DWORD index = 0; index < 16 && offset + index < limit; ++index) {
            written = swprintf_s(cursor, remaining, L" %02X", bytes[offset + index]);
            cursor += written;
            remaining -= (size_t)written;
        }
        reach_probe_print(line);
    }
}

static void reach_probe_scan_payload(const BYTE *bytes, DWORD count)
{
    if (bytes == nullptr || count < sizeof(DWORD)) {
        return;
    }

    DWORD notify_size = sizeof(NOTIFYICONDATAW);
    for (DWORD offset = 0; offset + sizeof(DWORD) <= count; offset += 4) {
        DWORD value = *reinterpret_cast<const DWORD *>(bytes + offset);
        if (value == notify_size ||
            value == NIM_ADD ||
            value == NIM_MODIFY ||
            value == NIM_DELETE ||
            value == NIM_SETVERSION ||
            value == 0x2345 ||
            value == WM_APP + 77) {
            reach_probe_printf(L"candidate dword offset=%llu value=0x%llX", offset, value);
        }
    }

    const wchar_t needle[] = L"ReachProbe";
    size_t needle_bytes = (wcslen(needle) + 1) * sizeof(wchar_t);
    for (DWORD offset = 0; offset + needle_bytes <= count; offset += 2) {
        if (memcmp(bytes + offset, needle, needle_bytes - sizeof(wchar_t)) == 0) {
            reach_probe_printf(L"candidate UTF-16 marker offset=%llu", offset);
        }
    }
}

static LRESULT CALLBACK reach_probe_tray_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_COPYDATA) {
        COPYDATASTRUCT *copy = reinterpret_cast<COPYDATASTRUCT *>(lparam);
        if (copy != nullptr) {
            reach_probe_printf(
                L"WM_COPYDATA hwnd=0x%llX from=0x%llX dwData=0x%llX cbData=%llu",
                reinterpret_cast<uintptr_t>(hwnd),
                static_cast<uintptr_t>(wparam),
                static_cast<uintptr_t>(copy->dwData),
                copy->cbData);
            reach_probe_scan_payload(reinterpret_cast<const BYTE *>(copy->lpData), copy->cbData);
            reach_probe_dump_bytes(reinterpret_cast<const BYTE *>(copy->lpData), copy->cbData);
        }
        return TRUE;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static LRESULT CALLBACK reach_probe_owner_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_APP + 77) {
        reach_probe_printf(
            L"callback hwnd=0x%llX wParam=0x%llX lParam=0x%llX",
            reinterpret_cast<uintptr_t>(hwnd),
            static_cast<uintptr_t>(wparam),
            static_cast<uintptr_t>(lparam));
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static HWND reach_probe_create_window(const wchar_t *class_name, WNDPROC proc)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = class_name;
    ATOM atom = RegisterClassExW(&wc);
    if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return nullptr;
    }

    return CreateWindowExW(
        0,
        class_name,
        L"",
        WS_POPUP,
        0,
        0,
        1,
        1,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
}

static void reach_probe_send_notify(HWND owner)
{
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = owner;
    data.uID = 0x2345;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = WM_APP + 77;
    data.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(data.szTip, L"ReachProbeAdd");

    reach_probe_print(L"calling Shell_NotifyIconW(NIM_ADD)");
    BOOL ok = Shell_NotifyIconW(NIM_ADD, &data);
    reach_probe_printf(L"NIM_ADD result=%llu last_error=%llu", ok ? 1 : 0, GetLastError());

    data.uVersion = NOTIFYICON_VERSION_4;
    reach_probe_print(L"calling Shell_NotifyIconW(NIM_SETVERSION)");
    ok = Shell_NotifyIconW(NIM_SETVERSION, &data);
    reach_probe_printf(L"NIM_SETVERSION result=%llu last_error=%llu", ok ? 1 : 0, GetLastError());

    wcscpy_s(data.szTip, L"ReachProbeModify");
    reach_probe_print(L"calling Shell_NotifyIconW(NIM_MODIFY)");
    ok = Shell_NotifyIconW(NIM_MODIFY, &data);
    reach_probe_printf(L"NIM_MODIFY result=%llu last_error=%llu", ok ? 1 : 0, GetLastError());

    reach_probe_print(L"calling Shell_NotifyIconW(NIM_DELETE)");
    ok = Shell_NotifyIconW(NIM_DELETE, &data);
    reach_probe_printf(L"NIM_DELETE result=%llu last_error=%llu", ok ? 1 : 0, GetLastError());
}

int wmain()
{
    HWND tray = reach_probe_create_window(REACH_PROBE_TRAY_CLASS, reach_probe_tray_proc);
    HWND owner = reach_probe_create_window(REACH_PROBE_OWNER_CLASS, reach_probe_owner_proc);
    if (tray == nullptr || owner == nullptr) {
        reach_probe_print(L"failed to create probe windows");
        return 1;
    }

    HWND shell_tray = FindWindowW(REACH_PROBE_TRAY_CLASS, nullptr);
    reach_probe_printf(
        L"probe_tray=0x%llX find_shell_tray=0x%llX owner=0x%llX",
        reinterpret_cast<uintptr_t>(tray),
        reinterpret_cast<uintptr_t>(shell_tray),
        reinterpret_cast<uintptr_t>(owner));
    if (shell_tray != tray) {
        reach_probe_print(L"FindWindow(Shell_TrayWnd) does not resolve to the probe. Stop Explorer/other shell before probing.");
        return 2;
    }

    reach_probe_send_notify(owner);
    DestroyWindow(owner);
    DestroyWindow(tray);
    return 0;
}
