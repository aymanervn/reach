#include "windows_adapters_internal.h"

#include <windows.h>
#include <wincodec.h>

#include <math.h>
#include <new>
#include <stdint.h>
#include <vector>

struct reach_wallpaper_window {
    HWND hwnd;
    HDC memory_dc;
    HBITMAP bitmap;
    HGDIOBJ old_bitmap;
    uint16_t bitmap_path[260];
    reach_rect_f32 last_bounds;
    int32_t bounds_valid;
    int bitmap_width;
    int bitmap_height;
};

struct reach_wallpaper_surface {
    IWICImagingFactory *wic_factory;
    uint16_t path[260];
    uint16_t monitor_paths[REACH_MAX_WALLPAPER_MONITORS][260];
    std::vector<reach_wallpaper_window *> windows;
    int32_t visible;
};

template <typename T>
static void reach_wallpaper_release(T **object)
{
    if (object != nullptr && *object != nullptr) {
        (*object)->Release();
        *object = nullptr;
    }
}

static const wchar_t *reach_wallpaper_window_class_name()
{
    return L"ReachWallpaperWindow";
}

static int reach_wallpaper_width(reach_rect_f32 bounds)
{
    int width = (int)bounds.width;
    return width > 0 ? width : 1;
}

static int reach_wallpaper_height(reach_rect_f32 bounds)
{
    int height = (int)bounds.height;
    return height > 0 ? height : 1;
}

static void reach_wallpaper_paint_black(HDC dc, HWND hwnd)
{
    if (dc == nullptr || hwnd == nullptr) {
        return;
    }

    RECT rect = {};
    GetClientRect(hwnd, &rect);

    HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
    if (brush != nullptr) {
        FillRect(dc, &rect, brush);
        DeleteObject(brush);
    }
}

static LRESULT CALLBACK reach_wallpaper_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    (void)wparam;
    (void)lparam;

    reach_wallpaper_window *window =
        reinterpret_cast<reach_wallpaper_window *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_ERASEBKGND) {
        return 1;
    }

    if (message == WM_NCHITTEST) {
        return HTTRANSPARENT;
    }

    if (message == WM_PAINT) {
        PAINTSTRUCT paint = {};
        HDC dc = BeginPaint(hwnd, &paint);
        if (dc != nullptr &&
            window != nullptr &&
            window->memory_dc != nullptr &&
            window->bitmap != nullptr &&
            window->bitmap_width > 0 &&
            window->bitmap_height > 0) {
            RECT client = {};
            GetClientRect(hwnd, &client);
            int width = client.right - client.left;
            int height = client.bottom - client.top;
            if (width > 0 && height > 0) {
                if (width == window->bitmap_width && height == window->bitmap_height) {
                    BitBlt(dc, 0, 0, width, height, window->memory_dc, 0, 0, SRCCOPY);
                } else {
                    SetStretchBltMode(dc, HALFTONE);
                    StretchBlt(
                        dc,
                        0,
                        0,
                        width,
                        height,
                        window->memory_dc,
                        0,
                        0,
                        window->bitmap_width,
                        window->bitmap_height,
                        SRCCOPY);
                }
            }
        } else if (dc != nullptr) {
            reach_wallpaper_paint_black(dc, hwnd);
        }

        EndPaint(hwnd, &paint);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static reach_result reach_register_wallpaper_window_class()
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = reach_wallpaper_window_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = reach_wallpaper_window_class_name();

    ATOM atom = RegisterClassExW(&wc);
    if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return REACH_ERROR;
    }

    return REACH_OK;
}

static void reach_wallpaper_unload_bitmap(reach_wallpaper_window *window)
{
    if (window == nullptr) {
        return;
    }

    if (window->memory_dc != nullptr && window->old_bitmap != nullptr) {
        SelectObject(window->memory_dc, window->old_bitmap);
        window->old_bitmap = nullptr;
    }

    if (window->bitmap != nullptr) {
        DeleteObject(window->bitmap);
        window->bitmap = nullptr;
    }

    if (window->memory_dc != nullptr) {
        DeleteDC(window->memory_dc);
        window->memory_dc = nullptr;
    }

    window->bitmap_width = 0;
    window->bitmap_height = 0;
    window->bitmap_path[0] = 0;
}

static int32_t reach_wallpaper_path_equal(const uint16_t *a, const uint16_t *b)
{
    if (a == nullptr && b == nullptr) {
        return 1;
    }
    if (a == nullptr || b == nullptr) {
        return 0;
    }
    return lstrcmpW(reinterpret_cast<const wchar_t *>(a), reinterpret_cast<const wchar_t *>(b)) == 0;
}

static reach_result reach_wallpaper_client_size(reach_wallpaper_window *window, int *out_width, int *out_height)
{
    if (window == nullptr || window->hwnd == nullptr || out_width == nullptr || out_height == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    RECT client = {};
    GetClientRect(window->hwnd, &client);

    int width = client.right - client.left;
    int height = client.bottom - client.top;
    if (width <= 0) {
        width = 1;
    }
    if (height <= 0) {
        height = 1;
    }

    *out_width = width;
    *out_height = height;
    return REACH_OK;
}

static reach_result reach_wallpaper_create_dib(
    reach_wallpaper_window *window,
    int width,
    int height,
    void **out_pixels,
    UINT *out_stride,
    UINT *out_buffer_size
)
{
    if (window == nullptr || width <= 0 || height <= 0 ||
        out_pixels == nullptr || out_stride == nullptr || out_buffer_size == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_wallpaper_unload_bitmap(window);

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void *pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (bitmap == nullptr || pixels == nullptr) {
        if (bitmap != nullptr) {
            DeleteObject(bitmap);
        }
        return REACH_ERROR;
    }

    HDC memory_dc = CreateCompatibleDC(nullptr);
    if (memory_dc == nullptr) {
        DeleteObject(bitmap);
        return REACH_ERROR;
    }

    HGDIOBJ old_bitmap = SelectObject(memory_dc, bitmap);
    if (old_bitmap == nullptr || old_bitmap == HGDI_ERROR) {
        DeleteDC(memory_dc);
        DeleteObject(bitmap);
        return REACH_ERROR;
    }

    UINT stride = (UINT)width * 4u;
    if ((UINT)height > UINT_MAX / stride) {
        SelectObject(memory_dc, old_bitmap);
        DeleteDC(memory_dc);
        DeleteObject(bitmap);
        return REACH_ERROR;
    }

    window->memory_dc = memory_dc;
    window->bitmap = bitmap;
    window->old_bitmap = old_bitmap;
    window->bitmap_width = width;
    window->bitmap_height = height;

    *out_pixels = pixels;
    *out_stride = stride;
    *out_buffer_size = stride * (UINT)height;
    return REACH_OK;
}

static reach_result reach_wallpaper_load_bitmap(
    reach_wallpaper_surface *surface,
    reach_wallpaper_window *window,
    const uint16_t *path
)
{
    if (surface == nullptr || window == nullptr || surface->wic_factory == nullptr ||
        path == nullptr || path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    int target_width = 0;
    int target_height = 0;
    reach_result size_result = reach_wallpaper_client_size(window, &target_width, &target_height);
    if (size_result != REACH_OK) {
        return size_result;
    }

    if (window->bitmap != nullptr &&
        window->bitmap_width == target_width &&
        window->bitmap_height == target_height &&
        reach_wallpaper_path_equal(window->bitmap_path, path)) {
        return REACH_OK;
    }

    IWICBitmapDecoder *decoder = nullptr;
    IWICBitmapFrameDecode *frame = nullptr;
    IWICBitmapClipper *clipper = nullptr;
    IWICBitmapScaler *scaler = nullptr;
    IWICFormatConverter *converter = nullptr;

    HRESULT hr = surface->wic_factory->CreateDecoderFromFilename(
        reinterpret_cast<const wchar_t *>(path),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder);

    if (SUCCEEDED(hr)) {
        hr = decoder->GetFrame(0, &frame);
    }

    UINT source_width = 0;
    UINT source_height = 0;
    if (SUCCEEDED(hr)) {
        hr = frame->GetSize(&source_width, &source_height);
    }

    WICRect crop = {};
    if (SUCCEEDED(hr)) {
        if (source_width == 0 || source_height == 0) {
            hr = E_FAIL;
        } else {
            double target_aspect = (double)target_width / (double)target_height;
            double source_aspect = (double)source_width / (double)source_height;

            crop.X = 0;
            crop.Y = 0;
            crop.Width = (INT)source_width;
            crop.Height = (INT)source_height;

            if (source_aspect > target_aspect) {
                UINT crop_width = (UINT)((double)source_height * target_aspect);
                if (crop_width == 0) {
                    crop_width = source_width;
                }
                crop.X = (INT)((source_width - crop_width) / 2u);
                crop.Width = (INT)crop_width;
            } else if (source_aspect < target_aspect) {
                UINT crop_height = (UINT)((double)source_width / target_aspect);
                if (crop_height == 0) {
                    crop_height = source_height;
                }
                crop.Y = (INT)((source_height - crop_height) / 2u);
                crop.Height = (INT)crop_height;
            }
        }
    }

    if (SUCCEEDED(hr)) {
        hr = surface->wic_factory->CreateBitmapClipper(&clipper);
    }
    if (SUCCEEDED(hr)) {
        hr = clipper->Initialize(frame, &crop);
    }
    if (SUCCEEDED(hr)) {
        hr = surface->wic_factory->CreateBitmapScaler(&scaler);
    }
    if (SUCCEEDED(hr)) {
        hr = scaler->Initialize(
            clipper,
            (UINT)target_width,
            (UINT)target_height,
            WICBitmapInterpolationModeFant);
    }
    if (SUCCEEDED(hr)) {
        hr = surface->wic_factory->CreateFormatConverter(&converter);
    }
    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(
            scaler,
            GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeMedianCut);
    }

    void *pixels = nullptr;
    UINT stride = 0;
    UINT buffer_size = 0;
    if (SUCCEEDED(hr)) {
        reach_result dib_result = reach_wallpaper_create_dib(
            window,
            target_width,
            target_height,
            &pixels,
            &stride,
            &buffer_size);
        hr = dib_result == REACH_OK ? S_OK : E_FAIL;
    }

    if (SUCCEEDED(hr)) {
        hr = converter->CopyPixels(nullptr, stride, buffer_size, static_cast<BYTE *>(pixels));
    }

    reach_wallpaper_release(&converter);
    reach_wallpaper_release(&scaler);
    reach_wallpaper_release(&clipper);
    reach_wallpaper_release(&frame);
    reach_wallpaper_release(&decoder);

    if (FAILED(hr)) {
        reach_wallpaper_unload_bitmap(window);
        return REACH_ERROR;
    }

    return reach_copy_utf16(window->bitmap_path, 260, path);
}

static const uint16_t *reach_wallpaper_path_for_monitor(reach_wallpaper_surface *surface, size_t monitor_index)
{
    if (surface == nullptr) {
        return nullptr;
    }

    if (monitor_index < REACH_MAX_WALLPAPER_MONITORS && surface->monitor_paths[monitor_index][0] != 0) {
        return surface->monitor_paths[monitor_index];
    }

    return surface->path[0] != 0 ? surface->path : nullptr;
}

static reach_result reach_wallpaper_render_window(
    reach_wallpaper_surface *surface,
    reach_wallpaper_window *window,
    size_t monitor_index
)
{
    if (surface == nullptr || window == nullptr || window->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    const uint16_t *path = reach_wallpaper_path_for_monitor(surface, monitor_index);
    if (path != nullptr && path[0] != 0) {
        reach_result result = reach_wallpaper_load_bitmap(surface, window, path);
        if (result != REACH_OK) {
            reach_wallpaper_unload_bitmap(window);
        }
    } else {
        reach_wallpaper_unload_bitmap(window);
    }

    InvalidateRect(window->hwnd, nullptr, FALSE);
    UpdateWindow(window->hwnd);
    return REACH_OK;
}

static reach_result reach_wallpaper_render_all(reach_wallpaper_surface *surface)
{
    if (surface == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = REACH_OK;
    for (size_t index = 0; index < surface->windows.size(); ++index) {
        reach_result window_result = reach_wallpaper_render_window(surface, surface->windows[index], index);
        if (result == REACH_OK && window_result != REACH_OK) {
            result = window_result;
        }
    }

    return result;
}

static void reach_wallpaper_window_destroy(reach_wallpaper_window *window)
{
    if (window == nullptr) {
        return;
    }

    reach_wallpaper_unload_bitmap(window);

    if (window->hwnd != nullptr) {
        SetWindowLongPtrW(window->hwnd, GWLP_USERDATA, 0);
        DestroyWindow(window->hwnd);
        window->hwnd = nullptr;
    }

    delete window;
}

static int32_t reach_wallpaper_bounds_equal(reach_rect_f32 a, reach_rect_f32 b)
{
    return fabsf(a.x - b.x) < 0.5f &&
        fabsf(a.y - b.y) < 0.5f &&
        fabsf(a.width - b.width) < 0.5f &&
        fabsf(a.height - b.height) < 0.5f;
}

struct reach_wallpaper_monitor_enum {
    std::vector<reach_rect_f32> bounds;
};

static BOOL CALLBACK reach_wallpaper_monitor_enum_proc(HMONITOR monitor, HDC dc, LPRECT rect, LPARAM param)
{
    (void)monitor;
    (void)dc;

    reach_wallpaper_monitor_enum *state = reinterpret_cast<reach_wallpaper_monitor_enum *>(param);
    if (state == nullptr || rect == nullptr) {
        return TRUE;
    }

    reach_rect_f32 bounds = {};
    bounds.x = (float)rect->left;
    bounds.y = (float)rect->top;
    bounds.width = (float)(rect->right - rect->left);
    bounds.height = (float)(rect->bottom - rect->top);

    if (bounds.width > 0.0f && bounds.height > 0.0f) {
        state->bounds.push_back(bounds);
    }

    return TRUE;
}

static reach_result reach_wallpaper_collect_monitor_bounds(
    reach_wallpaper_monitor_enum *out_state,
    reach_rect_f32 fallback
)
{
    if (out_state == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    out_state->bounds.clear();
    EnumDisplayMonitors(nullptr, nullptr, reach_wallpaper_monitor_enum_proc, reinterpret_cast<LPARAM>(out_state));

    if (out_state->bounds.empty() && fallback.width > 0.0f && fallback.height > 0.0f) {
        out_state->bounds.push_back(fallback);
    }

    if (out_state->bounds.empty()) {
        reach_rect_f32 primary = {};
        primary.x = 0.0f;
        primary.y = 0.0f;
        primary.width = (float)GetSystemMetrics(SM_CXSCREEN);
        primary.height = (float)GetSystemMetrics(SM_CYSCREEN);
        out_state->bounds.push_back(primary);
    }

    return REACH_OK;
}

static reach_wallpaper_window *reach_wallpaper_create_window(
    reach_wallpaper_surface *surface,
    reach_rect_f32 bounds
)
{
    if (surface == nullptr) {
        return nullptr;
    }

    reach_wallpaper_window *window = new (std::nothrow) reach_wallpaper_window();
    if (window == nullptr) {
        return nullptr;
    }

    window->hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        reach_wallpaper_window_class_name(),
        L"Reach Wallpaper",
        WS_POPUP,
        (int)bounds.x,
        (int)bounds.y,
        reach_wallpaper_width(bounds),
        reach_wallpaper_height(bounds),
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    if (window->hwnd == nullptr) {
        delete window;
        return nullptr;
    }

    SetWindowLongPtrW(window->hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));

    window->last_bounds = bounds;
    window->bounds_valid = 1;

    if (surface->visible) {
        ShowWindow(window->hwnd, SW_SHOWNOACTIVATE);
        SetWindowPos(window->hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
    }

    return window;
}

static reach_result reach_wallpaper_apply_window_bounds(
    reach_wallpaper_surface *surface,
    reach_wallpaper_window *window,
    reach_rect_f32 bounds,
    int32_t *out_changed
)
{
    (void)surface;

    if (window == nullptr || window->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    if (out_changed != nullptr) {
        *out_changed = 0;
    }

    if (window->bounds_valid && reach_wallpaper_bounds_equal(window->last_bounds, bounds)) {
        return REACH_OK;
    }

    BOOL ok = SetWindowPos(
        window->hwnd,
        HWND_BOTTOM,
        (int)bounds.x,
        (int)bounds.y,
        reach_wallpaper_width(bounds),
        reach_wallpaper_height(bounds),
        SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    if (!ok) {
        return REACH_ERROR;
    }

    window->last_bounds = bounds;
    window->bounds_valid = 1;
    reach_wallpaper_unload_bitmap(window);

    if (out_changed != nullptr) {
        *out_changed = 1;
    }

    return REACH_OK;
}

static reach_result reach_wallpaper_sync_monitors(
    reach_wallpaper_surface *surface,
    reach_rect_f32 fallback_bounds
)
{
    if (surface == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_wallpaper_monitor_enum monitors = {};
    reach_result result = reach_wallpaper_collect_monitor_bounds(&monitors, fallback_bounds);
    if (result != REACH_OK) {
        return result;
    }

    while (surface->windows.size() > monitors.bounds.size()) {
        reach_wallpaper_window_destroy(surface->windows.back());
        surface->windows.pop_back();
    }

    int32_t needs_render = 0;
    while (surface->windows.size() < monitors.bounds.size()) {
        reach_wallpaper_window *window = reach_wallpaper_create_window(
            surface,
            monitors.bounds[surface->windows.size()]);
        if (window == nullptr) {
            return REACH_ERROR;
        }

        surface->windows.push_back(window);
        needs_render = 1;
    }

    for (size_t index = 0; index < surface->windows.size(); ++index) {
        int32_t changed = 0;
        result = reach_wallpaper_apply_window_bounds(surface, surface->windows[index], monitors.bounds[index], &changed);
        if (result != REACH_OK) {
            return result;
        }

        if (changed) {
            needs_render = 1;
        }

        if (surface->visible) {
            ShowWindow(surface->windows[index]->hwnd, SW_SHOWNOACTIVATE);
            SetWindowPos(
                surface->windows[index]->hwnd,
                HWND_BOTTOM,
                0,
                0,
                0,
                0,
                SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
        }
    }

    return needs_render ? reach_wallpaper_render_all(surface) : REACH_OK;
}

static reach_result reach_wallpaper_surface_show(reach_wallpaper_surface *surface)
{
    if (surface == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    surface->visible = 1;

    reach_rect_f32 fallback = {};
    reach_result result = reach_wallpaper_sync_monitors(surface, fallback);
    if (result != REACH_OK) {
        return result;
    }

    for (reach_wallpaper_window *window : surface->windows) {
        if (window != nullptr && window->hwnd != nullptr) {
            ShowWindow(window->hwnd, SW_SHOWNOACTIVATE);
            SetWindowPos(window->hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
            InvalidateRect(window->hwnd, nullptr, FALSE);
        }
    }

    return REACH_OK;
}

static reach_result reach_wallpaper_surface_hide(reach_wallpaper_surface *surface)
{
    if (surface == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    for (reach_wallpaper_window *window : surface->windows) {
        if (window != nullptr && window->hwnd != nullptr) {
            ShowWindow(window->hwnd, SW_HIDE);
        }
    }

    surface->visible = 0;
    return REACH_OK;
}

static reach_result reach_wallpaper_surface_set_bounds(reach_wallpaper_surface *surface, reach_rect_f32 bounds)
{
    return reach_wallpaper_sync_monitors(surface, bounds);
}

static reach_result reach_wallpaper_surface_set_wallpaper(
    reach_wallpaper_surface *surface,
    const uint16_t *path
)
{
    if (surface == nullptr || path == nullptr || path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = reach_copy_utf16(surface->path, 260, path);
    if (result != REACH_OK) {
        return result;
    }

    for (reach_wallpaper_window *window : surface->windows) {
        reach_wallpaper_unload_bitmap(window);
    }

    return reach_wallpaper_render_all(surface);
}

static reach_result reach_wallpaper_surface_set_monitor_wallpaper(
    reach_wallpaper_surface *surface,
    size_t monitor_index,
    const uint16_t *path
)
{
    if (surface == nullptr || path == nullptr || path[0] == 0 || monitor_index >= REACH_MAX_WALLPAPER_MONITORS) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = reach_copy_utf16(surface->monitor_paths[monitor_index], 260, path);
    if (result != REACH_OK) {
        return result;
    }

    if (monitor_index < surface->windows.size()) {
        reach_wallpaper_unload_bitmap(surface->windows[monitor_index]);
        return reach_wallpaper_render_window(surface, surface->windows[monitor_index], monitor_index);
    }

    return REACH_OK;
}

static reach_result reach_wallpaper_surface_clear_monitor_wallpaper(
    reach_wallpaper_surface *surface,
    size_t monitor_index
)
{
    if (surface == nullptr || monitor_index >= REACH_MAX_WALLPAPER_MONITORS) {
        return REACH_INVALID_ARGUMENT;
    }

    surface->monitor_paths[monitor_index][0] = 0;

    if (monitor_index < surface->windows.size()) {
        reach_wallpaper_unload_bitmap(surface->windows[monitor_index]);
        return reach_wallpaper_render_window(surface, surface->windows[monitor_index], monitor_index);
    }

    return REACH_OK;
}

static reach_result reach_wallpaper_surface_clear(reach_wallpaper_surface *surface)
{
    if (surface == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    for (reach_wallpaper_window *window : surface->windows) {
        reach_wallpaper_unload_bitmap(window);
    }

    surface->path[0] = 0;
    for (size_t index = 0; index < REACH_MAX_WALLPAPER_MONITORS; ++index) {
        surface->monitor_paths[index][0] = 0;
    }

    return reach_wallpaper_render_all(surface);
}

static void reach_wallpaper_surface_destroy(reach_wallpaper_surface *surface)
{
    if (surface == nullptr) {
        return;
    }

    for (reach_wallpaper_window *window : surface->windows) {
        reach_wallpaper_window_destroy(window);
    }

    surface->windows.clear();

    if (surface->wic_factory != nullptr) {
        surface->wic_factory->Release();
        surface->wic_factory = nullptr;
    }

    delete surface;
}

reach_result reach_windows_create_wallpaper_surface(reach_wallpaper_surface_port *out_port)
{
    if (out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};

    reach_result result = reach_register_wallpaper_window_class();
    if (result != REACH_OK) {
        return result;
    }

    reach_wallpaper_surface *surface = new (std::nothrow) reach_wallpaper_surface();
    if (surface == nullptr) {
        return REACH_ERROR;
    }

    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&surface->wic_factory));
    if (FAILED(hr)) {
        reach_wallpaper_surface_destroy(surface);
        return REACH_ERROR;
    }

    out_port->surface = surface;
    out_port->ops.show = reach_wallpaper_surface_show;
    out_port->ops.hide = reach_wallpaper_surface_hide;
    out_port->ops.set_bounds = reach_wallpaper_surface_set_bounds;
    out_port->ops.set_wallpaper = reach_wallpaper_surface_set_wallpaper;
    out_port->ops.set_monitor_wallpaper = reach_wallpaper_surface_set_monitor_wallpaper;
    out_port->ops.clear_monitor_wallpaper = reach_wallpaper_surface_clear_monitor_wallpaper;
    out_port->ops.clear = reach_wallpaper_surface_clear;
    out_port->ops.destroy = reach_wallpaper_surface_destroy;
    return REACH_OK;
}
