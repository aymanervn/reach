#include "reach/renderer.h"

#include <windows.h>
#include <d2d1_1.h>
#include <dcomp.h>
#include <dwrite.h>
#include <dxgi1_6.h>

#include <new>

struct reach_renderer {
    HWND hwnd;
    ID2D1Factory1 *d2d_factory;
    ID2D1HwndRenderTarget *target;
    IDWriteFactory *dwrite_factory;
    IDCompositionDevice *composition_device;
};

reach_result reach_renderer_create(const reach_renderer_desc *desc, reach_renderer **out_renderer)
{
    if (desc == nullptr || out_renderer == nullptr || desc->native_window == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_renderer *renderer = new (std::nothrow) reach_renderer();
    if (renderer == nullptr) {
        *out_renderer = nullptr;
        return REACH_ERROR;
    }

    renderer->hwnd = static_cast<HWND>(desc->native_window);
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &renderer->d2d_factory);
    if (SUCCEEDED(hr)) {
        RECT client = {};
        GetClientRect(renderer->hwnd, &client);
        UINT width = desc->width > 0 ? static_cast<UINT>(desc->width) : static_cast<UINT>(client.right - client.left);
        UINT height = desc->height > 0 ? static_cast<UINT>(desc->height) : static_cast<UINT>(client.bottom - client.top);
        hr = renderer->d2d_factory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(renderer->hwnd, D2D1::SizeU(width, height)),
            &renderer->target);
    }
    if (SUCCEEDED(hr)) {
        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown **>(&renderer->dwrite_factory));
    }
    if (SUCCEEDED(hr)) {
        (void)DCompositionCreateDevice(nullptr, __uuidof(IDCompositionDevice), reinterpret_cast<void **>(&renderer->composition_device));
    }

    if (FAILED(hr)) {
        reach_renderer_destroy(renderer);
        *out_renderer = nullptr;
        return REACH_ERROR;
    }

    *out_renderer = renderer;
    return REACH_OK;
}

void reach_renderer_destroy(reach_renderer *renderer)
{
    if (renderer == nullptr) {
        return;
    }

    if (renderer->target != nullptr) {
        renderer->target->Release();
    }
    if (renderer->composition_device != nullptr) {
        renderer->composition_device->Release();
    }
    if (renderer->dwrite_factory != nullptr) {
        renderer->dwrite_factory->Release();
    }
    if (renderer->d2d_factory != nullptr) {
        renderer->d2d_factory->Release();
    }
    delete renderer;
}

reach_result reach_renderer_resize(reach_renderer *renderer, int32_t width, int32_t height)
{
    if (renderer == nullptr || renderer->target == nullptr || width <= 0 || height <= 0) {
        return REACH_INVALID_ARGUMENT;
    }

    HRESULT hr = renderer->target->Resize(D2D1::SizeU(static_cast<UINT32>(width), static_cast<UINT32>(height)));
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

reach_result reach_renderer_begin(reach_renderer *renderer)
{
    if (renderer == nullptr || renderer->target == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    renderer->target->BeginDraw();
    return REACH_OK;
}

reach_result reach_renderer_end(reach_renderer *renderer)
{
    if (renderer == nullptr || renderer->target == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    HRESULT hr = renderer->target->EndDraw();
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

reach_result reach_renderer_clear(reach_renderer *renderer, float r, float g, float b, float a)
{
    if (renderer == nullptr || renderer->target == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    renderer->target->Clear(D2D1::ColorF(r, g, b, a));
    return REACH_OK;
}
