#include "render_d2d_internal.h"

reach_result reach_dcomp_create_target_bitmap(reach_render_backend *backend)
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

        hr = backend->d2d_context->CreateBitmapFromDxgiSurface(
            surface,
            &properties,
            &backend->swap_chain_bitmap);
    }

    if (surface != nullptr) {
        surface->Release();
    }

    if (SUCCEEDED(hr)) {
        backend->d2d_context->SetTarget(backend->swap_chain_bitmap);
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

reach_result reach_dcomp_create_swap_chain(reach_render_backend *backend, UINT width, UINT height)
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

        hr = factory->CreateSwapChainForComposition(
            backend->d3d_device,
            &desc,
            nullptr,
            &backend->swap_chain);
    }

    if (factory != nullptr) {
        factory->Release();
    }
    if (adapter != nullptr) {
        adapter->Release();
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

reach_result reach_dcomp_create_target(reach_render_backend *backend)
{
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

    D3D_FEATURE_LEVEL actual_level = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = reach_d3d11_create_device(&backend->d3d_device, &actual_level);
    if (SUCCEEDED(hr)) {
        hr = backend->d3d_device->QueryInterface(IID_PPV_ARGS(&backend->dxgi_device));
    }
    if (SUCCEEDED(hr)) {
        hr = backend->factory->CreateDevice(backend->dxgi_device, &backend->d2d_device);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->d2d_device->CreateDeviceContext(
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
            &backend->d2d_context);
    }
    if (SUCCEEDED(hr)) {
        hr = reach_dcomp_create_swap_chain(backend, width, height) == REACH_OK ? S_OK : E_FAIL;
    }
    if (SUCCEEDED(hr)) {
        hr = DCompositionCreateDevice(
            backend->dxgi_device,
            IID_PPV_ARGS(&backend->dcomp_device));
    }
    if (SUCCEEDED(hr)) {
        hr = backend->dcomp_device->CreateTargetForHwnd(
            backend->hwnd,
            TRUE,
            &backend->dcomp_target);
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

reach_result reach_dcomp_create_blur_target(reach_render_backend *backend)
{
    if (backend == nullptr || backend->hwnd == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    return reach_dcomp_create_target(backend);
}
