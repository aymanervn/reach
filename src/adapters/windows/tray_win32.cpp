#include "windows_adapters_internal.h"

#include "reach/ports/tray_provider.h"
#include "windows_icon_handle_internal.h"

#include <windows.h>
#include <shellapi.h>
#include <new>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

static const wchar_t *REACH_TRAY_HOST_CLASS = L"Shell_TrayWnd";

static const uint32_t REACH_TRAY_WIRE_SIGNATURE = 0x34753423;
static const size_t REACH_TRAY_WIRE_BASE_MIN_SIZE = 0x128;

static const size_t REACH_TRAY_WIRE_MAGIC_OFFSET = 0x00;
static const size_t REACH_TRAY_WIRE_MESSAGE_OFFSET = 0x04;
static const size_t REACH_TRAY_WIRE_CBSIZE_OFFSET = 0x08;
static const size_t REACH_TRAY_WIRE_HWND_OFFSET = 0x0C;
static const size_t REACH_TRAY_WIRE_UID_OFFSET = 0x10;
static const size_t REACH_TRAY_WIRE_FLAGS_OFFSET = 0x14;
static const size_t REACH_TRAY_WIRE_CALLBACK_OFFSET = 0x18;
static const size_t REACH_TRAY_WIRE_ICON_OFFSET = 0x1C;
static const size_t REACH_TRAY_WIRE_TIP_OFFSET = 0x20;
static const size_t REACH_TRAY_WIRE_STATE_OFFSET = 0x120;
static const size_t REACH_TRAY_WIRE_STATE_MASK_OFFSET = 0x124;
static const size_t REACH_TRAY_WIRE_VERSION_OFFSET = 0x328;
static const size_t REACH_TRAY_WIRE_INFO_FLAGS_OFFSET = 0x3AC;
static const size_t REACH_TRAY_WIRE_GUID_OFFSET = 0x3B0;
static const size_t REACH_TRAY_WIRE_EXE_PATH_OFFSET = 0x3C4;
static const UINT_PTR REACH_TRAY_TASKBAR_CREATED_REBROADCAST_TIMER = 1001;
static const size_t REACH_TRAY_TOMBSTONE_COUNT = REACH_MAX_TRAY_ITEMS;
static const DWORD REACH_TRAY_TOMBSTONE_TTL_MS = 30000;

struct reach_tray_native_item
{
    reach_tray_item item;
    HWND owner;
    uint32_t owner_id;
    GUID guid;
    int32_t has_guid;
    uint32_t flags;
    uint32_t callback_message;
    uint32_t raw_icon_handle;
    uint32_t state;
    uint32_t version;
};

struct reach_tray_identity
{
    HWND owner;
    uint32_t owner_id;
    GUID guid;
    int32_t has_guid;
};

struct reach_tray_tombstone
{
    reach_tray_identity identity;
    size_t index;
    uint32_t public_id;
    DWORD tick;
    int32_t used;
};

struct reach_tray_provider
{
    HWND host_window;
    ATOM host_class;
    UINT taskbar_created_message;
    reach_tray_native_item items[REACH_MAX_TRAY_ITEMS];
    size_t item_count;
    uint32_t next_item_id;
    int32_t dirty;
    reach_tray_tombstone tombstones[REACH_TRAY_TOMBSTONE_COUNT];
};

static uint32_t reach_tray_read_u32(const BYTE *bytes, size_t count, size_t offset)
{
    if (bytes == nullptr || offset + sizeof(uint32_t) > count)
    {
        return 0;
    }

    uint32_t value = 0;
    memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

static int32_t reach_tray_read_guid(const BYTE *bytes, size_t count, size_t offset, GUID *out_guid)
{
    if (bytes == nullptr || out_guid == nullptr || offset + sizeof(GUID) > count)
    {
        return 0;
    }

    memcpy(out_guid, bytes + offset, sizeof(GUID));
    return 1;
}

static int32_t reach_tray_guid_is_null(const GUID *guid)
{
    static const GUID null_guid = {};
    return guid == nullptr || memcmp(guid, &null_guid, sizeof(GUID)) == 0;
}

static int32_t reach_tray_guid_equal(const GUID *a, const GUID *b)
{
    return a != nullptr && b != nullptr && memcmp(a, b, sizeof(GUID)) == 0;
}

static reach_tray_identity reach_tray_make_identity(HWND owner, uint32_t owner_id, const GUID *guid,
                                                    int32_t has_guid)
{
    reach_tray_identity identity = {};
    identity.owner = owner;
    identity.owner_id = owner_id;
    identity.has_guid = has_guid;

    if (has_guid && guid != nullptr)
    {
        identity.guid = *guid;
    }

    return identity;
}

static int32_t reach_tray_identity_equal(const reach_tray_identity *a, const reach_tray_identity *b)
{
    if (a == nullptr || b == nullptr)
    {
        return 0;
    }

    if (a->has_guid || b->has_guid)
    {
        return a->has_guid && b->has_guid && reach_tray_guid_equal(&a->guid, &b->guid);
    }

    return a->owner == b->owner && a->owner_id == b->owner_id;
}

static reach_tray_identity reach_tray_item_identity(const reach_tray_native_item *item)
{
    reach_tray_identity identity = {};
    if (item == nullptr)
    {
        return identity;
    }

    identity.owner = item->owner;
    identity.owner_id = item->owner_id;
    identity.has_guid = item->has_guid;
    identity.guid = item->guid;
    return identity;
}

static void reach_tray_copy_wide_to_u16(uint16_t *dst, size_t dst_count, const wchar_t *src,
                                        size_t src_max_count)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }

    dst[0] = 0;
    if (src == nullptr || src_max_count == 0)
    {
        return;
    }

    size_t index = 0;
    while (index + 1 < dst_count && index < src_max_count && src[index] != 0)
    {
        dst[index] = (uint16_t)src[index];
        ++index;
    }

    dst[index] = 0;
}

static void reach_tray_copy_payload_wstring(uint16_t *dst, size_t dst_count, const BYTE *bytes,
                                            size_t count, size_t offset, size_t wchar_count)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }

    dst[0] = 0;
    if (bytes == nullptr || offset >= count)
    {
        return;
    }

    size_t available_bytes = count - offset;
    size_t available_wchars = available_bytes / sizeof(wchar_t);
    if (available_wchars > wchar_count)
    {
        available_wchars = wchar_count;
    }

    const wchar_t *src = reinterpret_cast<const wchar_t *>(bytes + offset);
    reach_tray_copy_wide_to_u16(dst, dst_count, src, available_wchars);
}

static void reach_tray_release_item_icon(reach_tray_native_item *item)
{
    if (item != nullptr && item->item.icon_id != 0)
    {
        reach_windows_icon_id_release(item->item.icon_id);
        item->item.icon_id = 0;
    }
}

static void reach_tray_update_item_icon(reach_tray_native_item *item, uint32_t raw_icon)
{
    if (item == nullptr)
    {
        return;
    }

    item->raw_icon_handle = raw_icon;

    HICON incoming = reinterpret_cast<HICON>((ULONG_PTR)raw_icon);
    HICON copied = incoming != nullptr ? CopyIcon(incoming) : nullptr;
    if (copied == nullptr)
    {
        return;
    }

    uint64_t icon_id = reach_windows_icon_id_from_hicon(copied);
    if (icon_id == 0)
    {
        return;
    }

    reach_tray_release_item_icon(item);
    item->item.icon_id = icon_id;
}

static uint32_t reach_tray_allocate_item_id(reach_tray_provider *provider)
{
    if (provider == nullptr)
    {
        return 0;
    }

    ++provider->next_item_id;
    if (provider->next_item_id == 0)
    {
        ++provider->next_item_id;
    }

    return provider->next_item_id;
}

static void reach_tray_initialize_item(reach_tray_provider *provider, reach_tray_native_item *item,
                                       HWND owner, uint32_t owner_id, const GUID *guid,
                                       int32_t has_guid)
{
    if (item == nullptr)
    {
        return;
    }

    *item = {};
    item->item.id = reach_tray_allocate_item_id(provider);
    item->owner = owner;
    item->owner_id = owner_id;
    item->has_guid = has_guid;

    if (has_guid && guid != nullptr)
    {
        item->guid = *guid;
    }
}

static size_t reach_tray_find_item(reach_tray_provider *provider, HWND owner, uint32_t owner_id,
                                   const GUID *guid, int32_t has_guid)
{
    if (provider == nullptr)
    {
        return REACH_MAX_TRAY_ITEMS;
    }

    if (has_guid && !reach_tray_guid_is_null(guid))
    {
        for (size_t index = 0; index < provider->item_count; ++index)
        {
            reach_tray_native_item *item = &provider->items[index];
            if (item->has_guid && reach_tray_guid_equal(&item->guid, guid))
            {
                return index;
            }
        }

        return REACH_MAX_TRAY_ITEMS;
    }

    if (owner == nullptr)
    {
        return REACH_MAX_TRAY_ITEMS;
    }

    for (size_t index = 0; index < provider->item_count; ++index)
    {
        reach_tray_native_item *item = &provider->items[index];
        if (!item->has_guid && item->owner == owner && item->owner_id == owner_id)
        {
            return index;
        }
    }

    return REACH_MAX_TRAY_ITEMS;
}

static void reach_tray_remove_item_at(reach_tray_provider *provider, size_t index)
{
    if (provider == nullptr || index >= provider->item_count)
    {
        return;
    }

    reach_tray_release_item_icon(&provider->items[index]);

    for (size_t next = index + 1; next < provider->item_count; ++next)
    {
        provider->items[next - 1] = provider->items[next];
    }

    provider->items[provider->item_count - 1] = {};
    --provider->item_count;
    provider->dirty = 1;
}

static void reach_tray_prune_tombstones(reach_tray_provider *provider)
{
    if (provider == nullptr)
    {
        return;
    }

    DWORD now = GetTickCount();

    for (size_t index = 0; index < REACH_TRAY_TOMBSTONE_COUNT; ++index)
    {
        reach_tray_tombstone *tombstone = &provider->tombstones[index];
        if (!tombstone->used)
        {
            continue;
        }

        if ((DWORD)(now - tombstone->tick) > REACH_TRAY_TOMBSTONE_TTL_MS)
        {
            *tombstone = {};
        }
    }
}

static void reach_tray_save_tombstone(reach_tray_provider *provider,
                                      const reach_tray_native_item *item, size_t item_index)
{
    if (provider == nullptr || item == nullptr)
    {
        return;
    }

    reach_tray_prune_tombstones(provider);

    reach_tray_identity identity = reach_tray_item_identity(item);

    size_t slot = REACH_TRAY_TOMBSTONE_COUNT;
    for (size_t index = 0; index < REACH_TRAY_TOMBSTONE_COUNT; ++index)
    {
        if (provider->tombstones[index].used &&
            reach_tray_identity_equal(&provider->tombstones[index].identity, &identity))
        {
            slot = index;
            break;
        }

        if (!provider->tombstones[index].used && slot == REACH_TRAY_TOMBSTONE_COUNT)
        {
            slot = index;
        }
    }

    if (slot == REACH_TRAY_TOMBSTONE_COUNT)
    {
        slot = 0;
    }

    provider->tombstones[slot].identity = identity;
    provider->tombstones[slot].index = item_index;
    provider->tombstones[slot].public_id = item->item.id;
    provider->tombstones[slot].tick = GetTickCount();
    provider->tombstones[slot].used = 1;
}

static int32_t reach_tray_take_tombstone(reach_tray_provider *provider,
                                         const reach_tray_identity *identity, size_t *out_index,
                                         uint32_t *out_public_id)
{
    if (provider == nullptr || identity == nullptr || out_index == nullptr ||
        out_public_id == nullptr)
    {
        return 0;
    }

    reach_tray_prune_tombstones(provider);

    for (size_t index = 0; index < REACH_TRAY_TOMBSTONE_COUNT; ++index)
    {
        reach_tray_tombstone *tombstone = &provider->tombstones[index];
        if (!tombstone->used)
        {
            continue;
        }

        if (!reach_tray_identity_equal(&tombstone->identity, identity))
        {
            continue;
        }

        *out_index = tombstone->index;
        *out_public_id = tombstone->public_id;
        *tombstone = {};
        return 1;
    }

    return 0;
}

static void reach_tray_apply_payload(reach_tray_native_item *item, const BYTE *bytes, size_t count)
{
    if (item == nullptr || bytes == nullptr || count < REACH_TRAY_WIRE_BASE_MIN_SIZE)
    {
        return;
    }

    uint32_t flags = reach_tray_read_u32(bytes, count, REACH_TRAY_WIRE_FLAGS_OFFSET);
    item->flags = flags;

    if ((flags & NIF_MESSAGE) != 0)
    {
        item->callback_message = reach_tray_read_u32(bytes, count, REACH_TRAY_WIRE_CALLBACK_OFFSET);
    }

    if ((flags & NIF_ICON) != 0)
    {
        uint32_t raw_icon = reach_tray_read_u32(bytes, count, REACH_TRAY_WIRE_ICON_OFFSET);
        reach_tray_update_item_icon(item, raw_icon);
    }

    if ((flags & NIF_TIP) != 0)
    {
        reach_tray_copy_payload_wstring(item->item.title, 128, bytes, count,
                                        REACH_TRAY_WIRE_TIP_OFFSET, 128);
    }

    if ((flags & NIF_STATE) != 0)
    {
        uint32_t state = reach_tray_read_u32(bytes, count, REACH_TRAY_WIRE_STATE_OFFSET);
        uint32_t state_mask = reach_tray_read_u32(bytes, count, REACH_TRAY_WIRE_STATE_MASK_OFFSET);
        item->state = (item->state & ~state_mask) | (state & state_mask);
    }

    reach_tray_copy_payload_wstring(item->item.icon_ref, 260, bytes, count,
                                    REACH_TRAY_WIRE_EXE_PATH_OFFSET, MAX_PATH);
}

static int32_t reach_tray_item_is_public(const reach_tray_native_item *item)
{
    return item != nullptr && item->owner != nullptr && (item->state & NIS_HIDDEN) == 0 &&
           (item->item.icon_id != 0 || item->item.title[0] != 0);
}

static reach_result reach_tray_handle_copydata(reach_tray_provider *provider,
                                               const COPYDATASTRUCT *copy)
{
    if (provider == nullptr || copy == nullptr || copy->lpData == nullptr)
    {
        return REACH_OK;
    }

    if (copy->dwData != 1 || copy->cbData < REACH_TRAY_WIRE_BASE_MIN_SIZE)
    {
        return REACH_OK;
    }

    const BYTE *bytes = reinterpret_cast<const BYTE *>(copy->lpData);
    size_t count = copy->cbData;

    if (reach_tray_read_u32(bytes, count, REACH_TRAY_WIRE_MAGIC_OFFSET) !=
        REACH_TRAY_WIRE_SIGNATURE)
    {
        return REACH_OK;
    }

    uint32_t message = reach_tray_read_u32(bytes, count, REACH_TRAY_WIRE_MESSAGE_OFFSET);
    HWND owner = reinterpret_cast<HWND>(
        (ULONG_PTR)reach_tray_read_u32(bytes, count, REACH_TRAY_WIRE_HWND_OFFSET));
    uint32_t owner_id = reach_tray_read_u32(bytes, count, REACH_TRAY_WIRE_UID_OFFSET);
    uint32_t flags = reach_tray_read_u32(bytes, count, REACH_TRAY_WIRE_FLAGS_OFFSET);

    GUID guid = {};
    int32_t has_guid = (flags & NIF_GUID) != 0 &&
                       reach_tray_read_guid(bytes, count, REACH_TRAY_WIRE_GUID_OFFSET, &guid) &&
                       !reach_tray_guid_is_null(&guid);

    size_t index = reach_tray_find_item(provider, owner, owner_id, &guid, has_guid);

    if (message == NIM_DELETE)
    {
        if (index != REACH_MAX_TRAY_ITEMS)
        {
            reach_tray_save_tombstone(provider, &provider->items[index], index);
            reach_tray_remove_item_at(provider, index);
        }

        return REACH_OK;
    }

    if (message == NIM_SETVERSION)
    {
        if (index != REACH_MAX_TRAY_ITEMS &&
            REACH_TRAY_WIRE_VERSION_OFFSET + sizeof(uint32_t) <= count)
        {
            provider->items[index].version =
                reach_tray_read_u32(bytes, count, REACH_TRAY_WIRE_VERSION_OFFSET);
            provider->dirty = 1;
        }
        return REACH_OK;
    }

    if (message == NIM_SETFOCUS)
    {
        return REACH_OK;
    }

    if (message == NIM_MODIFY)
    {
        if (index != REACH_MAX_TRAY_ITEMS)
        {
            reach_tray_apply_payload(&provider->items[index], bytes, count);
            provider->dirty = 1;
        }
        return REACH_OK;
    }

    if (message == NIM_ADD)
    {
        if (index == REACH_MAX_TRAY_ITEMS)
        {
            if (provider->item_count >= REACH_MAX_TRAY_ITEMS)
            {
                return REACH_OK;
            }

            reach_tray_identity identity =
                reach_tray_make_identity(owner, owner_id, &guid, has_guid);

            size_t restored_index = provider->item_count;
            uint32_t restored_public_id = 0;

            if (reach_tray_take_tombstone(provider, &identity, &restored_index,
                                          &restored_public_id))
            {
                if (restored_index > provider->item_count)
                {
                    restored_index = provider->item_count;
                }
            }

            for (size_t move = provider->item_count; move > restored_index; --move)
            {
                provider->items[move] = provider->items[move - 1];
            }

            index = restored_index;
            ++provider->item_count;

            reach_tray_initialize_item(provider, &provider->items[index], owner, owner_id, &guid,
                                       has_guid);

            if (restored_public_id != 0)
            {
                provider->items[index].item.id = restored_public_id;
            }
        }
        else
        {
            uint32_t stable_public_id = provider->items[index].item.id;
            reach_tray_release_item_icon(&provider->items[index]);
            reach_tray_initialize_item(provider, &provider->items[index], owner, owner_id, &guid,
                                       has_guid);
            provider->items[index].item.id = stable_public_id;
        }

        provider->items[index].version = 0;
        reach_tray_apply_payload(&provider->items[index], bytes, count);
        provider->dirty = 1;
        return REACH_OK;
    }

    (void)reach_tray_read_u32(bytes, count, REACH_TRAY_WIRE_CBSIZE_OFFSET);
    (void)reach_tray_read_u32(bytes, count, REACH_TRAY_WIRE_INFO_FLAGS_OFFSET);
    return REACH_OK;
}

static void reach_tray_broadcast_taskbar_created(reach_tray_provider *provider)
{
    if (provider == nullptr)
    {
        return;
    }

    provider->taskbar_created_message = RegisterWindowMessageW(L"TaskbarCreated");
    if (provider->taskbar_created_message == 0)
    {
        return;
    }

    SendMessageTimeoutW(HWND_BROADCAST, provider->taskbar_created_message, 0, 0,
                        SMTO_ABORTIFHUNG | SMTO_NORMAL, 2000, nullptr);
}

static LRESULT CALLBACK reach_tray_host_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_NCCREATE)
    {
        CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return TRUE;
    }

    reach_tray_provider *provider =
        reinterpret_cast<reach_tray_provider *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_COPYDATA)
    {
        (void)reach_tray_handle_copydata(provider,
                                         reinterpret_cast<const COPYDATASTRUCT *>(lparam));
        return TRUE;
    }
    if (message == WM_TIMER && wparam == REACH_TRAY_TASKBAR_CREATED_REBROADCAST_TIMER)
    {
        KillTimer(hwnd, REACH_TRAY_TASKBAR_CREATED_REBROADCAST_TIMER);

        reach_tray_provider *provider =
            reinterpret_cast<reach_tray_provider *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        reach_tray_broadcast_taskbar_created(provider);
        return 0;
    }

    (void)wparam;
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static reach_result reach_tray_create_host_window(reach_tray_provider *provider)
{
    REACH_ASSERT(provider != nullptr);
    if (provider == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = reach_tray_host_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = REACH_TRAY_HOST_CLASS;

    provider->host_class = RegisterClassExW(&wc);
    if (provider->host_class == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return REACH_ERROR;
    }

    provider->host_window = CreateWindowExW(0, REACH_TRAY_HOST_CLASS, L"", WS_POPUP, 0, 0, 1, 1,
                                            nullptr, nullptr, GetModuleHandleW(nullptr), provider);

    return provider->host_window != nullptr ? REACH_OK : REACH_ERROR;
}

static reach_result reach_tray_refresh(reach_tray_provider *provider)
{
    REACH_ASSERT(provider != nullptr);
    if (provider == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    size_t index = 0;
    while (index < provider->item_count)
    {
        HWND owner = provider->items[index].owner;
        if (owner == nullptr || !IsWindow(owner))
        {
            reach_tray_save_tombstone(provider, &provider->items[index], index);
            reach_tray_remove_item_at(provider, index);
        }
        else
        {
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
    if (provider == nullptr)
    {
        return 0;
    }

    size_t public_count = 0;
    for (size_t index = 0; index < provider->item_count; ++index)
    {
        if (reach_tray_item_is_public(&provider->items[index]))
        {
            ++public_count;
        }
    }

    return public_count;
}

static reach_result reach_tray_item_at(const reach_tray_provider *provider, size_t index,
                                       reach_tray_item *out_item)
{
    REACH_ASSERT(provider != nullptr);
    REACH_ASSERT(out_item != nullptr);

    if (provider == nullptr || out_item == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    size_t public_index = 0;
    for (size_t native_index = 0; native_index < provider->item_count; ++native_index)
    {
        if (!reach_tray_item_is_public(&provider->items[native_index]))
        {
            continue;
        }

        if (public_index == index)
        {
            *out_item = provider->items[native_index].item;
            return REACH_OK;
        }

        ++public_index;
    }

    return REACH_INVALID_ARGUMENT;
}

static LPARAM reach_tray_v4_lparam(uint32_t notification, uint32_t icon_id)
{
    return MAKELPARAM(notification & 0xffff, icon_id & 0xffff);
}

static WPARAM reach_tray_cursor_wparam(void)
{
    POINT point = {};
    GetCursorPos(&point);
    return (WPARAM)MAKELPARAM(point.x, point.y);
}

static int32_t reach_tray_post_v4_sequence(HWND owner, UINT callback, uint32_t uid,
                                           reach_tray_action action)
{
    WPARAM wparam = reach_tray_cursor_wparam();

    if (action == REACH_TRAY_ACTION_RIGHT_CLICK)
    {
        return PostMessageW(owner, callback, wparam, reach_tray_v4_lparam(WM_RBUTTONDOWN, uid)) &&
               PostMessageW(owner, callback, wparam, reach_tray_v4_lparam(WM_RBUTTONUP, uid)) &&
               PostMessageW(owner, callback, wparam, reach_tray_v4_lparam(WM_CONTEXTMENU, uid));
    }

    return PostMessageW(owner, callback, wparam, reach_tray_v4_lparam(WM_LBUTTONDOWN, uid)) &&
           PostMessageW(owner, callback, wparam, reach_tray_v4_lparam(WM_LBUTTONUP, uid)) &&
           PostMessageW(owner, callback, wparam, reach_tray_v4_lparam(NIN_SELECT, uid));
}

static int32_t reach_tray_post_legacy_sequence(HWND owner, UINT callback, uint32_t uid,
                                               reach_tray_action action)
{
    if (action == REACH_TRAY_ACTION_RIGHT_CLICK)
    {
        return PostMessageW(owner, callback, uid, WM_RBUTTONDOWN) &&
               PostMessageW(owner, callback, uid, WM_RBUTTONUP);
    }

    return PostMessageW(owner, callback, uid, WM_LBUTTONDOWN) &&
           PostMessageW(owner, callback, uid, WM_LBUTTONUP) &&
           PostMessageW(owner, callback, uid, WM_LBUTTONDBLCLK) &&
           PostMessageW(owner, callback, uid, WM_LBUTTONUP);
}

static reach_result reach_tray_cancel_active_menu(reach_tray_provider *provider)
{
    return provider != nullptr ? REACH_OK : REACH_INVALID_ARGUMENT;
}

static reach_result reach_tray_activate(reach_tray_provider *provider, uint32_t item_id,
                                        reach_tray_action action)
{
    REACH_ASSERT(provider != nullptr);
    if (provider == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < provider->item_count; ++index)
    {
        reach_tray_native_item *item = &provider->items[index];
        if (item->item.id != item_id || item->owner == nullptr || item->callback_message == 0)
        {
            continue;
        }

        if (!IsWindow(item->owner))
        {
            reach_tray_remove_item_at(provider, index);
            return REACH_INVALID_ARGUMENT;
        }

        if (action == REACH_TRAY_ACTION_RIGHT_CLICK)
        {
            SetForegroundWindow(item->owner);
        }

        if (item->version >= NOTIFYICON_VERSION_4)
        {
            return reach_tray_post_v4_sequence(item->owner, item->callback_message, item->owner_id,
                                               action)
                       ? REACH_OK
                       : REACH_ERROR;
        }

        return reach_tray_post_legacy_sequence(item->owner, item->callback_message, item->owner_id,
                                               action)
                   ? REACH_OK
                   : REACH_ERROR;
    }

    return REACH_INVALID_ARGUMENT;
}

static void reach_tray_destroy(reach_tray_provider *provider)
{
    if (provider != nullptr)
    {
        for (size_t index = 0; index < provider->item_count; ++index)
        {
            reach_tray_release_item_icon(&provider->items[index]);
        }

        if (provider->host_window != nullptr)
        {
            DestroyWindow(provider->host_window);
            provider->host_window = nullptr;
        }
    }

    delete provider;
}

reach_result reach_windows_create_tray_provider(reach_tray_provider_port *out_port)
{
    REACH_ASSERT(out_port != nullptr);
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};

    reach_tray_provider *provider = new (std::nothrow) reach_tray_provider();
    if (provider == nullptr)
    {
        return REACH_ERROR;
    }

    reach_result result = reach_tray_create_host_window(provider);
    if (result != REACH_OK)
    {
        delete provider;
        return result;
    }

    reach_tray_broadcast_taskbar_created(provider);
    if (provider->host_window != nullptr)
    {
        SetTimer(provider->host_window, REACH_TRAY_TASKBAR_CREATED_REBROADCAST_TIMER, 3000,
                 nullptr);
    }

    out_port->provider = provider;
    out_port->ops.refresh = reach_tray_refresh;
    out_port->ops.needs_refresh = reach_tray_needs_refresh;
    out_port->ops.item_count = reach_tray_item_count;
    out_port->ops.item_at = reach_tray_item_at;
    out_port->ops.activate = reach_tray_activate;
    out_port->ops.cancel_active_menu = reach_tray_cancel_active_menu;
    out_port->ops.destroy = reach_tray_destroy;

    return REACH_OK;
}
