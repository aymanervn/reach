#include "reach/platform/windows_adapters.h"

#include "reach/ports/icon_provider.h"

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shobjidl.h>

#include <new>

struct reach_icon_provider {
    uint64_t next_id;
};

static int32_t reach_icon_copy_path(wchar_t *dst, DWORD dst_count, const wchar_t *src)
{
    if (dst == nullptr || dst_count == 0 || src == nullptr || src[0] == 0) {
        return 0;
    }
    return wcscpy_s(dst, dst_count, src) == 0;
}

static int32_t reach_icon_resolve_search_path(const wchar_t *path, wchar_t *resolved, DWORD resolved_count)
{
    if (path == nullptr || resolved == nullptr || resolved_count == 0) {
        return 0;
    }

    DWORD length = SearchPathW(nullptr, path, nullptr, resolved_count, resolved, nullptr);
    return length > 0 && length < resolved_count;
}

static int32_t reach_icon_resolve_shortcut(const wchar_t *path, wchar_t *resolved, DWORD resolved_count)
{
    if (path == nullptr || resolved == nullptr || resolved_count == 0 || lstrcmpiW(PathFindExtensionW(path), L".lnk") != 0) {
        return 0;
    }

    IShellLinkW *link = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link));
    if (FAILED(hr)) {
        return 0;
    }

    IPersistFile *persist = nullptr;
    hr = link->QueryInterface(IID_PPV_ARGS(&persist));
    if (SUCCEEDED(hr)) {
        hr = persist->Load(path, STGM_READ);
    }
    if (SUCCEEDED(hr)) {
        hr = link->Resolve(nullptr, SLR_NO_UI | SLR_NOSEARCH | SLR_NOTRACK);
    }
    if (SUCCEEDED(hr)) {
        hr = link->GetPath(resolved, resolved_count, nullptr, SLGP_UNCPRIORITY);
    }

    if (persist != nullptr) {
        persist->Release();
    }
    link->Release();
    return SUCCEEDED(hr) && resolved[0] != 0;
}

static HICON reach_icon_from_shell_file_info(const wchar_t *path, int32_t size_px)
{
    if (path == nullptr || path[0] == 0) {
        return nullptr;
    }

    SHFILEINFOW info = {};
    UINT flags = SHGFI_ICON | (size_px > 32 ? SHGFI_LARGEICON : SHGFI_SMALLICON);
    DWORD_PTR result = SHGetFileInfoW(path, 0, &info, sizeof(info), flags);
    return result != 0 ? info.hIcon : nullptr;
}

static HICON reach_icon_from_extract_icon(const wchar_t *path, int32_t size_px)
{
    if (path == nullptr || path[0] == 0) {
        return nullptr;
    }

    HICON large_icon = nullptr;
    HICON small_icon = nullptr;
    UINT count = ExtractIconExW(path, 0, &large_icon, &small_icon, 1);
    if (count == 0) {
        return nullptr;
    }

    HICON selected = size_px > 32 ? large_icon : small_icon;
    HICON unused = size_px > 32 ? small_icon : large_icon;
    if (unused != nullptr) {
        DestroyIcon(unused);
    }
    return selected;
}

static reach_result reach_icon_load(reach_icon_provider *provider, const reach_icon_request *request, reach_icon_handle *out_icon)
{
    REACH_ASSERT(provider != nullptr);
    REACH_ASSERT(request != nullptr);
    REACH_ASSERT(out_icon != nullptr);
    if (provider == nullptr || request == nullptr || out_icon == nullptr || request->path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    const wchar_t *requested_path = reinterpret_cast<const wchar_t *>(request->path);
    wchar_t resolved[260] = {};
    wchar_t shortcut_target[260] = {};
    const wchar_t *icon_path = requested_path;
    if (reach_icon_resolve_shortcut(requested_path, shortcut_target, 260)) {
        icon_path = shortcut_target;
    } else if (reach_icon_resolve_search_path(requested_path, resolved, 260)) {
        icon_path = resolved;
    } else {
        (void)reach_icon_copy_path(resolved, 260, requested_path);
        icon_path = resolved;
    }

    HICON icon = reach_icon_from_shell_file_info(icon_path, request->size_px);
    if (icon == nullptr) {
        icon = reach_icon_from_extract_icon(icon_path, request->size_px);
    }
    if (icon == nullptr && icon_path != requested_path) {
        icon = reach_icon_from_shell_file_info(requested_path, request->size_px);
    }
    if (icon == nullptr) {
        return REACH_ERROR;
    }

    *out_icon = {};
    out_icon->id = reinterpret_cast<uint64_t>(icon);
    reach_copy_utf16(out_icon->debug_name, 260, reinterpret_cast<const uint16_t *>(icon_path));
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
