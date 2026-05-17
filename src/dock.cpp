#include "reach/dock.h"

#include <windows.h>
#include <dcomp.h>
#include <shellapi.h>

#include <new>

struct reach_dock {
    HWND hwnd;
    reach_dock_config config;
    int32_t hidden;
};

static const wchar_t *reach_dock_class_name()
{
    return L"ReachDockWindow";
}

static LRESULT CALLBACK reach_dock_wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_NCHITTEST:
        return HTCLIENT;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint = {};
        HDC dc = BeginPaint(hwnd, &paint);
        RECT rect = {};
        GetClientRect(hwnd, &rect);
        HBRUSH brush = CreateSolidBrush(RGB(24, 24, 28));
        FillRect(dc, &rect, brush);
        DeleteObject(brush);
        EndPaint(hwnd, &paint);
        return 0;
    }
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

static reach_result reach_register_dock_class()
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = reach_dock_wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = reach_dock_class_name();

    ATOM atom = RegisterClassExW(&wc);
    if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return REACH_ERROR;
    }

    return REACH_OK;
}

static RECT reach_primary_work_area()
{
    RECT result = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &result, 0);
    return result;
}

static void reach_dock_apply_bounds(reach_dock *dock)
{
    RECT work = reach_primary_work_area();
    int height = dock->config.height_px > 0 ? dock->config.height_px : 56;
    int width = work.right - work.left;
    int x = work.left;
    int y = dock->hidden ? work.bottom - 2 : work.bottom - height;

    SetWindowPos(dock->hwnd, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

reach_result reach_dock_create(const reach_dock_desc *desc, reach_dock **out_dock)
{
    if (out_dock == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result class_result = reach_register_dock_class();
    if (class_result != REACH_OK) {
        *out_dock = nullptr;
        return class_result;
    }

    reach_dock *dock = new (std::nothrow) reach_dock();
    if (dock == nullptr) {
        *out_dock = nullptr;
        return REACH_ERROR;
    }

    dock->config.height_px = 56;
    dock->config.icon_size_px = 32;
    dock->config.auto_hide = 1;
    dock->config.animation_seconds = 0.16;
    if (desc != nullptr && desc->config != nullptr) {
        dock->config = *desc->config;
    }

    DWORD ex_style = WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE;
    dock->hwnd = CreateWindowExW(
        ex_style,
        reach_dock_class_name(),
        L"Reach Dock",
        WS_POPUP,
        0,
        0,
        1,
        1,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    if (dock->hwnd == nullptr) {
        delete dock;
        *out_dock = nullptr;
        return REACH_ERROR;
    }

    SetLayeredWindowAttributes(dock->hwnd, 0, 242, LWA_ALPHA);
    reach_dock_apply_bounds(dock);
    *out_dock = dock;
    return REACH_OK;
}

void reach_dock_destroy(reach_dock *dock)
{
    if (dock == nullptr) {
        return;
    }

    if (dock->hwnd != nullptr) {
        DestroyWindow(dock->hwnd);
    }
    delete dock;
}

reach_result reach_dock_show(reach_dock *dock)
{
    if (dock == nullptr || dock->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    dock->hidden = 0;
    SetLayeredWindowAttributes(dock->hwnd, 0, 242, LWA_ALPHA);
    reach_dock_apply_bounds(dock);
    return REACH_OK;
}

reach_result reach_dock_hide(reach_dock *dock)
{
    if (dock == nullptr || dock->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    dock->hidden = 1;
    SetLayeredWindowAttributes(dock->hwnd, 0, 0, LWA_ALPHA);
    reach_dock_apply_bounds(dock);
    return REACH_OK;
}

reach_result reach_dock_update(reach_dock *dock, double delta_seconds)
{
    (void)delta_seconds;
    if (dock == nullptr || dock->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    return REACH_OK;
}

reach_result reach_dock_set_auto_hidden(reach_dock *dock, int32_t hidden)
{
    if (dock == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    return hidden ? reach_dock_hide(dock) : reach_dock_show(dock);
}

reach_result reach_dock_show_tray_menu(reach_dock *dock, int32_t x, int32_t y)
{
    if (dock == nullptr || dock->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return REACH_ERROR;
    }

    AppendMenuW(menu, MF_STRING | MF_GRAYED, 1, L"System tray");
    SetForegroundWindow(dock->hwnd);
    TrackPopupMenu(menu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_NONOTIFY, x, y, 0, dock->hwnd, nullptr);
    DestroyMenu(menu);
    return REACH_OK;
}
