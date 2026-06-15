#include "windows_adapters_internal.h"

#include "reach/ports/icon_provider.h"
#include "windows_icon_handle_internal.h"

#include <commctrl.h>
#include <commoncontrols.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shobjidl.h>

#include <climits>
#include <cstdlib>
#include <mutex>
#include <new>

struct reach_icon_provider
{
    uint64_t next_id;
    std::mutex mutex;
};

struct reach_icon_com_scope
{
    HRESULT hr;
    int32_t uninitialize;

    reach_icon_com_scope()
        : hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)), uninitialize(SUCCEEDED(hr) ? 1 : 0)
    {
    }

    ~reach_icon_com_scope()
    {
        if (uninitialize)
        {
            CoUninitialize();
        }
    }
};

static int32_t reach_icon_copy_path(wchar_t *dst, DWORD dst_count, const wchar_t *src)
{
    if (dst == nullptr || dst_count == 0 || src == nullptr || src[0] == 0)
    {
        return 0;
    }
    return wcscpy_s(dst, dst_count, src) == 0;
}

static int32_t reach_icon_resolve_search_path(const wchar_t *path, wchar_t *resolved,
                                              DWORD resolved_count)
{
    if (path == nullptr || resolved == nullptr || resolved_count == 0)
    {
        return 0;
    }

    DWORD length = SearchPathW(nullptr, path, nullptr, resolved_count, resolved, nullptr);
    return length > 0 && length < resolved_count;
}

static int32_t reach_icon_split_resource_ref(const wchar_t *path, wchar_t *out_path,
                                             DWORD out_path_count, int *out_icon_index)
{
    if (path == nullptr || out_path == nullptr || out_path_count == 0 || out_icon_index == nullptr)
    {
        return 0;
    }

    *out_icon_index = 0;
    out_path[0] = 0;

    const wchar_t *comma = wcsrchr(path, L',');
    if (comma == nullptr || comma == path || comma[1] == 0)
    {
        return 0;
    }

    wchar_t *end = nullptr;
    long icon_index = wcstol(comma + 1, &end, 10);
    if (end == comma + 1 || *end != 0 || icon_index < INT_MIN || icon_index > INT_MAX)
    {
        return 0;
    }

    size_t path_count = (size_t)(comma - path);
    if (path_count == 0 || path_count >= out_path_count)
    {
        return 0;
    }

    wmemcpy(out_path, path, path_count);
    out_path[path_count] = 0;
    *out_icon_index = (int)icon_index;
    return 1;
}

static int32_t reach_icon_resolve_shortcut(const wchar_t *path, wchar_t *resolved,
                                           DWORD resolved_count)
{
    if (path == nullptr || resolved == nullptr || resolved_count == 0 ||
        lstrcmpiW(PathFindExtensionW(path), L".lnk") != 0)
    {
        return 0;
    }

    IShellLinkW *link = nullptr;
    HRESULT hr =
        CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link));
    if (FAILED(hr))
    {
        return 0;
    }

    IPersistFile *persist = nullptr;
    hr = link->QueryInterface(IID_PPV_ARGS(&persist));
    if (SUCCEEDED(hr))
    {
        hr = persist->Load(path, STGM_READ);
    }
    if (SUCCEEDED(hr))
    {
        hr = link->Resolve(nullptr, SLR_NO_UI | SLR_NOSEARCH | SLR_NOTRACK);
    }
    if (SUCCEEDED(hr))
    {
        hr = link->GetPath(resolved, resolved_count, nullptr, SLGP_UNCPRIORITY);
    }

    if (persist != nullptr)
    {
        persist->Release();
    }
    link->Release();
    return SUCCEEDED(hr) && resolved[0] != 0;
}

static HBITMAP reach_icon_bitmap_from_shell_item_image_factory(const wchar_t *path, int32_t size_px)
{
    if (path == nullptr || path[0] == 0)
    {
        return nullptr;
    }

    IShellItemImageFactory *factory = nullptr;
    HRESULT hr = SHCreateItemFromParsingName(path, nullptr, IID_PPV_ARGS(&factory));

    if (FAILED(hr) || factory == nullptr)
    {
        return nullptr;
    }

    int32_t requested_size = size_px;
    if (requested_size < 64)
    {
        requested_size = 64;
    }

    SIZE size = {};
    size.cx = requested_size;
    size.cy = requested_size;

    HBITMAP bitmap = nullptr;
    hr = factory->GetImage(size, SIIGBF_BIGGERSIZEOK | SIIGBF_ICONONLY, &bitmap);

    factory->Release();

    return SUCCEEDED(hr) ? bitmap : nullptr;
}

static HICON reach_icon_from_shell_file_info(const wchar_t *path, int32_t size_px)
{
    if (path == nullptr || path[0] == 0)
    {
        return nullptr;
    }

    SHFILEINFOW info = {};
    UINT flags = SHGFI_ICON | (size_px > 32 ? SHGFI_LARGEICON : SHGFI_SMALLICON);
    DWORD_PTR result = SHGetFileInfoW(path, 0, &info, sizeof(info), flags);
    return result != 0 ? info.hIcon : nullptr;
}

static HICON reach_icon_from_system_image_list_size(const wchar_t *path, int image_list_size)
{
    if (path == nullptr || path[0] == 0)
    {
        return nullptr;
    }

    SHFILEINFOW info = {};
    DWORD_PTR result = SHGetFileInfoW(path, 0, &info, sizeof(info), SHGFI_SYSICONINDEX);
    if (result == 0 || info.iIcon < 0)
    {
        return nullptr;
    }

    IImageList *image_list = nullptr;
    HRESULT hr =
        SHGetImageList(image_list_size, IID_IImageList, reinterpret_cast<void **>(&image_list));
    if (FAILED(hr) || image_list == nullptr)
    {
        return nullptr;
    }

    HICON icon = nullptr;
    hr = image_list->GetIcon(info.iIcon, ILD_TRANSPARENT, &icon);
    image_list->Release();
    return SUCCEEDED(hr) ? icon : nullptr;
}

static HICON reach_icon_from_system_image_list(const wchar_t *path, int32_t size_px)
{
    HICON icon = nullptr;
    if (size_px > 96)
    {
        icon = reach_icon_from_system_image_list_size(path, SHIL_JUMBO);
        if (icon != nullptr)
        {
            return icon;
        }
    }
    return reach_icon_from_system_image_list_size(path, SHIL_EXTRALARGE);
}

static HICON reach_icon_from_extract_icon(const wchar_t *path, int32_t size_px, int icon_index)
{
    if (path == nullptr || path[0] == 0)
    {
        return nullptr;
    }

    int32_t requested_size = size_px;
    if (requested_size < 64)
    {
        requested_size = 64;
    }
    if (requested_size > 256)
    {
        requested_size = 256;
    }

    HICON icon = nullptr;
    UINT icon_id = 0;

    UINT count =
        PrivateExtractIconsW(path, icon_index, requested_size, requested_size, &icon, &icon_id, 1,
                             0);

    return count > 0 && icon != nullptr ? icon : nullptr;
}

static reach_result reach_icon_load(reach_icon_provider *provider,
                                    const reach_icon_request *request, reach_icon_handle *out_icon)
{
    REACH_ASSERT(provider != nullptr);
    REACH_ASSERT(request != nullptr);
    REACH_ASSERT(out_icon != nullptr);
    if (provider == nullptr || request == nullptr || out_icon == nullptr || request->path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_icon_com_scope com_scope;
    std::lock_guard<std::mutex> lock(provider->mutex);

    const wchar_t *requested_path = reinterpret_cast<const wchar_t *>(request->path);
    wchar_t resource_ref_path[260] = {};
    wchar_t resolved[260] = {};
    wchar_t shortcut_target[260] = {};
    const wchar_t *icon_path = requested_path;
    int icon_index = 0;
    int32_t resource_ref = reach_icon_split_resource_ref(requested_path, resource_ref_path, 260,
                                                        &icon_index);
    if (resource_ref)
    {
        icon_path = resource_ref_path;
    }
    else if (reach_icon_resolve_shortcut(requested_path, shortcut_target, 260))
    {
        icon_path = shortcut_target;
    }
    else if (reach_icon_resolve_search_path(requested_path, resolved, 260))
    {
        icon_path = resolved;
    }
    else
    {
        (void)reach_icon_copy_path(resolved, 260, requested_path);
        icon_path = resolved;
    }

    uint64_t icon_id = 0;
    HICON hicon = nullptr;
    const wchar_t *extension = PathFindExtensionW(icon_path);
    int32_t is_exe = lstrcmpiW(extension, L".exe") == 0;
    if (is_exe)
    {
        hicon = reach_icon_from_extract_icon(icon_path, request->size_px, icon_index);
        icon_id = reach_windows_icon_id_from_hicon(hicon);
    }

    HBITMAP bitmap = nullptr;
    if (icon_id == 0)
    {
        bitmap = reach_icon_bitmap_from_shell_item_image_factory(icon_path, request->size_px);
        if (bitmap != nullptr)
        {
            icon_id = reach_windows_icon_id_from_hbitmap(bitmap);
        }
    }

    if (icon_id == 0)
    {
        hicon = reach_icon_from_system_image_list(icon_path, request->size_px);
        icon_id = reach_windows_icon_id_from_hicon(hicon);
    }
    if (icon_id == 0)
    {
        hicon = reach_icon_from_shell_file_info(icon_path, request->size_px);
        icon_id = reach_windows_icon_id_from_hicon(hicon);
    }
    if (icon_id == 0 && !is_exe)
    {
        hicon = reach_icon_from_extract_icon(icon_path, request->size_px, icon_index);
        icon_id = reach_windows_icon_id_from_hicon(hicon);
    }
    if (icon_id == 0 && icon_path != requested_path)
    {
        bitmap = reach_icon_bitmap_from_shell_item_image_factory(requested_path, request->size_px);
        icon_id = reach_windows_icon_id_from_hbitmap(bitmap);
    }
    if (icon_id == 0 && icon_path != requested_path)
    {
        hicon = reach_icon_from_shell_file_info(requested_path, request->size_px);
        icon_id = reach_windows_icon_id_from_hicon(hicon);
    }
    if (icon_id == 0)
    {
        return REACH_ERROR;
    }

    *out_icon = {};
    out_icon->id = icon_id;
    reach_copy_utf16(out_icon->debug_name, 260, reinterpret_cast<const uint16_t *>(icon_path));
    provider->next_id += 1;
    return REACH_OK;
}

static reach_result reach_icon_release(reach_icon_provider *provider, reach_icon_handle icon)
{
    if (provider != nullptr)
    {
        std::lock_guard<std::mutex> lock(provider->mutex);
        reach_windows_icon_id_release(icon.id);
        return REACH_OK;
    }
    reach_windows_icon_id_release(icon.id);
    return REACH_OK;
}

static void reach_icon_destroy(reach_icon_provider *provider)
{
    delete provider;
}

reach_result reach_windows_create_icon_provider(reach_icon_provider_port *out_port)
{
    REACH_ASSERT(out_port != nullptr);
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_icon_provider *provider = new (std::nothrow) reach_icon_provider();
    if (provider == nullptr)
    {
        return REACH_ERROR;
    }

    provider->next_id = 1;
    out_port->provider = provider;
    out_port->ops.load = reach_icon_load;
    out_port->ops.release = reach_icon_release;
    out_port->ops.destroy = reach_icon_destroy;
    return REACH_OK;
}
