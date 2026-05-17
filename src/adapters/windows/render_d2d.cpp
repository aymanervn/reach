#include "reach/platform/windows_adapters.h"

#include "reach/ports/render_backend.h"

#include <windows.h>
#include <d2d1_1.h>
#include <dcomp.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <wincodec.h>

#include <stdint.h>
#include <new>
#include <vector>

struct reach_d2d_icon_cache_entry {
    uintptr_t icon_id;
    ID2D1Bitmap *bitmap;
};

struct reach_render_backend {
    HWND hwnd;
    ID2D1Factory1 *factory;
    ID2D1HwndRenderTarget *target;
    ID3D11Device *d3d_device;
    IDXGIDevice *dxgi_device;
    ID2D1Device *d2d_device;
    ID2D1DeviceContext *d2d_context;
    IDXGISwapChain1 *swap_chain;
    ID2D1Bitmap1 *swap_chain_bitmap;
    IDCompositionDevice *dcomp_device;
    IDCompositionTarget *dcomp_target;
    IDCompositionVisual *dcomp_visual;
    IDWriteFactory *text_factory;
    IWICImagingFactory *wic_factory;
    std::vector<reach_d2d_icon_cache_entry> icon_cache;
    UINT target_width;
    UINT target_height;
};

static ID2D1RenderTarget *reach_d2d_target(reach_render_backend *backend)
{
    if (backend == nullptr) {
        return nullptr;
    }
    if (backend->d2d_context != nullptr) {
        return backend->d2d_context;
    }
    return backend->target;
}

static D2D1_COLOR_F reach_d2d_color(reach_color color)
{
    return D2D1::ColorF(color.r, color.g, color.b, color.a);
}

static reach_result reach_d2d_create_target(reach_render_backend *backend)
{
    REACH_ASSERT(backend != nullptr);
    if (backend == nullptr || backend->hwnd == nullptr || backend->factory == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    RECT client = {};
    GetClientRect(backend->hwnd, &client);
    UINT width = static_cast<UINT>(client.right - client.left);
    UINT height = static_cast<UINT>(client.bottom - client.top);
    if (width == 0) {
        width = 1;
    }
    if (height == 0) {
        height = 1;
    }

    HRESULT hr = backend->factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(backend->hwnd, D2D1::SizeU(width, height)),
        &backend->target);
    if (SUCCEEDED(hr)) {
        backend->target_width = width;
        backend->target_height = height;
    }
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_dcomp_create_target_bitmap(reach_render_backend *backend)
{
    if (backend == nullptr || backend->swap_chain == nullptr || backend->d2d_context == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    IDXGISurface *surface = nullptr;
    HRESULT hr = backend->swap_chain->GetBuffer(0, IID_PPV_ARGS(&surface));
    if (SUCCEEDED(hr)) {
        D2D1_BITMAP_PROPERTIES1 properties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        hr = backend->d2d_context->CreateBitmapFromDxgiSurface(surface, &properties, &backend->swap_chain_bitmap);
    }
    if (surface != nullptr) {
        surface->Release();
    }
    if (SUCCEEDED(hr)) {
        backend->d2d_context->SetTarget(backend->swap_chain_bitmap);
    }
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_dcomp_create_swap_chain(reach_render_backend *backend, UINT width, UINT height)
{
    if (backend == nullptr || backend->dxgi_device == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    IDXGIAdapter *adapter = nullptr;
    IDXGIFactory2 *factory = nullptr;
    HRESULT hr = backend->dxgi_device->GetAdapter(&adapter);
    if (SUCCEEDED(hr)) {
        hr = adapter->GetParent(IID_PPV_ARGS(&factory));
    }
    if (SUCCEEDED(hr)) {
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.Stereo = FALSE;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        hr = factory->CreateSwapChainForComposition(backend->d3d_device, &desc, nullptr, &backend->swap_chain);
    }
    if (factory != nullptr) {
        factory->Release();
    }
    if (adapter != nullptr) {
        adapter->Release();
    }
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_dcomp_create_target(reach_render_backend *backend)
{
    if (backend == nullptr || backend->hwnd == nullptr || backend->factory == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    RECT client = {};
    GetClientRect(backend->hwnd, &client);
    UINT width = static_cast<UINT>(client.right - client.left);
    UINT height = static_cast<UINT>(client.bottom - client.top);
    if (width == 0) width = 1;
    if (height == 0) height = 1;

    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL actual_level = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        levels,
        sizeof(levels) / sizeof(levels[0]),
        D3D11_SDK_VERSION,
        &backend->d3d_device,
        &actual_level,
        nullptr);
    if (SUCCEEDED(hr)) {
        hr = backend->d3d_device->QueryInterface(IID_PPV_ARGS(&backend->dxgi_device));
    }
    if (SUCCEEDED(hr)) {
        hr = backend->factory->CreateDevice(backend->dxgi_device, &backend->d2d_device);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &backend->d2d_context);
    }
    if (SUCCEEDED(hr)) {
        hr = reach_dcomp_create_swap_chain(backend, width, height) == REACH_OK ? S_OK : E_FAIL;
    }
    if (SUCCEEDED(hr)) {
        hr = DCompositionCreateDevice(backend->dxgi_device, IID_PPV_ARGS(&backend->dcomp_device));
    }
    if (SUCCEEDED(hr)) {
        hr = backend->dcomp_device->CreateTargetForHwnd(backend->hwnd, TRUE, &backend->dcomp_target);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->dcomp_device->CreateVisual(&backend->dcomp_visual);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->dcomp_visual->SetContent(backend->swap_chain);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->dcomp_target->SetRoot(backend->dcomp_visual);
    }
    if (SUCCEEDED(hr)) {
        hr = reach_dcomp_create_target_bitmap(backend) == REACH_OK ? S_OK : E_FAIL;
    }
    if (SUCCEEDED(hr)) {
        hr = backend->dcomp_device->Commit();
    }
    if (SUCCEEDED(hr)) {
        backend->target_width = width;
        backend->target_height = height;
    }
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static void reach_d2d_clear_icon_cache(reach_render_backend *backend)
{
    if (backend == nullptr) {
        return;
    }

    for (reach_d2d_icon_cache_entry &entry : backend->icon_cache) {
        if (entry.bitmap != nullptr) {
            entry.bitmap->Release();
        }
    }
    backend->icon_cache.clear();
}

static reach_result reach_d2d_begin_frame(reach_render_backend *backend)
{
    REACH_ASSERT(backend != nullptr);
    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (backend == nullptr || target == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    RECT client = {};
    GetClientRect(backend->hwnd, &client);
    UINT width = static_cast<UINT>(client.right - client.left);
    UINT height = static_cast<UINT>(client.bottom - client.top);
    if (width == 0) {
        width = 1;
    }
    if (height == 0) {
        height = 1;
    }
    if (width != backend->target_width || height != backend->target_height) {
        reach_d2d_clear_icon_cache(backend);
        if (backend->d2d_context != nullptr && backend->swap_chain != nullptr) {
            backend->d2d_context->SetTarget(nullptr);
            if (backend->swap_chain_bitmap != nullptr) {
                backend->swap_chain_bitmap->Release();
                backend->swap_chain_bitmap = nullptr;
            }
            HRESULT resize_hr = backend->swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
            if (FAILED(resize_hr) || reach_dcomp_create_target_bitmap(backend) != REACH_OK) {
                return REACH_ERROR;
            }
        } else {
            HRESULT resize_hr = backend->target->Resize(D2D1::SizeU(width, height));
            if (FAILED(resize_hr)) {
                return REACH_ERROR;
            }
        }
        backend->target_width = width;
        backend->target_height = height;
    }

    target = reach_d2d_target(backend);
    target->BeginDraw();
    target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
    return REACH_OK;
}

static reach_result reach_d2d_end_frame(reach_render_backend *backend)
{
    REACH_ASSERT(backend != nullptr);
    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (backend == nullptr || target == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    HRESULT hr = target->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        reach_d2d_clear_icon_cache(backend);
        if (backend->target != nullptr) {
            backend->target->Release();
            backend->target = nullptr;
            return reach_d2d_create_target(backend);
        }
        return REACH_ERROR;
    }
    if (SUCCEEDED(hr) && backend->swap_chain != nullptr) {
        hr = backend->swap_chain->Present(1, 0);
    }
    if (SUCCEEDED(hr) && backend->dcomp_device != nullptr) {
        hr = backend->dcomp_device->Commit();
    }
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_d2d_create_icon_bitmap(reach_render_backend *backend, uintptr_t icon_id, ID2D1Bitmap **out_bitmap)
{
    REACH_ASSERT(backend != nullptr);
    REACH_ASSERT(out_bitmap != nullptr);
    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (backend == nullptr || target == nullptr || backend->wic_factory == nullptr || icon_id == 0 || out_bitmap == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_bitmap = nullptr;
    IWICBitmap *wic_bitmap = nullptr;
    HRESULT hr = backend->wic_factory->CreateBitmapFromHICON(reinterpret_cast<HICON>(icon_id), &wic_bitmap);
    if (FAILED(hr)) {
        return REACH_ERROR;
    }

    IWICFormatConverter *converter = nullptr;
    hr = backend->wic_factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(
            wic_bitmap,
            GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeMedianCut);
    }

    ID2D1Bitmap *bitmap = nullptr;
    if (SUCCEEDED(hr)) {
        hr = target->CreateBitmapFromWicBitmap(converter, nullptr, &bitmap);
    }
    if (SUCCEEDED(hr) && bitmap != nullptr) {
        *out_bitmap = bitmap;
    }
    if (converter != nullptr) {
        converter->Release();
    }
    wic_bitmap->Release();
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_d2d_get_icon_bitmap(reach_render_backend *backend, uintptr_t icon_id, ID2D1Bitmap **out_bitmap)
{
    REACH_ASSERT(backend != nullptr);
    REACH_ASSERT(out_bitmap != nullptr);
    if (backend == nullptr || out_bitmap == nullptr || icon_id == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    for (reach_d2d_icon_cache_entry &entry : backend->icon_cache) {
        if (entry.icon_id == icon_id && entry.bitmap != nullptr) {
            *out_bitmap = entry.bitmap;
            return REACH_OK;
        }
    }

    ID2D1Bitmap *bitmap = nullptr;
    reach_result result = reach_d2d_create_icon_bitmap(backend, icon_id, &bitmap);
    if (result != REACH_OK || bitmap == nullptr) {
        return result == REACH_OK ? REACH_ERROR : result;
    }

    try {
        backend->icon_cache.push_back({ icon_id, bitmap });
    } catch (...) {
        bitmap->Release();
        return REACH_ERROR;
    }

    *out_bitmap = bitmap;
    return REACH_OK;
}

static reach_result reach_d2d_draw_icon(reach_render_backend *backend, const reach_render_command *command)
{
    REACH_ASSERT(backend != nullptr);
    REACH_ASSERT(command != nullptr);
    if (backend == nullptr || command == nullptr || command->icon_id == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    ID2D1Bitmap *bitmap = nullptr;
    reach_result result = reach_d2d_get_icon_bitmap(backend, command->icon_id, &bitmap);
    if (result != REACH_OK || bitmap == nullptr) {
        return result == REACH_OK ? REACH_ERROR : result;
    }

    D2D1_RECT_F rect = D2D1::RectF(command->rect.x, command->rect.y, command->rect.x + command->rect.width, command->rect.y + command->rect.height);
    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (target == nullptr) {
        return REACH_ERROR;
    }
    target->DrawBitmap(bitmap, rect, command->color.a, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    return REACH_OK;
}

static reach_result reach_d2d_execute(reach_render_backend *backend, const reach_render_command_buffer *commands)
{
    REACH_ASSERT(backend != nullptr);
    REACH_ASSERT(commands != nullptr);
    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (backend == nullptr || target == nullptr || commands == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < commands->count; ++index) {
        const reach_render_command *command = &commands->commands[index];
        if (command->type == REACH_RENDER_COMMAND_ICON && command->icon_id != 0) {
            reach_result result = reach_d2d_draw_icon(backend, command);
            if (result == REACH_OK) {
                continue;
            }
        }

        if (command->type == REACH_RENDER_COMMAND_RECT ||
            command->type == REACH_RENDER_COMMAND_ICON ||
            command->type == REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE) {
            ID2D1SolidColorBrush *brush = nullptr;
            HRESULT hr = target->CreateSolidColorBrush(reach_d2d_color(command->color), &brush);
            if (FAILED(hr)) {
                return REACH_ERROR;
            }

            D2D1_ROUNDED_RECT rect = D2D1::RoundedRect(
                D2D1::RectF(command->rect.x, command->rect.y, command->rect.x + command->rect.width, command->rect.y + command->rect.height),
                command->radius,
                command->radius);
            if (command->type == REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE) {
                float stroke_width = command->stroke_width > 0.0f ? command->stroke_width : 1.0f;
                target->DrawRoundedRectangle(rect, brush, stroke_width);
            } else {
                target->FillRoundedRectangle(rect, brush);
            }
            brush->Release();
        } else if (command->type == REACH_RENDER_COMMAND_TEXT) {
            IDWriteTextFormat *format = nullptr;
            ID2D1SolidColorBrush *brush = nullptr;
            HRESULT hr = backend->text_factory->CreateTextFormat(
                L"Segoe UI",
                nullptr,
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                16.0f,
                L"",
                &format);
            if (SUCCEEDED(hr)) {
                (void)format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                (void)format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }
            if (SUCCEEDED(hr)) {
                hr = target->CreateSolidColorBrush(reach_d2d_color(command->color), &brush);
            }
            if (FAILED(hr)) {
                if (format != nullptr) {
                    format->Release();
                }
                return REACH_ERROR;
            }

            D2D1_RECT_F rect = D2D1::RectF(command->rect.x, command->rect.y, command->rect.x + command->rect.width, command->rect.y + command->rect.height);
            const wchar_t *text = reinterpret_cast<const wchar_t *>(command->text);
            if (text == nullptr) {
                brush->Release();
                format->Release();
                return REACH_INVALID_ARGUMENT;
            }
            target->DrawText(
                text,
                static_cast<UINT32>(wcslen(text)),
                format,
                rect,
                brush
            );
            brush->Release();
            format->Release();
        } else if (command->type == REACH_RENDER_COMMAND_BLUR_BACKGROUND) {
            // Implement blur through the platform surface/composition path, not core rendering logic.
        }
    }

    return REACH_OK;
}

static void reach_d2d_destroy(reach_render_backend *backend)
{
    if (backend == nullptr) {
        return;
    }

    reach_d2d_clear_icon_cache(backend);
    if (backend->d2d_context != nullptr) {
        backend->d2d_context->SetTarget(nullptr);
    }
    if (backend->swap_chain_bitmap != nullptr) {
        backend->swap_chain_bitmap->Release();
    }
    if (backend->dcomp_visual != nullptr) {
        backend->dcomp_visual->Release();
    }
    if (backend->dcomp_target != nullptr) {
        backend->dcomp_target->Release();
    }
    if (backend->dcomp_device != nullptr) {
        backend->dcomp_device->Release();
    }
    if (backend->swap_chain != nullptr) {
        backend->swap_chain->Release();
    }
    if (backend->d2d_context != nullptr) {
        backend->d2d_context->Release();
    }
    if (backend->d2d_device != nullptr) {
        backend->d2d_device->Release();
    }
    if (backend->dxgi_device != nullptr) {
        backend->dxgi_device->Release();
    }
    if (backend->d3d_device != nullptr) {
        backend->d3d_device->Release();
    }
    if (backend->target != nullptr) {
        backend->target->Release();
    }
    if (backend->text_factory != nullptr) {
        backend->text_factory->Release();
    }
    if (backend->wic_factory != nullptr) {
        backend->wic_factory->Release();
    }
    if (backend->factory != nullptr) {
        backend->factory->Release();
    }
    delete backend;
}

reach_result reach_windows_create_d2d_render_backend(void *native_window, reach_render_backend_port *out_port)
{
    REACH_ASSERT(native_window != nullptr);
    REACH_ASSERT(out_port != nullptr);
    if (native_window == nullptr || out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_render_backend *backend = new (std::nothrow) reach_render_backend();
    if (backend == nullptr) {
        return REACH_ERROR;
    }

    backend->hwnd = static_cast<HWND>(native_window);
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &backend->factory);
    if (SUCCEEDED(hr)) {
        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown **>(&backend->text_factory));
    }
    if (SUCCEEDED(hr)) {
        hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&backend->wic_factory));
    }
    if (SUCCEEDED(hr)) {
        hr = reach_d2d_create_target(backend) == REACH_OK ? S_OK : E_FAIL;
    }
    if (FAILED(hr)) {
        reach_d2d_destroy(backend);
        return REACH_ERROR;
    }

    out_port->backend = backend;
    out_port->ops.begin_frame = reach_d2d_begin_frame;
    out_port->ops.end_frame = reach_d2d_end_frame;
    out_port->ops.execute = reach_d2d_execute;
    out_port->ops.destroy = reach_d2d_destroy;
    return REACH_OK;
}

reach_result reach_windows_create_dcomp_render_backend(void *native_window, reach_render_backend_port *out_port)
{
    REACH_ASSERT(native_window != nullptr);
    REACH_ASSERT(out_port != nullptr);
    if (native_window == nullptr || out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_render_backend *backend = new (std::nothrow) reach_render_backend();
    if (backend == nullptr) {
        return REACH_ERROR;
    }

    backend->hwnd = static_cast<HWND>(native_window);
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &backend->factory);
    if (SUCCEEDED(hr)) {
        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown **>(&backend->text_factory));
    }
    if (SUCCEEDED(hr)) {
        hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&backend->wic_factory));
    }
    if (SUCCEEDED(hr)) {
        hr = reach_dcomp_create_target(backend) == REACH_OK ? S_OK : E_FAIL;
    }
    if (FAILED(hr)) {
        reach_d2d_destroy(backend);
        return REACH_ERROR;
    }

    out_port->backend = backend;
    out_port->ops.begin_frame = reach_d2d_begin_frame;
    out_port->ops.end_frame = reach_d2d_end_frame;
    out_port->ops.execute = reach_d2d_execute;
    out_port->ops.destroy = reach_d2d_destroy;
    return REACH_OK;
}
