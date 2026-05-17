#include "reach/platform/windows_adapters.h"

#include <windows.h>
#include <d2d1_1.h>
#include <wincodec.h>

#include <math.h>
#include <new>

struct reach_wallpaper_surface {
    HWND hwnd;
    ID2D1Factory1 *factory;
    ID2D1HwndRenderTarget *target;
    IWICImagingFactory *wic_factory;
    ID2D1Bitmap *bitmap;
    uint16_t path[260];
    uint16_t bitmap_path[260];
    reach_rect_f32 last_bounds;
    int32_t bounds_valid;
    UINT target_width;
    UINT target_height;
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

static reach_result reach_wallpaper_create_target(reach_wallpaper_surface *surface)
{
    if (surface == nullptr || surface->factory == nullptr || surface->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    RECT client = {};
    GetClientRect(surface->hwnd, &client);
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
        D2D1::HwndRenderTargetProperties(surface->hwnd, D2D1::SizeU(width, height)),
        &surface->target);
    if (SUCCEEDED(hr)) {
        surface->target_width = width;
        surface->target_height = height;
    }
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static void reach_wallpaper_unload_bitmap(reach_wallpaper_surface *surface)
{
    if (surface == nullptr) {
        return;
    }
    if (surface->bitmap != nullptr) {
        surface->bitmap->Release();
        surface->bitmap = nullptr;
    }
    surface->bitmap_path[0] = 0;
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

static reach_result reach_wallpaper_load_bitmap(reach_wallpaper_surface *surface, const uint16_t *path)
{
    if (surface == nullptr || surface->target == nullptr || surface->wic_factory == nullptr ||
        path == nullptr || path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }
    if (surface->bitmap != nullptr && reach_wallpaper_path_equal(surface->bitmap_path, path)) {
        return REACH_OK;
    }

    reach_wallpaper_unload_bitmap(surface);

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
        hr = surface->target->CreateBitmapFromWicBitmap(converter, nullptr, &surface->bitmap);
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
        reach_wallpaper_unload_bitmap(surface);
        return REACH_ERROR;
    }

    return reach_copy_utf16(surface->bitmap_path, 260, path);
}

static reach_result reach_wallpaper_begin_frame(reach_wallpaper_surface *surface)
{
    if (surface == nullptr || surface->target == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    RECT client = {};
    GetClientRect(surface->hwnd, &client);
    UINT width = static_cast<UINT>(client.right - client.left);
    UINT height = static_cast<UINT>(client.bottom - client.top);
    if (width == 0) {
        width = 1;
    }
    if (height == 0) {
        height = 1;
    }
    if (width != surface->target_width || height != surface->target_height) {
        reach_wallpaper_unload_bitmap(surface);
        HRESULT resize_hr = surface->target->Resize(D2D1::SizeU(width, height));
        if (FAILED(resize_hr)) {
            return REACH_ERROR;
        }
        surface->target_width = width;
        surface->target_height = height;
    }

    surface->target->BeginDraw();
    surface->target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f));
    return REACH_OK;
}

static reach_result reach_wallpaper_end_frame(reach_wallpaper_surface *surface)
{
    if (surface == nullptr || surface->target == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    HRESULT hr = surface->target->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        reach_wallpaper_unload_bitmap(surface);
        surface->target->Release();
        surface->target = nullptr;
        return reach_wallpaper_create_target(surface);
    }
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static void reach_wallpaper_draw_fill(reach_wallpaper_surface *surface)
{
    if (surface == nullptr || surface->target == nullptr || surface->bitmap == nullptr) {
        return;
    }

    D2D1_SIZE_F target = surface->target->GetSize();
    D2D1_SIZE_F image = surface->bitmap->GetSize();
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
    surface->target->DrawBitmap(surface->bitmap, dest, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, source);
}

static reach_result reach_wallpaper_render(reach_wallpaper_surface *surface)
{
    reach_result result = reach_wallpaper_begin_frame(surface);
    if (result != REACH_OK) {
        return result;
    }
    if (surface != nullptr && surface->path[0] != 0) {
        result = reach_wallpaper_load_bitmap(surface, surface->path);
        if (result != REACH_OK) {
            reach_wallpaper_unload_bitmap(surface);
            (void)reach_wallpaper_end_frame(surface);
            return REACH_OK;
        }
    }
    reach_wallpaper_draw_fill(surface);
    return reach_wallpaper_end_frame(surface);
}

static reach_result reach_wallpaper_surface_show(reach_wallpaper_surface *surface)
{
    if (surface == nullptr || surface->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    ShowWindow(surface->hwnd, SW_SHOWNOACTIVATE);
    SetWindowPos(surface->hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
    surface->visible = 1;
    return reach_wallpaper_render(surface);
}

static reach_result reach_wallpaper_surface_hide(reach_wallpaper_surface *surface)
{
    if (surface == nullptr || surface->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    ShowWindow(surface->hwnd, SW_HIDE);
    surface->visible = 0;
    return REACH_OK;
}

static reach_result reach_wallpaper_surface_set_bounds(reach_wallpaper_surface *surface, reach_rect_f32 bounds)
{
    if (surface == nullptr || surface->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    if (!surface->bounds_valid ||
        fabsf(surface->last_bounds.x - bounds.x) >= 0.5f ||
        fabsf(surface->last_bounds.y - bounds.y) >= 0.5f ||
        fabsf(surface->last_bounds.width - bounds.width) >= 0.5f ||
        fabsf(surface->last_bounds.height - bounds.height) >= 0.5f) {
        BOOL ok = SetWindowPos(
            surface->hwnd,
            HWND_BOTTOM,
            (int)bounds.x,
            (int)bounds.y,
            (int)bounds.width,
            (int)bounds.height,
            SWP_NOACTIVATE);
        if (!ok) {
            return REACH_ERROR;
        }
        surface->last_bounds = bounds;
        surface->bounds_valid = 1;
        return reach_wallpaper_render(surface);
    }
    return REACH_OK;
}

static reach_result reach_wallpaper_surface_set_wallpaper(reach_wallpaper_surface *surface, const uint16_t *path)
{
    if (surface == nullptr || path == nullptr || path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = reach_copy_utf16(surface->path, 260, path);
    if (result != REACH_OK) return result;
    return reach_wallpaper_render(surface);
}

static reach_result reach_wallpaper_surface_clear(reach_wallpaper_surface *surface)
{
    if (surface == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_wallpaper_unload_bitmap(surface);
    surface->path[0] = 0;
    return reach_wallpaper_render(surface);
}

static void reach_wallpaper_surface_destroy(reach_wallpaper_surface *surface)
{
    if (surface == nullptr) {
        return;
    }

    reach_wallpaper_unload_bitmap(surface);
    if (surface->target != nullptr) {
        surface->target->Release();
    }
    if (surface->wic_factory != nullptr) {
        surface->wic_factory->Release();
    }
    if (surface->factory != nullptr) {
        surface->factory->Release();
    }
    if (surface->hwnd != nullptr) {
        DestroyWindow(surface->hwnd);
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

    surface->hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        reach_wallpaper_window_class_name(),
        L"Reach Wallpaper",
        WS_POPUP,
        0,
        0,
        1,
        1,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    if (surface->hwnd == nullptr) {
        delete surface;
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
    if (SUCCEEDED(hr)) {
        hr = reach_wallpaper_create_target(surface) == REACH_OK ? S_OK : E_FAIL;
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
    out_port->ops.clear = reach_wallpaper_surface_clear;
    out_port->ops.destroy = reach_wallpaper_surface_destroy;
    return REACH_OK;
}
