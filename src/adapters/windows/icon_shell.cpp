#include "reach/platform/windows_adapters.h"

#include "reach/ports/icon_provider.h"

#include <windows.h>
#include <shellapi.h>

#include <new>

struct reach_icon_provider {
    uint64_t next_id;
};

static reach_result reach_icon_load(reach_icon_provider *provider, const reach_icon_request *request, reach_icon_handle *out_icon)
{
    REACH_ASSERT(provider != nullptr);
    REACH_ASSERT(request != nullptr);
    REACH_ASSERT(out_icon != nullptr);
    if (provider == nullptr || request == nullptr || out_icon == nullptr || request->path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    SHFILEINFOW info = {};
    UINT flags = SHGFI_ICON | (request->size_px > 32 ? SHGFI_LARGEICON : SHGFI_SMALLICON);
    DWORD_PTR result = SHGetFileInfoW(reinterpret_cast<const wchar_t *>(request->path), 0, &info, sizeof(info), flags);
    if (result == 0 || info.hIcon == nullptr) {
        return REACH_ERROR;
    }

    *out_icon = {};
    out_icon->id = reinterpret_cast<uint64_t>(info.hIcon);
    reach_copy_utf16(out_icon->debug_name, 260, request->path);
    provider->next_id += 1;
    return REACH_OK;
}

static reach_result reach_icon_release(reach_icon_provider *provider, reach_icon_handle icon)
{
    REACH_ASSERT(provider != nullptr);
    if (icon.id != 0) {
        DestroyIcon(reinterpret_cast<HICON>(icon.id));
    }
    return REACH_OK;
}

static void reach_icon_destroy(reach_icon_provider *provider)
{
    delete provider;
}

reach_result reach_windows_create_icon_provider(reach_icon_provider_port *out_port)
{
    REACH_ASSERT(out_port != nullptr);
    if (out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_icon_provider *provider = new (std::nothrow) reach_icon_provider();
    if (provider == nullptr) {
        return REACH_ERROR;
    }

    provider->next_id = 1;
    out_port->provider = provider;
    out_port->ops.load = reach_icon_load;
    out_port->ops.release = reach_icon_release;
    out_port->ops.destroy = reach_icon_destroy;
    return REACH_OK;
}
