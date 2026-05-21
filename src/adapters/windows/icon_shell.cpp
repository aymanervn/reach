#include "reach/platform/windows_adapters.h"

#include "reach/ports/icon_provider.h"

#include <windows.h>
#include <commctrl.h>
#include <commoncontrols.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shobjidl.h>

#include <new>
#include <vector>

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

static HICON reach_icon_from_system_image_list_size(const wchar_t *path, int image_list_size)
{
    if (path == nullptr || path[0] == 0) {
        return nullptr;
    }

    SHFILEINFOW info = {};
    DWORD_PTR result = SHGetFileInfoW(path, 0, &info, sizeof(info), SHGFI_SYSICONINDEX);
    if (result == 0 || info.iIcon < 0) {
        return nullptr;
    }

    IImageList *image_list = nullptr;
    HRESULT hr = SHGetImageList(image_list_size, IID_IImageList, reinterpret_cast<void **>(&image_list));
    if (FAILED(hr) || image_list == nullptr) {
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
    if (size_px > 96) {
        icon = reach_icon_from_system_image_list_size(path, SHIL_JUMBO);
        if (icon != nullptr) {
            return icon;
        }
    }
    return reach_icon_from_system_image_list_size(path, SHIL_EXTRALARGE);
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

static int32_t reach_icon_corner_has_content(const std::vector<uint32_t> &pixels, int width, int height, int corner_x, int corner_y)
{
    int sample = width < height ? width / 6 : height / 6;
    if (sample < 3) {
        sample = 3;
    }
    int start_x = corner_x == 0 ? 0 : width - sample;
    int start_y = corner_y == 0 ? 0 : height - sample;
    for (int y = 0; y < sample; ++y) {
        for (int x = 0; x < sample; ++x) {
            uint8_t alpha = (uint8_t)(pixels[(start_y + y) * width + start_x + x] >> 24);
            if (alpha > 32) {
                return 1;
            }
        }
    }
    return 0;
}

static int32_t reach_icon_classify_backplate(HICON icon)
{
    if (icon == nullptr) {
        return 0;
    }

    ICONINFO info = {};
    if (!GetIconInfo(icon, &info) || info.hbmColor == nullptr) {
        if (info.hbmColor != nullptr) {
            DeleteObject(info.hbmColor);
        }
        if (info.hbmMask != nullptr) {
            DeleteObject(info.hbmMask);
        }
        return 0;
    }

    BITMAP bitmap = {};
    if (GetObjectW(info.hbmColor, sizeof(bitmap), &bitmap) == 0 || bitmap.bmWidth <= 0 || bitmap.bmHeight <= 0) {
        DeleteObject(info.hbmColor);
        if (info.hbmMask != nullptr) {
            DeleteObject(info.hbmMask);
        }
        return 0;
    }

    int width = bitmap.bmWidth;
    int height = bitmap.bmHeight;
    std::vector<uint32_t> pixels((size_t)width * (size_t)height);

    BITMAPINFO dib = {};
    dib.bmiHeader.biSize = sizeof(dib.bmiHeader);
    dib.bmiHeader.biWidth = width;
    dib.bmiHeader.biHeight = -height;
    dib.bmiHeader.biPlanes = 1;
    dib.bmiHeader.biBitCount = 32;
    dib.bmiHeader.biCompression = BI_RGB;

    HDC dc = GetDC(nullptr);
    int scanlines = dc != nullptr
        ? GetDIBits(dc, info.hbmColor, 0, (UINT)height, pixels.data(), &dib, DIB_RGB_COLORS)
        : 0;
    if (dc != nullptr) {
        ReleaseDC(nullptr, dc);
    }
    DeleteObject(info.hbmColor);
    if (info.hbmMask != nullptr) {
        DeleteObject(info.hbmMask);
    }
    if (scanlines == 0) {
        return 0;
    }

    int min_x = width;
    int min_y = height;
    int max_x = -1;
    int max_y = -1;
    int content = 0;
    int strong_alpha = 0;
    int alpha_seen = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t alpha = (uint8_t)(pixels[(size_t)y * (size_t)width + (size_t)x] >> 24);
            if (alpha > 0) {
                alpha_seen = 1;
            }
            if (alpha > 32) {
                ++content;
                if (alpha > 220) {
                    ++strong_alpha;
                }
                if (x < min_x) min_x = x;
                if (y < min_y) min_y = y;
                if (x > max_x) max_x = x;
                if (y > max_y) max_y = y;
            }
        }
    }

    if (!alpha_seen || content == 0) {
        return 0;
    }

    float total = (float)(width * height);
    float content_ratio = (float)content / total;
    float bbox_width_ratio = (float)(max_x - min_x + 1) / (float)width;
    float bbox_height_ratio = (float)(max_y - min_y + 1) / (float)height;
    float bbox_aspect = bbox_height_ratio > 0.0f ? bbox_width_ratio / bbox_height_ratio : 1.0f;
    int transparent_corners = 0;
    transparent_corners += !reach_icon_corner_has_content(pixels, width, height, 0, 0);
    transparent_corners += !reach_icon_corner_has_content(pixels, width, height, 1, 0);
    transparent_corners += !reach_icon_corner_has_content(pixels, width, height, 0, 1);
    transparent_corners += !reach_icon_corner_has_content(pixels, width, height, 1, 1);

    int near_square_box = bbox_width_ratio >= 0.86f &&
        bbox_height_ratio >= 0.86f &&
        bbox_aspect >= 0.90f &&
        bbox_aspect <= 1.10f;
    if (near_square_box && transparent_corners <= 1 && content_ratio >= 0.88f) {
        return 0;
    }
    if (near_square_box && transparent_corners >= 2 && content_ratio >= 0.92f) {
        return 0;
    }
    return 1;
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

    HICON icon = reach_icon_from_system_image_list(icon_path, request->size_px);
    if (icon == nullptr) {
        icon = reach_icon_from_shell_file_info(icon_path, request->size_px);
    }
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
    out_icon->wants_backplate = reach_icon_classify_backplate(icon);
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
