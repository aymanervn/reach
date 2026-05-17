#include "reach/platform/windows_adapters.h"

#include "reach/ports/tray_provider.h"

#include <windows.h>
#include <shellapi.h>

#include <new>

struct reach_tray_provider {
    int32_t native_tray_supported;
};

enum reach_tray_menu_command {
    REACH_TRAY_MENU_OPEN_EXPLORER = 100,
    REACH_TRAY_MENU_OPEN_TASK_MANAGER = 101
};

static void reach_tray_copy_text(uint16_t *dst, size_t dst_count, const wchar_t *src)
{
    if (dst == nullptr || dst_count == 0) {
        return;
    }
    if (src == nullptr) {
        dst[0] = 0;
        return;
    }

    size_t index = 0;
    while (index + 1 < dst_count && src[index] != 0) {
        dst[index] = (uint16_t)src[index];
        ++index;
    }
    dst[index] = 0;
}

static reach_result reach_tray_open_explorer(void)
{
    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.lpFile = L"explorer.exe";
    info.lpParameters = L"shell:MyComputerFolder";
    info.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&info) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_tray_open_task_manager(void)
{
    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.lpFile = L"taskmgr.exe";
    info.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&info) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_tray_refresh(reach_tray_provider *provider)
{
    REACH_ASSERT(provider != nullptr);
    if (provider == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    provider->native_tray_supported = 0;
    return REACH_NOT_IMPLEMENTED;
}

static size_t reach_tray_item_count(const reach_tray_provider *provider)
{
    REACH_ASSERT(provider != nullptr);
    (void)provider;
    return 2;
}

static reach_result reach_tray_item_at(const reach_tray_provider *provider, size_t index, reach_tray_item *out_item)
{
    REACH_ASSERT(provider != nullptr);
    REACH_ASSERT(out_item != nullptr);
    if (provider == nullptr || out_item == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_item = {};
    if (index == 0) {
        out_item->id = REACH_TRAY_MENU_OPEN_EXPLORER;
        reach_tray_copy_text(out_item->title, 128, L"Open Explorer");
        return REACH_OK;
    }
    if (index == 1) {
        out_item->id = REACH_TRAY_MENU_OPEN_TASK_MANAGER;
        reach_tray_copy_text(out_item->title, 128, L"Open Task Manager");
        return REACH_OK;
    }
    return REACH_INVALID_ARGUMENT;
}

static reach_result reach_tray_open_menu(reach_tray_provider *provider, uint32_t item_id)
{
    REACH_ASSERT(provider != nullptr);
    if (provider == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    if (item_id == REACH_TRAY_MENU_OPEN_EXPLORER) {
        return reach_tray_open_explorer();
    }
    if (item_id == REACH_TRAY_MENU_OPEN_TASK_MANAGER) {
        return reach_tray_open_task_manager();
    }

    POINT point = {};
    GetCursorPos(&point);
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return REACH_ERROR;
    }

    AppendMenuW(menu, MF_STRING | MF_GRAYED, 1, L"Reach fallback tray");
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 2, L"Native tray icons are not owned yet");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, REACH_TRAY_MENU_OPEN_EXPLORER, L"Open Explorer");
    AppendMenuW(menu, MF_STRING, REACH_TRAY_MENU_OPEN_TASK_MANAGER, L"Open Task Manager");

    HWND owner = GetForegroundWindow();
    if (owner == nullptr) {
        owner = GetDesktopWindow();
    }
    SetForegroundWindow(owner);
    int command = TrackPopupMenu(menu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RETURNCMD, point.x, point.y, 0, owner, nullptr);
    DestroyMenu(menu);

    if (command == REACH_TRAY_MENU_OPEN_EXPLORER) {
        return reach_tray_open_explorer();
    }
    if (command == REACH_TRAY_MENU_OPEN_TASK_MANAGER) {
        return reach_tray_open_task_manager();
    }

    return REACH_OK;
}

static void reach_tray_destroy(reach_tray_provider *provider)
{
    delete provider;
}

reach_result reach_windows_create_tray_provider(reach_tray_provider_port *out_port)
{
    REACH_ASSERT(out_port != nullptr);
    if (out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_tray_provider *provider = new (std::nothrow) reach_tray_provider();
    if (provider == nullptr) {
        return REACH_ERROR;
    }
    provider->native_tray_supported = 0;

    out_port->provider = provider;
    out_port->ops.refresh = reach_tray_refresh;
    out_port->ops.item_count = reach_tray_item_count;
    out_port->ops.item_at = reach_tray_item_at;
    out_port->ops.open_menu = reach_tray_open_menu;
    out_port->ops.destroy = reach_tray_destroy;
    return REACH_OK;
}
