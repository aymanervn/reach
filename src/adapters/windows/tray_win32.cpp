#include "reach/platform/windows_adapters.h"

#include "reach/ports/tray_provider.h"

#include <windows.h>
#include <shellapi.h>

#include <new>

static const wchar_t *REACH_TRAY_HOST_CLASS = L"Shell_TrayWnd";
static const uint32_t REACH_TRAY_WIRE_SIGNATURE = 0x34753423;
static const size_t REACH_TRAY_WIRE_MIN_SIZE = 288;
static const size_t REACH_TRAY_WIRE_VERSION_OFFSET = 808;

struct reach_tray_native_item {
    reach_tray_item item;
    HWND owner;
    uint32_t owner_id;
    uint32_t callback_message;
    uint32_t version;
};

struct reach_tray_provider {
    HWND host_window;
    ATOM host_class;
    UINT taskbar_created_message;
    reach_tray_native_item items[REACH_MAX_TRAY_ITEMS];
    size_t item_count;
    int32_t dirty;
};

static uint32_t reach_tray_read_u32(const BYTE *bytes, size_t count, size_t offset)
{
    if (bytes == nullptr || offset + sizeof(uint32_t) > count) {
        return 0;
    }

    uint32_t value = 0;
    memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

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

static void reach_tray_release_item_icon(reach_tray_native_item *item)
{
    if (item != nullptr && item->item.icon_id != 0) {
        DestroyIcon(reinterpret_cast<HICON>(item->item.icon_id));
        item->item.icon_id = 0;
    }
}

static size_t reach_tray_find_item(reach_tray_provider *provider, HWND owner, uint32_t owner_id)
{
    if (provider == nullptr || owner == nullptr) {
        return REACH_MAX_TRAY_ITEMS;
    }

    for (size_t index = 0; index < provider->item_count; ++index) {
        if (provider->items[index].owner == owner && provider->items[index].owner_id == owner_id) {
            return index;
        }
    }
    return REACH_MAX_TRAY_ITEMS;
}

static void reach_tray_remove_item_at(reach_tray_provider *provider, size_t index)
{
    if (provider == nullptr || index >= provider->item_count) {
        return;
    }

    reach_tray_release_item_icon(&provider->items[index]);
    for (size_t next = index + 1; next < provider->item_count; ++next) {
        provider->items[next - 1] = provider->items[next];
    }
    provider->items[provider->item_count - 1] = {};
    --provider->item_count;
}

static void reach_tray_apply_payload(reach_tray_native_item *item, const BYTE *bytes, size_t count, uint32_t message)
{
    if (item == nullptr || bytes == nullptr || count < REACH_TRAY_WIRE_MIN_SIZE) {
        return;
    }

    uint32_t flags = reach_tray_read_u32(bytes, count, 20);
    item->callback_message = reach_tray_read_u32(bytes, count, 24);
    item->item.id = item->owner_id;

    if ((flags & NIF_TIP) != 0) {
        const wchar_t *tip = reinterpret_cast<const wchar_t *>(bytes + 32);
        reach_tray_copy_text(item->item.title, 128, tip);
    }

    if ((flags & NIF_ICON) != 0) {
        HICON incoming = reinterpret_cast<HICON>((uintptr_t)reach_tray_read_u32(bytes, count, 28));
        HICON copy = incoming != nullptr ? CopyIcon(incoming) : nullptr;
        if (copy != nullptr || message == NIM_MODIFY) {
            reach_tray_release_item_icon(item);
            item->item.icon_id = reinterpret_cast<uint64_t>(copy);
        }
    }
}

static reach_result reach_tray_handle_copydata(reach_tray_provider *provider, const COPYDATASTRUCT *copy)
{
    if (provider == nullptr || copy == nullptr || copy->lpData == nullptr || copy->cbData < REACH_TRAY_WIRE_MIN_SIZE) {
        return REACH_INVALID_ARGUMENT;
    }

    const BYTE *bytes = reinterpret_cast<const BYTE *>(copy->lpData);
    size_t count = copy->cbData;
    if (reach_tray_read_u32(bytes, count, 0) != REACH_TRAY_WIRE_SIGNATURE) {
        return REACH_ERROR;
    }

    uint32_t message = reach_tray_read_u32(bytes, count, 4);
    HWND owner = reinterpret_cast<HWND>((uintptr_t)reach_tray_read_u32(bytes, count, 12));
    uint32_t owner_id = reach_tray_read_u32(bytes, count, 16);
    size_t index = reach_tray_find_item(provider, owner, owner_id);

    if (message == NIM_DELETE) {
        if (index != REACH_MAX_TRAY_ITEMS) {
            reach_tray_remove_item_at(provider, index);
            provider->dirty = 1;
        }
        return REACH_OK;
    }

    if (message == NIM_SETVERSION) {
        if (index != REACH_MAX_TRAY_ITEMS) {
            provider->items[index].version = reach_tray_read_u32(bytes, count, REACH_TRAY_WIRE_VERSION_OFFSET);
        }
        return REACH_OK;
    }

    if (message != NIM_ADD && message != NIM_MODIFY) {
        return REACH_OK;
    }

    if (index == REACH_MAX_TRAY_ITEMS) {
        if (message == NIM_MODIFY || provider->item_count >= REACH_MAX_TRAY_ITEMS) {
            return REACH_ERROR;
        }
        index = provider->item_count++;
        provider->items[index] = {};
        provider->items[index].owner = owner;
        provider->items[index].owner_id = owner_id;
    }

    reach_tray_apply_payload(&provider->items[index], bytes, count, message);
    provider->dirty = 1;
    return REACH_OK;
}

static LRESULT CALLBACK reach_tray_host_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    reach_tray_provider *provider = reinterpret_cast<reach_tray_provider *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return TRUE;
    }

    if (message == WM_COPYDATA) {
        reach_result result = reach_tray_handle_copydata(provider, reinterpret_cast<const COPYDATASTRUCT *>(lparam));
        return result == REACH_OK ? TRUE : FALSE;
    }

    (void)wparam;
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static reach_result reach_tray_create_host_window(reach_tray_provider *provider)
{
    REACH_ASSERT(provider != nullptr);
    if (provider == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = reach_tray_host_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = REACH_TRAY_HOST_CLASS;
    provider->host_class = RegisterClassExW(&wc);
    if (provider->host_class == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return REACH_ERROR;
    }

    provider->host_window = CreateWindowExW(
        0,
        REACH_TRAY_HOST_CLASS,
        L"",
        WS_POPUP,
        0,
        0,
        1,
        1,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        provider);
    return provider->host_window != nullptr ? REACH_OK : REACH_ERROR;
}

static void reach_tray_broadcast_taskbar_created(reach_tray_provider *provider)
{
    if (provider == nullptr) {
        return;
    }
    provider->taskbar_created_message = RegisterWindowMessageW(L"TaskbarCreated");
    if (provider->taskbar_created_message != 0) {
        SendMessageTimeoutW(HWND_BROADCAST, provider->taskbar_created_message, 0, 0, SMTO_ABORTIFHUNG, 2000, nullptr);
    }
}

static reach_result reach_tray_refresh(reach_tray_provider *provider)
{
    REACH_ASSERT(provider != nullptr);
    if (provider == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    size_t index = 0;
    while (index < provider->item_count) {
        if (provider->items[index].owner == nullptr || !IsWindow(provider->items[index].owner)) {
            reach_tray_remove_item_at(provider, index);
        } else {
            ++index;
        }
    }
    provider->dirty = 0;
    return REACH_OK;
}

static int32_t reach_tray_needs_refresh(const reach_tray_provider *provider)
{
    return provider != nullptr && provider->dirty;
}

static size_t reach_tray_item_count(const reach_tray_provider *provider)
{
    REACH_ASSERT(provider != nullptr);
    return provider != nullptr ? provider->item_count : 0;
}

static reach_result reach_tray_item_at(const reach_tray_provider *provider, size_t index, reach_tray_item *out_item)
{
    REACH_ASSERT(provider != nullptr);
    REACH_ASSERT(out_item != nullptr);
    if (provider == nullptr || out_item == nullptr || index >= provider->item_count) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_item = provider->items[index].item;
    return REACH_OK;
}

static LPARAM reach_tray_v4_lparam(uint32_t message, uint32_t owner_id)
{
    return (LPARAM)(((owner_id & 0xffff) << 16) | (message & 0xffff));
}

static WPARAM reach_tray_cursor_wparam(void)
{
    POINT point = {};
    GetCursorPos(&point);
    return (WPARAM)(((point.y & 0xffff) << 16) | (point.x & 0xffff));
}

static reach_result reach_tray_activate(reach_tray_provider *provider, uint32_t item_id, reach_tray_action action)
{
    REACH_ASSERT(provider != nullptr);
    if (provider == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < provider->item_count; ++index) {
        reach_tray_native_item *item = &provider->items[index];
        if (item->item.id != item_id || item->owner == nullptr || item->callback_message == 0) {
            continue;
        }

        uint32_t mouse_message = action == REACH_TRAY_ACTION_RIGHT_CLICK ? WM_RBUTTONUP : WM_LBUTTONUP;
        WPARAM wparam = item->owner_id;
        LPARAM lparam = mouse_message;
        if (item->version >= NOTIFYICON_VERSION_4) {
            wparam = reach_tray_cursor_wparam();
            lparam = reach_tray_v4_lparam(mouse_message, item->owner_id);
        }
        if (action == REACH_TRAY_ACTION_LEFT_CLICK) {
            WPARAM down_wparam = item->version >= NOTIFYICON_VERSION_4 ? reach_tray_cursor_wparam() : item->owner_id;
            LPARAM down_lparam = item->version >= NOTIFYICON_VERSION_4
                ? reach_tray_v4_lparam(WM_LBUTTONDOWN, item->owner_id)
                : WM_LBUTTONDOWN;
            if (!PostMessageW(item->owner, item->callback_message, down_wparam, down_lparam)) {
                return REACH_ERROR;
            }
        }
        return PostMessageW(item->owner, item->callback_message, wparam, lparam) ? REACH_OK : REACH_ERROR;
    }

    return REACH_INVALID_ARGUMENT;
}

static void reach_tray_destroy(reach_tray_provider *provider)
{
    if (provider != nullptr) {
        for (size_t index = 0; index < provider->item_count; ++index) {
            reach_tray_release_item_icon(&provider->items[index]);
        }
        if (provider->host_window != nullptr) {
            DestroyWindow(provider->host_window);
            provider->host_window = nullptr;
        }
    }
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

    reach_result result = reach_tray_create_host_window(provider);
    if (result != REACH_OK) {
        delete provider;
        return result;
    }
    reach_tray_broadcast_taskbar_created(provider);

    out_port->provider = provider;
    out_port->ops.refresh = reach_tray_refresh;
    out_port->ops.needs_refresh = reach_tray_needs_refresh;
    out_port->ops.item_count = reach_tray_item_count;
    out_port->ops.item_at = reach_tray_item_at;
    out_port->ops.activate = reach_tray_activate;
    out_port->ops.destroy = reach_tray_destroy;
    return REACH_OK;
}
