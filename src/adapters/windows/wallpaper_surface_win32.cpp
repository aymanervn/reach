#include "reach/platform/windows_adapters.h"

#include <windows.h>
#include <d2d1_1.h>
#include <wincodec.h>

#include <math.h>
#include <new>
#include <vector>

struct reach_wallpaper_window {
    HWND hwnd;
    ID2D1HwndRenderTarget *target;
    ID2D1Bitmap *bitmap;
    uint16_t bitmap_path[260];
    reach_rect_f32 last_bounds;
    int32_t bounds_valid;
    UINT target_width;
    UINT target_height;
};

struct reach_wallpaper_surface {
    ID2D1Factory1 *factory;
    IWICImagingFactory *wic_factory;
    uint16_t path[260];
    uint16_t monitor_paths[REACH_MAX_WALLPAPER_MONITORS][260];
    std::vector<reach_wallpaper_window *> windows;
    int32_t visible;
};

static const wchar_t *reach_wallpaper_window_class_name()
{
    return L"ReachWallpaperWindow";
}

static LRESULT CALLBACK reach_wallpaper_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    (void)hwnd;
    (void)wparam;
    (void)lparam;
    if (message == WM_ERASEBKGND) {
        return 1;
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

static reach_result reach_wallpaper_create_target(reach_wallpaper_surface *surface, reach_wallpaper_window *window)
{
    if (surface == nullptr || window == nullptr || surface->factory == nullptr || window->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    RECT client = {};
    GetClientRect(window->hwnd, &client);
    UINT width = static_cast<UINT>(client.right - client.left);
    UINT height = static_cast<UINT>(client.bottom - client.top);
    if (width == 0) {
        width = 1;
    }
    if (height == 0) {
        height = 1;
    }

    HRESULT hr = surface->factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(window->hwnd, D2D1::SizeU(width, height)),
        &window->target);
    if (SUCCEEDED(hr)) {
        window->target_width = width;
        window->target_height = height;
    }
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static void reach_wallpaper_unload_bitmap(reach_wallpaper_window *window)
{
    if (window == nullptr) {
        return;
    }
    if (window->bitmap != nullptr) {
        window->bitmap->Release();
        window->bitmap = nullptr;
    }
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

static reach_result reach_wallpaper_load_bitmap(reach_wallpaper_surface *surface, reach_wallpaper_window *window, const uint16_t *path)
{
    if (surface == nullptr || window == nullptr || window->target == nullptr || surface->wic_factory == nullptr ||
        path == nullptr || path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }
    if (window->bitmap != nullptr && reach_wallpaper_path_equal(window->bitmap_path, path)) {
        return REACH_OK;
    }

    reach_wallpaper_unload_bitmap(window);

    IWICBitmapDecoder *decoder = nullptr;
    HRESULT hr = surface->wic_factory->CreateDecoderFromFilename(
        reinterpret_cast<const wchar_t *>(path),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder);

    IWICBitmapFrameDecode *frame = nullptr;
    IWICFormatConverter *converter = nullptr;
    if (SUCCEEDED(hr)) {
        hr = decoder->GetFrame(0, &frame);
    }
    if (SUCCEEDED(hr)) {
        hr = surface->wic_factory->CreateFormatConverter(&converter);
    }
    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(
            frame,
            GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeMedianCut);
    }
    if (SUCCEEDED(hr)) {
        hr = window->target->CreateBitmapFromWicBitmap(converter, nullptr, &window->bitmap);
    }

    if (converter != nullptr) {
        converter->Release();
    }
    if (frame != nullptr) {
        frame->Release();
    }
    if (decoder != nullptr) {
        decoder->Release();
    }
    if (FAILED(hr)) {
        reach_wallpaper_unload_bitmap(window);
        return REACH_ERROR;
    }

    return reach_copy_utf16(window->bitmap_path, 260, path);
}

static reach_result reach_wallpaper_begin_frame(reach_wallpaper_surface *surface, reach_wallpaper_window *window)
{
    if (surface == nullptr || window == nullptr || window->target == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    RECT client = {};
    GetClientRect(window->hwnd, &client);
    UINT width = static_cast<UINT>(client.right - client.left);
    UINT height = static_cast<UINT>(client.bottom - client.top);
    if (width == 0) {
        width = 1;
    }
    if (height == 0) {
        height = 1;
    }
    if (width != window->target_width || height != window->target_height) {
        reach_wallpaper_unload_bitmap(window);
        HRESULT resize_hr = window->target->Resize(D2D1::SizeU(width, height));
        if (FAILED(resize_hr)) {
            return REACH_ERROR;
        }
        window->target_width = width;
        window->target_height = height;
    }

    window->target->BeginDraw();
    window->target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f));
    return REACH_OK;
}

static reach_result reach_wallpaper_end_frame(reach_wallpaper_surface *surface, reach_wallpaper_window *window)
{
    if (surface == nullptr || window == nullptr || window->target == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    HRESULT hr = window->target->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        reach_wallpaper_unload_bitmap(window);
        window->target->Release();
        window->target = nullptr;
        return reach_wallpaper_create_target(surface, window);
    }
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static void reach_wallpaper_draw_fill(reach_wallpaper_window *window)
{
    if (window == nullptr || window->target == nullptr || window->bitmap == nullptr) {
        return;
    }

    D2D1_SIZE_F target = window->target->GetSize();
    D2D1_SIZE_F image = window->bitmap->GetSize();
    if (target.width <= 0.0f || target.height <= 0.0f || image.width <= 0.0f || image.height <= 0.0f) {
        return;
    }

    float target_aspect = target.width / target.height;
    float image_aspect = image.width / image.height;
    D2D1_RECT_F source = D2D1::RectF(0.0f, 0.0f, image.width, image.height);
    if (image_aspect > target_aspect) {
        float crop_width = image.height * target_aspect;
        float left = (image.width - crop_width) * 0.5f;
        source.left = left;
        source.right = left + crop_width;
    } else if (image_aspect < target_aspect) {
        float crop_height = image.width / target_aspect;
        float top = (image.height - crop_height) * 0.5f;
        source.top = top;
        source.bottom = top + crop_height;
    }

    D2D1_RECT_F dest = D2D1::RectF(0.0f, 0.0f, target.width, target.height);
    window->target->DrawBitmap(window->bitmap, dest, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, source);
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

static reach_result reach_wallpaper_render_window(reach_wallpaper_surface *surface, reach_wallpaper_window *window, size_t monitor_index)
{
    reach_result result = reach_wallpaper_begin_frame(surface, window);
    if (result != REACH_OK) {
        return result;
    }
    const uint16_t *path = reach_wallpaper_path_for_monitor(surface, monitor_index);
    if (path != nullptr && path[0] != 0) {
        result = reach_wallpaper_load_bitmap(surface, window, path);
        if (result != REACH_OK) {
            reach_wallpaper_unload_bitmap(window);
            (void)reach_wallpaper_end_frame(surface, window);
            return REACH_OK;
        }
    }
    reach_wallpaper_draw_fill(window);
    return reach_wallpaper_end_frame(surface, window);
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
    if (window->target != nullptr) {
        window->target->Release();
    }
    if (window->hwnd != nullptr) {
        DestroyWindow(window->hwnd);
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

static reach_result reach_wallpaper_collect_monitor_bounds(reach_wallpaper_monitor_enum *out_state, reach_rect_f32 fallback)
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

static reach_wallpaper_window *reach_wallpaper_create_window(reach_wallpaper_surface *surface, reach_rect_f32 bounds)
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
    if (reach_wallpaper_create_target(surface, window) != REACH_OK) {
        reach_wallpaper_window_destroy(window);
        return nullptr;
    }
    window->last_bounds = bounds;
    window->bounds_valid = 1;
    if (surface->visible) {
        ShowWindow(window->hwnd, SW_SHOWNOACTIVATE);
    }
    return window;
}

static reach_result reach_wallpaper_apply_window_bounds(
    reach_wallpaper_surface *surface,
    reach_wallpaper_window *window,
    size_t monitor_index,
    reach_rect_f32 bounds)
{
    if (surface == nullptr || window == nullptr || window->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
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
    return reach_wallpaper_render_window(surface, window, monitor_index);
}

static reach_result reach_wallpaper_sync_monitors(reach_wallpaper_surface *surface, reach_rect_f32 fallback_bounds)
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
    while (surface->windows.size() < monitors.bounds.size()) {
        reach_wallpaper_window *window = reach_wallpaper_create_window(surface, monitors.bounds[surface->windows.size()]);
        if (window == nullptr) {
            return REACH_ERROR;
        }
        surface->windows.push_back(window);
    }

    for (size_t index = 0; index < surface->windows.size(); ++index) {
        result = reach_wallpaper_apply_window_bounds(surface, surface->windows[index], index, monitors.bounds[index]);
        if (result != REACH_OK) {
            return result;
        }
        if (surface->visible) {
            ShowWindow(surface->windows[index]->hwnd, SW_SHOWNOACTIVATE);
            SetWindowPos(surface->windows[index]->hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
        }
    }
    return reach_wallpaper_render_all(surface);
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
        ShowWindow(window->hwnd, SW_SHOWNOACTIVATE);
        SetWindowPos(window->hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
    }
    return reach_wallpaper_render_all(surface);
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

static reach_result reach_wallpaper_surface_set_wallpaper(reach_wallpaper_surface *surface, const uint16_t *path)
{
    if (surface == nullptr || path == nullptr || path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = reach_copy_utf16(surface->path, 260, path);
    if (result != REACH_OK) return result;
    for (reach_wallpaper_window *window : surface->windows) {
        reach_wallpaper_unload_bitmap(window);
    }
    return reach_wallpaper_render_all(surface);
}

static reach_result reach_wallpaper_surface_set_monitor_wallpaper(reach_wallpaper_surface *surface, size_t monitor_index, const uint16_t *path)
{
    if (surface == nullptr || path == nullptr || path[0] == 0 || monitor_index >= REACH_MAX_WALLPAPER_MONITORS) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = reach_copy_utf16(surface->monitor_paths[monitor_index], 260, path);
    if (result != REACH_OK) return result;
    if (monitor_index < surface->windows.size()) {
        reach_wallpaper_unload_bitmap(surface->windows[monitor_index]);
        return reach_wallpaper_render_window(surface, surface->windows[monitor_index], monitor_index);
    }
    return REACH_OK;
}

static reach_result reach_wallpaper_surface_clear_monitor_wallpaper(reach_wallpaper_surface *surface, size_t monitor_index)
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
    }
    if (surface->factory != nullptr) {
        surface->factory->Release();
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

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &surface->factory);
    if (SUCCEEDED(hr)) {
        hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&surface->wic_factory));
    }
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
