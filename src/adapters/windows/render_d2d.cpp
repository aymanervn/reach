#include "reach/platform/windows_adapters.h"

#include "reach/ports/render_backend.h"

#include <windows.h>
#include <d2d1_1.h>
#include <dcomp.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <roapi.h>
#include <wincodec.h>
#include <windows.ui.composition.h>
#include <windows.ui.composition.desktop.h>
#include <windows.ui.composition.interop.h>
#include <wrl/client.h>

#include <stdint.h>
#include <new>
#include <vector>

using Microsoft::WRL::ComPtr;

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
    int ro_initialized;
    ComPtr<ABI::Windows::UI::Composition::ICompositor> compositor;
    ComPtr<ABI::Windows::UI::Composition::ICompositionTarget> composition_target;
    ComPtr<ABI::Windows::UI::Composition::IContainerVisual> root_visual;
    ComPtr<ABI::Windows::UI::Composition::ISpriteVisual> backdrop_visual;
    ComPtr<ABI::Windows::UI::Composition::ISpriteVisual> swap_chain_visual;
    ComPtr<ABI::Windows::UI::Composition::ICompositionSurface> composition_surface;
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

static void reach_d2d_log_hresult(const wchar_t *context, HRESULT hr)
{
    if (SUCCEEDED(hr)) {
        return;
    }
    wchar_t message[256] = {};
    wsprintfW(
        message,
        L"Reach: %s failed with HRESULT 0x%08lX\r\n",
        context != nullptr ? context : L"renderer operation",
        static_cast<unsigned long>(hr));
    OutputDebugStringW(message);
}

static HRESULT reach_d3d11_create_device(ID3D11Device **out_device, D3D_FEATURE_LEVEL *out_level)
{
    if (out_device == nullptr || out_level == nullptr) {
        return E_INVALIDARG;
    }
    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        levels,
        sizeof(levels) / sizeof(levels[0]),
        D3D11_SDK_VERSION,
        out_device,
        out_level,
        nullptr);
    if (hr == E_INVALIDARG) {
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels + 1,
            (sizeof(levels) / sizeof(levels[0])) - 1,
            D3D11_SDK_VERSION,
            out_device,
            out_level,
            nullptr);
    }
    return hr;
}

static HRESULT reach_winrt_activate_compositor(ABI::Windows::UI::Composition::ICompositor **out_compositor)
{
    if (out_compositor == nullptr) {
        return E_INVALIDARG;
    }
    *out_compositor = nullptr;

    HSTRING class_name = nullptr;
    HRESULT hr = WindowsCreateString(
        L"Windows.UI.Composition.Compositor",
        static_cast<UINT32>(wcslen(L"Windows.UI.Composition.Compositor")),
        &class_name);
    if (SUCCEEDED(hr)) {
        IInspectable *inspectable = nullptr;
        hr = RoActivateInstance(class_name, &inspectable);
        if (SUCCEEDED(hr) && inspectable != nullptr) {
            hr = inspectable->QueryInterface(IID_PPV_ARGS(out_compositor));
            inspectable->Release();
        }
        WindowsDeleteString(class_name);
    }
    return hr;
}

static HRESULT reach_visual_set_size(IInspectable *inspectable, float width, float height)
{
    if (inspectable == nullptr) {
        return E_INVALIDARG;
    }
    ComPtr<ABI::Windows::UI::Composition::IVisual> visual;
    HRESULT hr = inspectable->QueryInterface(IID_PPV_ARGS(&visual));
    if (SUCCEEDED(hr)) {
        ABI::Windows::Foundation::Numerics::Vector2 size = {};
        size.X = width;
        size.Y = height;
        hr = visual->put_Size(size);
    }
    return hr;
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

    D3D_FEATURE_LEVEL actual_level = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = reach_d3d11_create_device(&backend->d3d_device, &actual_level);
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
            if (backend->root_visual != nullptr) {
                (void)reach_visual_set_size(backend->root_visual.Get(), (float)width, (float)height);
            }
            if (backend->backdrop_visual != nullptr) {
                (void)reach_visual_set_size(backend->backdrop_visual.Get(), (float)width, (float)height);
            }
            if (backend->swap_chain_visual != nullptr) {
                (void)reach_visual_set_size(backend->swap_chain_visual.Get(), (float)width, (float)height);
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

    if (command->radius > 0.0f) {
        ID2D1Factory *factory = nullptr;
        ID2D1RoundedRectangleGeometry *clip_geometry = nullptr;
        ID2D1Layer *clip_layer = nullptr;
        target->GetFactory(&factory);
        HRESULT hr = factory != nullptr
            ? factory->CreateRoundedRectangleGeometry(
                D2D1::RoundedRect(rect, command->radius, command->radius),
                &clip_geometry)
            : E_FAIL;
        if (SUCCEEDED(hr)) {
            hr = target->CreateLayer(nullptr, &clip_layer);
        }
        if (SUCCEEDED(hr)) {
            D2D1_LAYER_PARAMETERS layer = D2D1::LayerParameters(
                rect,
                clip_geometry,
                D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                D2D1::IdentityMatrix(),
                1.0f,
                nullptr,
                D2D1_LAYER_OPTIONS_NONE);
            target->PushLayer(layer, clip_layer);
            target->DrawBitmap(bitmap, rect, command->color.a, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            target->PopLayer();
        }
        if (clip_layer != nullptr) {
            clip_layer->Release();
        }
        if (clip_geometry != nullptr) {
            clip_geometry->Release();
        }
        if (factory != nullptr) {
            factory->Release();
        }
        return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
    }

    target->DrawBitmap(bitmap, rect, command->color.a, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    return REACH_OK;
}

static HRESULT reach_d2d_draw_gradient_line(
    ID2D1RenderTarget *target,
    D2D1_POINT_2F start,
    D2D1_POINT_2F end,
    reach_color color,
    float stroke_width,
    float end_alpha = 0.0f)
{
    D2D1_GRADIENT_STOP stops[2] = {};
    stops[0].position = 0.0f;
    stops[0].color = D2D1::ColorF(color.r, color.g, color.b, color.a);
    stops[1].position = 1.0f;
    stops[1].color = D2D1::ColorF(color.r, color.g, color.b, end_alpha);

    ID2D1GradientStopCollection *stop_collection = nullptr;
    HRESULT hr = target->CreateGradientStopCollection(stops, 2, &stop_collection);
    ID2D1LinearGradientBrush *brush = nullptr;
    if (SUCCEEDED(hr)) {
        hr = target->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(start, end),
            stop_collection,
            &brush);
    }
    if (SUCCEEDED(hr)) {
        target->DrawLine(start, end, brush, stroke_width);
    }
    if (brush != nullptr) {
        brush->Release();
    }
    if (stop_collection != nullptr) {
        stop_collection->Release();
    }
    return hr;
}

static HRESULT reach_d2d_draw_gradient_arc(
    ID2D1RenderTarget *target,
    D2D1_POINT_2F start,
    D2D1_POINT_2F end,
    D2D1_SIZE_F radius,
    D2D1_SWEEP_DIRECTION sweep,
    reach_color color,
    float stroke_width,
    float end_alpha = 0.85f)
{
    ID2D1PathGeometry *geometry = nullptr;
    ID2D1GeometrySink *sink = nullptr;
    ID2D1Factory *factory = nullptr;
    target->GetFactory(&factory);
    HRESULT hr = factory != nullptr ? factory->CreatePathGeometry(&geometry) : E_FAIL;
    if (SUCCEEDED(hr)) {
        hr = geometry->Open(&sink);
    }
    if (SUCCEEDED(hr)) {
        sink->BeginFigure(start, D2D1_FIGURE_BEGIN_HOLLOW);
        D2D1_ARC_SEGMENT arc = {};
        arc.point = end;
        arc.size = radius;
        arc.rotationAngle = 0.0f;
        arc.sweepDirection = sweep;
        arc.arcSize = D2D1_ARC_SIZE_SMALL;
        sink->AddArc(arc);
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
        hr = sink->Close();
    }

    D2D1_GRADIENT_STOP stops[2] = {};
    stops[0].position = 0.0f;
    stops[0].color = D2D1::ColorF(color.r, color.g, color.b, color.a);
    stops[1].position = 1.0f;
    stops[1].color = D2D1::ColorF(color.r, color.g, color.b, end_alpha);
    ID2D1GradientStopCollection *stop_collection = nullptr;
    ID2D1LinearGradientBrush *brush = nullptr;
    if (SUCCEEDED(hr)) {
        hr = target->CreateGradientStopCollection(stops, 2, &stop_collection);
    }
    if (SUCCEEDED(hr)) {
        hr = target->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(start, end),
            stop_collection,
            &brush);
    }
    if (SUCCEEDED(hr)) {
        target->DrawGeometry(geometry, brush, stroke_width);
    }
    if (brush != nullptr) {
        brush->Release();
    }
    if (stop_collection != nullptr) {
        stop_collection->Release();
    }
    if (sink != nullptr) {
        sink->Release();
    }
    if (geometry != nullptr) {
        geometry->Release();
    }
    if (factory != nullptr) {
        factory->Release();
    }
    return hr;
}

static reach_result reach_d2d_draw_backplate_edge(ID2D1RenderTarget *target, const reach_render_command *command)
{
    if (target == nullptr || command == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    float x = command->rect.x;
    float y = command->rect.y;
    float w = command->rect.width;
    float h = command->rect.height;
    float r = command->radius;
    float stroke = command->stroke_width > 0.0f ? command->stroke_width : 0.45f;
    float inset = 1.0f;
    float arc = r * 0.76f;

    HRESULT hr = reach_d2d_draw_gradient_arc(
        target,
        D2D1::Point2F(x + inset, y + arc),
        D2D1::Point2F(x + arc, y + inset),
        D2D1::SizeF(r - inset, r - inset),
        D2D1_SWEEP_DIRECTION_CLOCKWISE,
        command->color,
        stroke,
        0.0f);
    if (SUCCEEDED(hr)) {
        hr = reach_d2d_draw_gradient_line(
            target,
            D2D1::Point2F(x + r * 0.70f, y + inset),
            D2D1::Point2F(x + w * 0.58f, y + inset),
            command->color,
            stroke,
            0.0f);
    }
    if (SUCCEEDED(hr)) {
        hr = reach_d2d_draw_gradient_line(
            target,
            D2D1::Point2F(x + inset, y + r * 0.70f),
            D2D1::Point2F(x + inset, y + h * 0.58f),
            command->color,
            stroke,
            0.0f);
    }
    if (SUCCEEDED(hr)) {
        hr = reach_d2d_draw_gradient_arc(
            target,
            D2D1::Point2F(x + w - inset, y + h - arc),
            D2D1::Point2F(x + w - arc, y + h - inset),
            D2D1::SizeF(r - inset, r - inset),
            D2D1_SWEEP_DIRECTION_CLOCKWISE,
            command->color,
            stroke,
            0.0f);
    }
    if (SUCCEEDED(hr)) {
        hr = reach_d2d_draw_gradient_line(
            target,
            D2D1::Point2F(x + w - r * 0.70f, y + h - inset),
            D2D1::Point2F(x + w * 0.42f, y + h - inset),
            command->color,
            stroke,
            0.0f);
    }
    if (SUCCEEDED(hr)) {
        hr = reach_d2d_draw_gradient_line(
            target,
            D2D1::Point2F(x + w - inset, y + h - r * 0.70f),
            D2D1::Point2F(x + w - inset, y + h * 0.42f),
            command->color,
            stroke,
            0.0f);
    }
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_d2d_draw_notched_rounded_rect(ID2D1RenderTarget *target, const reach_render_command *command)
{
    if (target == nullptr || command == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    float x = command->rect.x;
    float y = command->rect.y;
    float w = command->rect.width;
    float h = command->rect.height;
    float r = command->radius;
    float notch_width = command->notch_width;
    float notch_height = command->notch_height;
    float notch_center = command->notch_center_x;
    float body_bottom = y + h - notch_height;
    if (r < 0.0f) {
        r = 0.0f;
    }
    if (r > w * 0.5f) {
        r = w * 0.5f;
    }
    if (r > (h - notch_height) * 0.5f) {
        r = (h - notch_height) * 0.5f;
    }
    if (notch_width < 0.0f) {
        notch_width = 0.0f;
    }
    if (notch_height < 0.0f) {
        notch_height = 0.0f;
    }
    float notch_left = notch_center - notch_width * 0.5f;
    float notch_right = notch_center + notch_width * 0.5f;
    if (notch_left < x + r) {
        notch_left = x + r;
    }
    if (notch_right > x + w - r) {
        notch_right = x + w - r;
    }
    notch_center = (notch_left + notch_right) * 0.5f;

    ID2D1SolidColorBrush *brush = nullptr;
    ID2D1Factory *factory = nullptr;
    ID2D1PathGeometry *geometry = nullptr;
    ID2D1GeometrySink *sink = nullptr;
    HRESULT hr = target->CreateSolidColorBrush(reach_d2d_color(command->color), &brush);
    if (SUCCEEDED(hr)) {
        target->GetFactory(&factory);
        hr = factory != nullptr ? factory->CreatePathGeometry(&geometry) : E_FAIL;
    }
    if (SUCCEEDED(hr)) {
        hr = geometry->Open(&sink);
    }
    if (SUCCEEDED(hr)) {
        sink->BeginFigure(D2D1::Point2F(x + r, y), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(x + w - r, y));

        D2D1_ARC_SEGMENT arc = {};
        arc.size = D2D1::SizeF(r, r);
        arc.rotationAngle = 0.0f;
        arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
        arc.arcSize = D2D1_ARC_SIZE_SMALL;

        arc.point = D2D1::Point2F(x + w, y + r);
        sink->AddArc(arc);
        sink->AddLine(D2D1::Point2F(x + w, body_bottom - r));
        arc.point = D2D1::Point2F(x + w - r, body_bottom);
        sink->AddArc(arc);
        sink->AddLine(D2D1::Point2F(notch_right, body_bottom));
        sink->AddLine(D2D1::Point2F(notch_center, body_bottom + notch_height));
        sink->AddLine(D2D1::Point2F(notch_left, body_bottom));
        sink->AddLine(D2D1::Point2F(x + r, body_bottom));
        arc.point = D2D1::Point2F(x, body_bottom - r);
        sink->AddArc(arc);
        sink->AddLine(D2D1::Point2F(x, y + r));
        arc.point = D2D1::Point2F(x + r, y);
        sink->AddArc(arc);
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        hr = sink->Close();
    }
    if (SUCCEEDED(hr)) {
        if (command->stroke_width > 0.0f) {
            target->DrawGeometry(geometry, brush, command->stroke_width);
        } else {
            target->FillGeometry(geometry, brush);
        }
    }
    if (sink != nullptr) {
        sink->Release();
    }
    if (geometry != nullptr) {
        geometry->Release();
    }
    if (factory != nullptr) {
        factory->Release();
    }
    if (brush != nullptr) {
        brush->Release();
    }
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_wuc_create_target(reach_render_backend *backend)
{
    if (backend == nullptr || backend->hwnd == nullptr || backend->factory == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    BOOL use_host_backdrop = TRUE;
    (void)DwmSetWindowAttribute(
        backend->hwnd,
        DWMWA_USE_HOSTBACKDROPBRUSH,
        &use_host_backdrop,
        sizeof(use_host_backdrop));

    RECT client = {};
    GetClientRect(backend->hwnd, &client);
    UINT width = static_cast<UINT>(client.right - client.left);
    UINT height = static_cast<UINT>(client.bottom - client.top);
    if (width == 0) width = 1;
    if (height == 0) height = 1;

    HRESULT hr = RoInitialize(RO_INIT_SINGLETHREADED);
    reach_d2d_log_hresult(L"RoInitialize", hr);
    if (SUCCEEDED(hr)) {
        backend->ro_initialized = 1;
    } else if (hr == RPC_E_CHANGED_MODE) {
        hr = S_OK;
    }

    D3D_FEATURE_LEVEL actual_level = D3D_FEATURE_LEVEL_11_0;
    if (SUCCEEDED(hr)) {
        hr = reach_d3d11_create_device(&backend->d3d_device, &actual_level);
        reach_d2d_log_hresult(L"D3D11CreateDevice", hr);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->d3d_device->QueryInterface(IID_PPV_ARGS(&backend->dxgi_device));
        reach_d2d_log_hresult(L"ID3D11Device::QueryInterface IDXGIDevice", hr);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->factory->CreateDevice(backend->dxgi_device, &backend->d2d_device);
        reach_d2d_log_hresult(L"ID2D1Factory1::CreateDevice", hr);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &backend->d2d_context);
        reach_d2d_log_hresult(L"ID2D1Device::CreateDeviceContext", hr);
    }
    if (SUCCEEDED(hr)) {
        hr = reach_dcomp_create_swap_chain(backend, width, height) == REACH_OK ? S_OK : E_FAIL;
        reach_d2d_log_hresult(L"CreateSwapChainForComposition", hr);
    }
    if (SUCCEEDED(hr)) {
        hr = reach_winrt_activate_compositor(&backend->compositor);
        reach_d2d_log_hresult(L"RoActivateInstance(Compositor)", hr);
    }

    ComPtr<ABI::Windows::UI::Composition::Desktop::ICompositorDesktopInterop> desktop_interop;
    if (SUCCEEDED(hr)) {
        hr = backend->compositor.As(&desktop_interop);
        reach_d2d_log_hresult(L"ICompositorDesktopInterop", hr);
    }
    ComPtr<ABI::Windows::UI::Composition::Desktop::IDesktopWindowTarget> desktop_target;
    if (SUCCEEDED(hr)) {
        hr = desktop_interop->CreateDesktopWindowTarget(backend->hwnd, TRUE, &desktop_target);
        reach_d2d_log_hresult(L"CreateDesktopWindowTarget", hr);
    }
    if (SUCCEEDED(hr)) {
        hr = desktop_target.As(&backend->composition_target);
        reach_d2d_log_hresult(L"ICompositionTarget", hr);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->compositor->CreateContainerVisual(&backend->root_visual);
        reach_d2d_log_hresult(L"CreateContainerVisual", hr);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->compositor->CreateSpriteVisual(&backend->backdrop_visual);
    }

    ComPtr<ABI::Windows::UI::Composition::ICompositor3> compositor3;
    if (SUCCEEDED(hr)) {
        hr = backend->compositor.As(&compositor3);
    }
    ComPtr<ABI::Windows::UI::Composition::ICompositionBackdropBrush> backdrop_brush;
    if (SUCCEEDED(hr)) {
        hr = compositor3->CreateHostBackdropBrush(&backdrop_brush);
        reach_d2d_log_hresult(L"CreateHostBackdropBrush", hr);
    }
    ComPtr<ABI::Windows::UI::Composition::ICompositionBrush> backdrop_base_brush;
    if (SUCCEEDED(hr)) {
        hr = backdrop_brush.As(&backdrop_base_brush);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->backdrop_visual->put_Brush(backdrop_base_brush.Get());
    }

    ComPtr<ABI::Windows::UI::Composition::ICompositorInterop> compositor_interop;
    if (SUCCEEDED(hr)) {
        hr = backend->compositor.As(&compositor_interop);
    }
    if (SUCCEEDED(hr)) {
        hr = compositor_interop->CreateCompositionSurfaceForSwapChain(backend->swap_chain, &backend->composition_surface);
        reach_d2d_log_hresult(L"CreateCompositionSurfaceForSwapChain", hr);
    }
    ComPtr<ABI::Windows::UI::Composition::ICompositionSurfaceBrush> surface_brush;
    if (SUCCEEDED(hr)) {
        hr = backend->compositor->CreateSurfaceBrushWithSurface(backend->composition_surface.Get(), &surface_brush);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->compositor->CreateSpriteVisual(&backend->swap_chain_visual);
    }
    ComPtr<ABI::Windows::UI::Composition::ICompositionBrush> surface_base_brush;
    if (SUCCEEDED(hr)) {
        hr = surface_brush.As(&surface_base_brush);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->swap_chain_visual->put_Brush(surface_base_brush.Get());
    }

    ComPtr<ABI::Windows::UI::Composition::IVisualCollection> children;
    if (SUCCEEDED(hr)) {
        hr = backend->root_visual->get_Children(&children);
    }
    ComPtr<ABI::Windows::UI::Composition::IVisual> backdrop_base_visual;
    if (SUCCEEDED(hr)) {
        hr = backend->backdrop_visual.As(&backdrop_base_visual);
    }
    if (SUCCEEDED(hr)) {
        hr = children->InsertAtTop(backdrop_base_visual.Get());
    }
    ComPtr<ABI::Windows::UI::Composition::IVisual> swap_chain_base_visual;
    if (SUCCEEDED(hr)) {
        hr = backend->swap_chain_visual.As(&swap_chain_base_visual);
    }
    if (SUCCEEDED(hr)) {
        hr = children->InsertAtTop(swap_chain_base_visual.Get());
    }
    ComPtr<ABI::Windows::UI::Composition::IVisual> root_base_visual;
    if (SUCCEEDED(hr)) {
        hr = backend->root_visual.As(&root_base_visual);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->composition_target->put_Root(root_base_visual.Get());
    }
    if (SUCCEEDED(hr)) {
        hr = reach_visual_set_size(root_base_visual.Get(), (float)width, (float)height);
    }
    if (SUCCEEDED(hr)) {
        hr = reach_visual_set_size(backdrop_base_visual.Get(), (float)width, (float)height);
    }
    if (SUCCEEDED(hr)) {
        hr = reach_visual_set_size(swap_chain_base_visual.Get(), (float)width, (float)height);
    }
    if (SUCCEEDED(hr)) {
        hr = reach_dcomp_create_target_bitmap(backend) == REACH_OK ? S_OK : E_FAIL;
    }
    if (SUCCEEDED(hr)) {
        backend->target_width = width;
        backend->target_height = height;
    }
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_dcomp_create_blur_target(reach_render_backend *backend)
{
    if (backend == nullptr || backend->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    return reach_dcomp_create_target(backend);
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
        if (command->type == REACH_RENDER_COMMAND_BACKPLATE_EDGE) {
            reach_result result = reach_d2d_draw_backplate_edge(target, command);
            if (result != REACH_OK) {
                return result;
            }
            continue;
        }
        if (command->type == REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT) {
            reach_result result = reach_d2d_draw_notched_rounded_rect(target, command);
            if (result != REACH_OK) {
                return result;
            }
            continue;
        }
        if (command->type == REACH_RENDER_COMMAND_TRIANGLE) {
            ID2D1SolidColorBrush *brush = nullptr;
            ID2D1Factory *factory = nullptr;
            ID2D1PathGeometry *geometry = nullptr;
            ID2D1GeometrySink *sink = nullptr;
            HRESULT hr = target->CreateSolidColorBrush(reach_d2d_color(command->color), &brush);
            if (SUCCEEDED(hr)) {
                target->GetFactory(&factory);
                hr = factory != nullptr ? factory->CreatePathGeometry(&geometry) : E_FAIL;
            }
            if (SUCCEEDED(hr)) {
                hr = geometry->Open(&sink);
            }
            if (SUCCEEDED(hr)) {
                float x = command->rect.x;
                float y = command->rect.y;
                float w = command->rect.width;
                float h = command->rect.height;
                sink->BeginFigure(D2D1::Point2F(x, y), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddLine(D2D1::Point2F(x + w, y));
                sink->AddLine(D2D1::Point2F(x + w * 0.5f, y + h));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                hr = sink->Close();
            }
            if (SUCCEEDED(hr)) {
                target->FillGeometry(geometry, brush);
            }
            if (sink != nullptr) {
                sink->Release();
            }
            if (geometry != nullptr) {
                geometry->Release();
            }
            if (factory != nullptr) {
                factory->Release();
            }
            if (brush != nullptr) {
                brush->Release();
            }
            if (FAILED(hr)) {
                return REACH_ERROR;
            }
            continue;
        }
        if (command->type == REACH_RENDER_COMMAND_NOTCH_STROKE) {
            ID2D1SolidColorBrush *brush = nullptr;
            HRESULT hr = target->CreateSolidColorBrush(reach_d2d_color(command->color), &brush);
            if (FAILED(hr)) {
                return REACH_ERROR;
            }
            float stroke_width = command->stroke_width > 0.0f ? command->stroke_width : 1.0f;
            float x = command->rect.x;
            float y = command->rect.y;
            float w = command->rect.width;
            float h = command->rect.height;
            D2D1_POINT_2F left = D2D1::Point2F(x, y);
            D2D1_POINT_2F tip = D2D1::Point2F(x + w * 0.5f, y + h);
            D2D1_POINT_2F right = D2D1::Point2F(x + w, y);
            target->DrawLine(left, tip, brush, stroke_width);
            target->DrawLine(tip, right, brush, stroke_width);
            brush->Release();
            continue;
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
            DWRITE_FONT_WEIGHT weight = command->text_weight > 0 ? (DWRITE_FONT_WEIGHT)command->text_weight : DWRITE_FONT_WEIGHT_NORMAL;
            float text_size = command->text_size > 0.0f ? command->text_size : 16.0f;
            HRESULT hr = backend->text_factory->CreateTextFormat(
                L"Segoe UI",
                nullptr,
                weight,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                text_size,
                L"",
                &format);
            if (SUCCEEDED(hr)) {
                DWRITE_TEXT_ALIGNMENT alignment = command->text_alignment != 0
                    ? (DWRITE_TEXT_ALIGNMENT)command->text_alignment
                    : DWRITE_TEXT_ALIGNMENT_CENTER;
                (void)format->SetTextAlignment(alignment);
                (void)format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                if (command->text_ellipsis) {
                    (void)format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                    IDWriteInlineObject *ellipsis = nullptr;
                    HRESULT trim_hr = backend->text_factory->CreateEllipsisTrimmingSign(format, &ellipsis);
                    if (SUCCEEDED(trim_hr)) {
                        DWRITE_TRIMMING trimming = {};
                        trimming.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
                        (void)format->SetTrimming(&trimming, ellipsis);
                        ellipsis->Release();
                    }
                }
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
    if (backend->composition_target != nullptr) {
        (void)backend->composition_target->put_Root(nullptr);
    }
    backend->composition_surface.Reset();
    backend->swap_chain_visual.Reset();
    backend->backdrop_visual.Reset();
    backend->root_visual.Reset();
    backend->composition_target.Reset();
    backend->compositor.Reset();
    if (backend->ro_initialized) {
        RoUninitialize();
        backend->ro_initialized = 0;
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
        hr = reach_wuc_create_target(backend) == REACH_OK ? S_OK : E_FAIL;
        if (FAILED(hr)) {
            backend->composition_surface.Reset();
            backend->swap_chain_visual.Reset();
            backend->backdrop_visual.Reset();
            backend->root_visual.Reset();
            if (backend->composition_target != nullptr) {
                (void)backend->composition_target->put_Root(nullptr);
            }
            backend->composition_target.Reset();
            backend->compositor.Reset();
            if (backend->ro_initialized) {
                RoUninitialize();
                backend->ro_initialized = 0;
            }
            if (backend->swap_chain_bitmap != nullptr) {
                backend->swap_chain_bitmap->Release();
                backend->swap_chain_bitmap = nullptr;
            }
            if (backend->swap_chain != nullptr) {
                backend->swap_chain->Release();
                backend->swap_chain = nullptr;
            }
            if (backend->d2d_context != nullptr) {
                backend->d2d_context->Release();
                backend->d2d_context = nullptr;
            }
            if (backend->d2d_device != nullptr) {
                backend->d2d_device->Release();
                backend->d2d_device = nullptr;
            }
            if (backend->dxgi_device != nullptr) {
                backend->dxgi_device->Release();
                backend->dxgi_device = nullptr;
            }
            if (backend->d3d_device != nullptr) {
                backend->d3d_device->Release();
                backend->d3d_device = nullptr;
            }
            hr = reach_dcomp_create_blur_target(backend) == REACH_OK ? S_OK : E_FAIL;
        }
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
