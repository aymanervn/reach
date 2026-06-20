#include "windows_adapters_internal.h"

#include "reach/ports/clipboard.h"

#include "windows_icon_handle_internal.h"

#include <windows.h>

#include <new>
#include <vector>
#include <shellapi.h>
#include <shobjidl_core.h>
#include <wchar.h>
struct reach_clipboard_payload
{
    uint64_t id;
    reach_clipboard_item_kind kind;
    std::vector<uint8_t> bytes;
    uint64_t thumbnail_id;
    UINT format;
};

struct reach_clipboard_provider
{
    HWND window;
    ATOM window_class;
    reach_clipboard_changed_callback callback;
    void *callback_user;
    uint64_t next_id;
    DWORD suppressed_sequence;
    std::vector<reach_clipboard_payload *> payloads;
};

static const wchar_t *REACH_CLIPBOARD_WINDOW_CLASS = L"ReachClipboardListenerWindow";

static uint64_t reach_clipboard_hash_bytes(const uint8_t *bytes, size_t size)
{
    uint64_t hash = 1469598103934665603ull;
    for (size_t index = 0; index < size; ++index)
    {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

static reach_clipboard_payload *
reach_clipboard_find_payload(reach_clipboard_provider *provider, uint64_t id)
{
    if (provider == nullptr)
    {
        return nullptr;
    }
    for (reach_clipboard_payload *payload : provider->payloads)
    {
        if (payload != nullptr && payload->id == id)
        {
            return payload;
        }
    }
    return nullptr;
}

static HBITMAP reach_clipboard_bitmap_from_dib(const uint8_t *bytes, size_t size,
                                               uint32_t *out_width, uint32_t *out_height)
{
    if (bytes == nullptr || size < sizeof(BITMAPINFOHEADER))
    {
        return nullptr;
    }
    const BITMAPINFOHEADER *header = reinterpret_cast<const BITMAPINFOHEADER *>(bytes);
    if (header->biSize < sizeof(BITMAPINFOHEADER) || header->biSize > size ||
        header->biWidth <= 0 || header->biHeight == 0)
    {
        return nullptr;
    }
    uint64_t width = static_cast<uint64_t>(header->biWidth);
    uint64_t height =
        static_cast<uint64_t>(header->biHeight < 0 ? -header->biHeight : header->biHeight);
    if (width > 32768 || height > 32768)
    {
        return nullptr;
    }

    size_t colors = header->biClrUsed;
    if (colors == 0 && header->biBitCount <= 8)
    {
        colors = (size_t)1u << header->biBitCount;
    }
    size_t pixel_offset = header->biSize + colors * sizeof(RGBQUAD);
    if (header->biCompression == BI_BITFIELDS && header->biSize == sizeof(BITMAPINFOHEADER))
    {
        pixel_offset += 3 * sizeof(DWORD);
    }
    if (pixel_offset > size)
    {
        return nullptr;
    }

    HDC dc = GetDC(nullptr);
    HBITMAP bitmap =
        CreateDIBitmap(dc, header, CBM_INIT, bytes + pixel_offset,
                       reinterpret_cast<const BITMAPINFO *>(bytes), DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    if (bitmap != nullptr)
    {
        *out_width = static_cast<uint32_t>(width);
        *out_height = static_cast<uint32_t>(height);
    }
    return bitmap;
}

static HBITMAP reach_clipboard_scale_bitmap(HBITMAP source, uint32_t width, uint32_t height)
{
    if (source == nullptr || width == 0 || height == 0)
    {
        return nullptr;
    }
    const int32_t limit = 256;
    float scale = width > height ? (float)limit / (float)width : (float)limit / (float)height;
    if (scale > 1.0f)
    {
        scale = 1.0f;
    }
    int target_width = (int)((float)width * scale + 0.5f);
    int target_height = (int)((float)height * scale + 0.5f);
    if (target_width < 1)
        target_width = 1;
    if (target_height < 1)
        target_height = 1;

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = target_width;
    info.bmiHeader.biHeight = -target_height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *pixels = nullptr;
    HDC screen = GetDC(nullptr);
    HBITMAP thumbnail = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    HDC source_dc = CreateCompatibleDC(screen);
    HDC target_dc = CreateCompatibleDC(screen);
    HGDIOBJ old_source = SelectObject(source_dc, source);
    HGDIOBJ old_target = SelectObject(target_dc, thumbnail);
    SetStretchBltMode(target_dc, HALFTONE);
    SetBrushOrgEx(target_dc, 0, 0, nullptr);
    BOOL ok = StretchBlt(target_dc, 0, 0, target_width, target_height, source_dc, 0, 0,
                         (int)width, (int)height, SRCCOPY);

    if (ok && pixels != nullptr)
    {
        uint8_t *pixel_bytes = static_cast<uint8_t *>(pixels);
        const size_t pixel_count = (size_t)target_width * (size_t)target_height;
        for (size_t index = 0; index < pixel_count; ++index)
        {
            pixel_bytes[index * 4u + 3u] = 0xFF;
        }
    }
    SelectObject(source_dc, old_source);
    SelectObject(target_dc, old_target);
    DeleteDC(source_dc);
    DeleteDC(target_dc);
    ReleaseDC(nullptr, screen);
    if (!ok)
    {
        DeleteObject(thumbnail);
        return nullptr;
    }
    return thumbnail;
}

static reach_result reach_clipboard_capture_text(reach_clipboard_provider *provider,
                                                 reach_clipboard_item *out_item)
{
    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (handle == nullptr)
    {
        return REACH_ERROR;
    }
    const wchar_t *text = static_cast<const wchar_t *>(GlobalLock(handle));
    if (text == nullptr)
    {
        return REACH_ERROR;
    }
    SIZE_T global_size = GlobalSize(handle);
    size_t capacity = global_size / sizeof(wchar_t);
    if (capacity == 0)
    {
        GlobalUnlock(handle);
        return REACH_ERROR;
    }
    size_t length = wcsnlen(text, capacity);
    if (length == capacity)
    {
        GlobalUnlock(handle);
        return REACH_ERROR;
    }
    size_t byte_count = (length + 1) * sizeof(wchar_t);
    reach_clipboard_payload *payload = new (std::nothrow) reach_clipboard_payload();
    if (payload == nullptr)
    {
        GlobalUnlock(handle);
        return REACH_ERROR;
    }
    try
    {
        payload->bytes.resize(byte_count);
    }
    catch (...)
    {
        delete payload;
        GlobalUnlock(handle);
        return REACH_ERROR;
    }
    memcpy(payload->bytes.data(), text, byte_count);
    GlobalUnlock(handle);
    payload->id = ++provider->next_id;
    payload->kind = REACH_CLIPBOARD_ITEM_TEXT;
    payload->thumbnail_id = 0;
    payload->format = CF_UNICODETEXT;
    try
    {
        provider->payloads.push_back(payload);
    }
    catch (...)
    {
        delete payload;
        return REACH_ERROR;
    }

    out_item->id = payload->id;
    out_item->kind = payload->kind;
    out_item->content_hash = reach_clipboard_hash_bytes(payload->bytes.data(), byte_count);
    reach_clipboard_build_text_preview(reinterpret_cast<const uint16_t *>(payload->bytes.data()),
                                       out_item->preview, REACH_CLIPBOARD_PREVIEW_CAPACITY);
    return REACH_OK;
}

static reach_result reach_clipboard_capture_dib(reach_clipboard_provider *provider, UINT format,
                                                reach_clipboard_item *out_item)
{
    HANDLE handle = GetClipboardData(format);
    SIZE_T byte_count = handle != nullptr ? GlobalSize(handle) : 0;
    if (handle == nullptr || byte_count < sizeof(BITMAPINFOHEADER))
    {
        return REACH_ERROR;
    }
    const uint8_t *bytes = static_cast<const uint8_t *>(GlobalLock(handle));
    if (bytes == nullptr)
    {
        return REACH_ERROR;
    }
    reach_clipboard_payload *payload = new (std::nothrow) reach_clipboard_payload();
    if (payload == nullptr)
    {
        GlobalUnlock(handle);
        return REACH_ERROR;
    }
    try
    {
        payload->bytes.assign(bytes, bytes + byte_count);
    }
    catch (...)
    {
        delete payload;
        GlobalUnlock(handle);
        return REACH_ERROR;
    }
    GlobalUnlock(handle);

    uint32_t width = 0;
    uint32_t height = 0;
    HBITMAP source =
        reach_clipboard_bitmap_from_dib(payload->bytes.data(), payload->bytes.size(), &width, &height);
    if (source == nullptr)
    {
        delete payload;
        return REACH_ERROR;
    }
    HBITMAP thumbnail = reach_clipboard_scale_bitmap(source, width, height);
    DeleteObject(source);
    if (thumbnail == nullptr)
    {
        delete payload;
        return REACH_ERROR;
    }
    payload->id = ++provider->next_id;
    payload->kind = REACH_CLIPBOARD_ITEM_IMAGE;
    payload->thumbnail_id = reach_windows_icon_id_from_hbitmap(thumbnail);
    payload->format = format;
    if (payload->thumbnail_id == 0)
    {
        delete payload;
        return REACH_ERROR;
    }
    try
    {
        provider->payloads.push_back(payload);
    }
    catch (...)
    {
        reach_windows_icon_id_release(payload->thumbnail_id);
        delete payload;
        return REACH_ERROR;
    }

    out_item->id = payload->id;
    out_item->kind = payload->kind;
    out_item->content_hash =
        reach_clipboard_hash_bytes(payload->bytes.data(), payload->bytes.size());
    out_item->thumbnail_id = payload->thumbnail_id;
    out_item->image_width = width;
    out_item->image_height = height;
    _snwprintf_s(reinterpret_cast<wchar_t *>(out_item->preview),
                 REACH_CLIPBOARD_PREVIEW_CAPACITY, _TRUNCATE, L"%u \u00D7 %u image", width, height);
    return REACH_OK;
}

static reach_result reach_clipboard_capture_bitmap(reach_clipboard_provider *provider,
                                                   reach_clipboard_item *out_item)
{
    HBITMAP bitmap = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
    BITMAP bitmap_info = {};
    if (bitmap == nullptr || GetObjectW(bitmap, sizeof(bitmap_info), &bitmap_info) == 0 ||
        bitmap_info.bmWidth <= 0 || bitmap_info.bmHeight <= 0)
    {
        return REACH_ERROR;
    }
    uint64_t stride = ((uint64_t)bitmap_info.bmWidth * 32u + 31u) / 32u * 4u;
    uint64_t pixel_bytes = stride * (uint64_t)bitmap_info.bmHeight;
    if (pixel_bytes > SIZE_MAX - sizeof(BITMAPINFOHEADER))
    {
        return REACH_ERROR;
    }
    reach_clipboard_payload *payload = new (std::nothrow) reach_clipboard_payload();
    if (payload == nullptr)
    {
        return REACH_ERROR;
    }
    try
    {
        payload->bytes.resize(sizeof(BITMAPINFOHEADER) + (size_t)pixel_bytes);
    }
    catch (...)
    {
        delete payload;
        return REACH_ERROR;
    }
    BITMAPINFOHEADER *header =
        reinterpret_cast<BITMAPINFOHEADER *>(payload->bytes.data());
    *header = {};
    header->biSize = sizeof(BITMAPINFOHEADER);
    header->biWidth = bitmap_info.bmWidth;
    header->biHeight = bitmap_info.bmHeight;
    header->biPlanes = 1;
    header->biBitCount = 32;
    header->biCompression = BI_RGB;
    header->biSizeImage = (DWORD)pixel_bytes;
    HDC dc = GetDC(nullptr);
    int copied = GetDIBits(dc, bitmap, 0, (UINT)bitmap_info.bmHeight,
                           payload->bytes.data() + sizeof(BITMAPINFOHEADER),
                           reinterpret_cast<BITMAPINFO *>(header), DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    if (copied == 0)
    {
        delete payload;
        return REACH_ERROR;
    }
    HBITMAP thumbnail = reach_clipboard_scale_bitmap(
        bitmap, (uint32_t)bitmap_info.bmWidth, (uint32_t)bitmap_info.bmHeight);
    if (thumbnail == nullptr)
    {
        delete payload;
        return REACH_ERROR;
    }
    payload->id = ++provider->next_id;
    payload->kind = REACH_CLIPBOARD_ITEM_IMAGE;
    payload->format = CF_DIB;
    payload->thumbnail_id = reach_windows_icon_id_from_hbitmap(thumbnail);
    if (payload->thumbnail_id == 0)
    {
        delete payload;
        return REACH_ERROR;
    }
    try
    {
        provider->payloads.push_back(payload);
    }
    catch (...)
    {
        reach_windows_icon_id_release(payload->thumbnail_id);
        delete payload;
        return REACH_ERROR;
    }
    out_item->id = payload->id;
    out_item->kind = payload->kind;
    out_item->content_hash =
        reach_clipboard_hash_bytes(payload->bytes.data(), payload->bytes.size());
    out_item->thumbnail_id = payload->thumbnail_id;
    out_item->image_width = (uint32_t)bitmap_info.bmWidth;
    out_item->image_height = (uint32_t)bitmap_info.bmHeight;
    _snwprintf_s(reinterpret_cast<wchar_t *>(out_item->preview),
                 REACH_CLIPBOARD_PREVIEW_CAPACITY, _TRUNCATE, L"%u \u00D7 %u image",
                 out_item->image_width, out_item->image_height);
    return REACH_OK;
}


static int32_t reach_clipboard_is_image_file_path(const wchar_t *path)
{
    if (path == nullptr || path[0] == 0)
    {
        return 0;
    }

    const wchar_t *extension = wcsrchr(path, L'.');
    if (extension == nullptr)
    {
        return 0;
    }

    return lstrcmpiW(extension, L".png") == 0 ||
           lstrcmpiW(extension, L".jpg") == 0 ||
           lstrcmpiW(extension, L".jpeg") == 0 ||
           lstrcmpiW(extension, L".jfif") == 0 ||
           lstrcmpiW(extension, L".bmp") == 0 ||
           lstrcmpiW(extension, L".gif") == 0 ||
           lstrcmpiW(extension, L".tif") == 0 ||
           lstrcmpiW(extension, L".tiff") == 0 ||
           lstrcmpiW(extension, L".webp") == 0 ||
           lstrcmpiW(extension, L".ico") == 0 ||
           lstrcmpiW(extension, L".heic") == 0 ||
           lstrcmpiW(extension, L".heif") == 0;
}

static const wchar_t *reach_clipboard_file_name_from_path(const wchar_t *path)
{
    if (path == nullptr)
    {
        return L"";
    }

    const wchar_t *slash = wcsrchr(path, L'\\');
    const wchar_t *forward_slash = wcsrchr(path, L'/');
    const wchar_t *separator = slash > forward_slash ? slash : forward_slash;
    return separator != nullptr ? separator + 1 : path;
}

static void reach_clipboard_force_hbitmap_opaque_alpha(HBITMAP bitmap)
{
    if (bitmap == nullptr)
    {
        return;
    }

    BITMAP bitmap_info = {};
    if (GetObject(bitmap, sizeof(bitmap_info), &bitmap_info) != sizeof(bitmap_info))
    {
        return;
    }

    if (bitmap_info.bmBits == nullptr || bitmap_info.bmBitsPixel != 32 ||
        bitmap_info.bmWidth <= 0 || bitmap_info.bmHeight == 0)
    {
        return;
    }

    const size_t width = (size_t)bitmap_info.bmWidth;
    const size_t height =
        (size_t)(bitmap_info.bmHeight < 0 ? -bitmap_info.bmHeight : bitmap_info.bmHeight);
    uint8_t *pixels = static_cast<uint8_t *>(bitmap_info.bmBits);

    for (size_t y = 0; y < height; ++y)
    {
        uint8_t *row = pixels + y * (size_t)bitmap_info.bmWidthBytes;
        for (size_t x = 0; x < width; ++x)
        {
            row[x * 4u + 3u] = 0xFF;
        }
    }
}

static HBITMAP reach_clipboard_thumbnail_from_file_path(const wchar_t *path, uint32_t *out_width,
                                                        uint32_t *out_height)
{
    if (out_width != nullptr)
    {
        *out_width = 0;
    }
    if (out_height != nullptr)
    {
        *out_height = 0;
    }

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

    SIZE size = {};
    size.cx = 256;
    size.cy = 256;

    HBITMAP bitmap = nullptr;
    hr = factory->GetImage(size, SIIGBF_BIGGERSIZEOK, &bitmap);
    factory->Release();

    if (FAILED(hr) || bitmap == nullptr)
    {
        return nullptr;
    }

    BITMAP bitmap_info = {};
    if (GetObject(bitmap, sizeof(bitmap_info), &bitmap_info) == sizeof(bitmap_info))
    {
        if (out_width != nullptr && bitmap_info.bmWidth > 0)
        {
            *out_width = (uint32_t)bitmap_info.bmWidth;
        }
        if (out_height != nullptr && bitmap_info.bmHeight != 0)
        {
            *out_height =
                (uint32_t)(bitmap_info.bmHeight < 0 ? -bitmap_info.bmHeight : bitmap_info.bmHeight);
        }
    }

    reach_clipboard_force_hbitmap_opaque_alpha(bitmap);
    return bitmap;
}

static reach_result reach_clipboard_capture_file_drop(reach_clipboard_provider *provider,
                                                      reach_clipboard_item *out_item)
{
    if (provider == nullptr || out_item == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    HGLOBAL handle = static_cast<HGLOBAL>(GetClipboardData(CF_HDROP));
    if (handle == nullptr)
    {
        return REACH_ERROR;
    }

    HDROP drop = static_cast<HDROP>(handle);
    const UINT file_count = DragQueryFileW(drop, 0xFFFFFFFFu, nullptr, 0);
    if (file_count == 0)
    {
        return REACH_ERROR;
    }

    std::vector<wchar_t> selected_path;
    for (UINT file_index = 0; file_index < file_count; ++file_index)
    {
        const UINT path_length = DragQueryFileW(drop, file_index, nullptr, 0);
        if (path_length == 0)
        {
            continue;
        }

        std::vector<wchar_t> candidate;
        try
        {
            candidate.resize((size_t)path_length + 1u);
        }
        catch (...)
        {
            return REACH_ERROR;
        }

        if (DragQueryFileW(drop, file_index, candidate.data(), path_length + 1u) == 0)
        {
            continue;
        }

        if (reach_clipboard_is_image_file_path(candidate.data()))
        {
            selected_path.swap(candidate);
            break;
        }
    }

    if (selected_path.empty())
    {
        return REACH_ERROR;
    }

    uint32_t thumbnail_width = 0;
    uint32_t thumbnail_height = 0;
    HBITMAP thumbnail = reach_clipboard_thumbnail_from_file_path(selected_path.data(),
                                                                 &thumbnail_width,
                                                                 &thumbnail_height);
    if (thumbnail == nullptr)
    {
        return REACH_ERROR;
    }

    void *source = GlobalLock(handle);
    if (source == nullptr)
    {
        DeleteObject(thumbnail);
        return REACH_ERROR;
    }

    const SIZE_T byte_count = GlobalSize(handle);
    if (byte_count == 0)
    {
        GlobalUnlock(handle);
        DeleteObject(thumbnail);
        return REACH_ERROR;
    }

    reach_clipboard_payload *payload = new (std::nothrow) reach_clipboard_payload();
    if (payload == nullptr)
    {
        GlobalUnlock(handle);
        DeleteObject(thumbnail);
        return REACH_ERROR;
    }

    try
    {
        payload->bytes.resize((size_t)byte_count);
    }
    catch (...)
    {
        GlobalUnlock(handle);
        DeleteObject(thumbnail);
        delete payload;
        return REACH_ERROR;
    }

    memcpy(payload->bytes.data(), source, (size_t)byte_count);
    GlobalUnlock(handle);

    payload->id = ++provider->next_id;
    payload->kind = REACH_CLIPBOARD_ITEM_IMAGE;
    payload->format = CF_HDROP;
    payload->thumbnail_id = reach_windows_icon_id_from_hbitmap(thumbnail);
    if (payload->thumbnail_id == 0)
    {
        delete payload;
        return REACH_ERROR;
    }

    try
    {
        provider->payloads.push_back(payload);
    }
    catch (...)
    {
        reach_windows_icon_id_release(payload->thumbnail_id);
        delete payload;
        return REACH_ERROR;
    }

    out_item->id = payload->id;
    out_item->kind = payload->kind;
    out_item->content_hash =
        reach_clipboard_hash_bytes(payload->bytes.data(), payload->bytes.size());
    out_item->thumbnail_id = payload->thumbnail_id;
    out_item->image_width = thumbnail_width;
    out_item->image_height = thumbnail_height;

    const wchar_t *file_name = reach_clipboard_file_name_from_path(selected_path.data());
    _snwprintf_s(reinterpret_cast<wchar_t *>(out_item->preview),
                 REACH_CLIPBOARD_PREVIEW_CAPACITY, _TRUNCATE, L"%s", file_name);

    return REACH_OK;
}
static reach_result reach_clipboard_capture_current(reach_clipboard_provider *provider,
                                                    reach_clipboard_item *out_item)
{
    if (provider == nullptr || out_item == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_item = {};
    if (!OpenClipboard(provider->window))
    {
        return REACH_ERROR;
    }
    reach_result result = REACH_ERROR;
    if (IsClipboardFormatAvailable(CF_DIBV5))
    {
        result = reach_clipboard_capture_dib(provider, CF_DIBV5, out_item);
    }
    else if (IsClipboardFormatAvailable(CF_DIB))
    {
        result = reach_clipboard_capture_dib(provider, CF_DIB, out_item);
    }
    else if (IsClipboardFormatAvailable(CF_BITMAP))
    {
        result = reach_clipboard_capture_bitmap(provider, out_item);
    }
    else if (IsClipboardFormatAvailable(CF_HDROP))
    {
        result = reach_clipboard_capture_file_drop(provider, out_item);
    }
    else if (IsClipboardFormatAvailable(CF_UNICODETEXT))
    {
        result = reach_clipboard_capture_text(provider, out_item);
    }
    CloseClipboard();
    return result;
}

static reach_result reach_clipboard_restore(reach_clipboard_provider *provider, uint64_t item_id)
{
    reach_clipboard_payload *payload = reach_clipboard_find_payload(provider, item_id);
    if (payload == nullptr || !OpenClipboard(provider->window))
    {
        return REACH_ERROR;
    }
    reach_result result = REACH_ERROR;
    if (EmptyClipboard())
    {
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, payload->bytes.size());
        void *destination = memory != nullptr ? GlobalLock(memory) : nullptr;
        if (destination != nullptr)
        {
            memcpy(destination, payload->bytes.data(), payload->bytes.size());
            GlobalUnlock(memory);
            if (SetClipboardData(payload->format, memory) != nullptr)
            {
                result = REACH_OK;
                memory = nullptr;
            }
        }
        if (memory != nullptr)
        {
            GlobalFree(memory);
        }
    }
    CloseClipboard();
    if (result == REACH_OK)
    {
        provider->suppressed_sequence = GetClipboardSequenceNumber();
    }
    return result;
}

static void reach_clipboard_release(reach_clipboard_provider *provider, uint64_t item_id)
{
    if (provider == nullptr || item_id == 0)
    {
        return;
    }
    for (size_t index = 0; index < provider->payloads.size(); ++index)
    {
        reach_clipboard_payload *payload = provider->payloads[index];
        if (payload != nullptr && payload->id == item_id)
        {
            reach_windows_icon_id_release(payload->thumbnail_id);
            delete payload;
            provider->payloads.erase(provider->payloads.begin() + index);
            return;
        }
    }
}

static LRESULT CALLBACK reach_clipboard_window_proc(HWND window, UINT message, WPARAM wparam,
                                                    LPARAM lparam)
{
    if (message == WM_NCCREATE)
    {
        CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lparam);
        SetWindowLongPtrW(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return TRUE;
    }
    reach_clipboard_provider *provider = reinterpret_cast<reach_clipboard_provider *>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_CLIPBOARDUPDATE && provider != nullptr)
    {
        DWORD sequence = GetClipboardSequenceNumber();
        if (provider->suppressed_sequence != 0 && sequence == provider->suppressed_sequence)
        {
            provider->suppressed_sequence = 0;
            return 0;
        }
        if (provider->callback != nullptr)
        {
            provider->callback(provider->callback_user);
        }
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static reach_result reach_clipboard_start(reach_clipboard_provider *provider,
                                          reach_clipboard_changed_callback callback, void *user)
{
    if (provider == nullptr || callback == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    provider->callback = callback;
    provider->callback_user = user;
    if (provider->window == nullptr)
    {
        WNDCLASSW window_class = {};
        window_class.lpfnWndProc = reach_clipboard_window_proc;
        window_class.hInstance = GetModuleHandleW(nullptr);
        window_class.lpszClassName = REACH_CLIPBOARD_WINDOW_CLASS;
        provider->window_class = RegisterClassW(&window_class);
        if (provider->window_class == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            return REACH_ERROR;
        }
        provider->window =
            CreateWindowExW(0, REACH_CLIPBOARD_WINDOW_CLASS, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                            nullptr, GetModuleHandleW(nullptr), provider);
    }
    return provider->window != nullptr && AddClipboardFormatListener(provider->window) ? REACH_OK
                                                                                       : REACH_ERROR;
}

static reach_result reach_clipboard_stop(reach_clipboard_provider *provider)
{
    if (provider == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (provider->window != nullptr)
    {
        RemoveClipboardFormatListener(provider->window);
    }
    provider->callback = nullptr;
    provider->callback_user = nullptr;
    return REACH_OK;
}

static void reach_clipboard_destroy(reach_clipboard_provider *provider)
{
    if (provider == nullptr)
    {
        return;
    }
    reach_clipboard_stop(provider);
    while (!provider->payloads.empty())
    {
        reach_clipboard_release(provider, provider->payloads.back()->id);
    }
    if (provider->window != nullptr)
    {
        DestroyWindow(provider->window);
    }
    delete provider;
}

extern "C" reach_result reach_windows_create_clipboard_provider(reach_clipboard_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_port = {};
    reach_clipboard_provider *provider = new (std::nothrow) reach_clipboard_provider();
    if (provider == nullptr)
    {
        return REACH_ERROR;
    }
    provider->next_id = 0;
    out_port->provider = provider;
    out_port->ops.start = reach_clipboard_start;
    out_port->ops.stop = reach_clipboard_stop;
    out_port->ops.capture_current = reach_clipboard_capture_current;
    out_port->ops.restore = reach_clipboard_restore;
    out_port->ops.release = reach_clipboard_release;
    out_port->ops.destroy = reach_clipboard_destroy;
    return REACH_OK;
}