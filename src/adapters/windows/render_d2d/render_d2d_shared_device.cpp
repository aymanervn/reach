#include "../windows_adapters_internal.h"

#include "render_d2d_internal.h"
#include "render_d2d_retained_policy.h"

reach_result reach_windows_create_render_device(reach_render_device **out_device)
{
    REACH_ASSERT(out_device != nullptr);
    if (out_device == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_device = nullptr;
    reach_render_device *device = new (std::nothrow) reach_render_device();
    if (device == nullptr)
    {
        return REACH_ERROR;
    }

    HRESULT hr = RoInitialize(RO_INIT_SINGLETHREADED);
    reach_d2d_log_hresult(L"RoInitialize", hr);
    if (SUCCEEDED(hr))
    {
        device->ro_initialized = 1;
    }
    else if (hr == RPC_E_CHANGED_MODE)
    {
        hr = S_OK;
    }

    if (SUCCEEDED(hr))
    {
        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &device->factory);
    }
    if (SUCCEEDED(hr))
    {
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
        hr = reach_d3d11_create_device(&device->d3d_device, &level);
        reach_d2d_log_hresult(L"D3D11CreateDevice", hr);
    }
    if (SUCCEEDED(hr))
    {
        hr = device->d3d_device->QueryInterface(IID_PPV_ARGS(&device->dxgi_device));
    }
    if (SUCCEEDED(hr))
    {
        hr = device->factory->CreateDevice(device->dxgi_device, &device->d2d_device);
        reach_d2d_log_hresult(L"ID2D1Factory1::CreateDevice", hr);
    }
    if (SUCCEEDED(hr))
    {
        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                 reinterpret_cast<IUnknown **>(&device->text_factory));
    }
    if (SUCCEEDED(hr))
    {
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&device->wic_factory));
    }

    if (SUCCEEDED(hr))
    {
        HRESULT compositor_hr = reach_winrt_activate_compositor(&device->compositor);
        reach_d2d_log_hresult(L"RoActivateInstance(Compositor)", compositor_hr);
        if (SUCCEEDED(compositor_hr))
        {
            ComPtr<ABI::Windows::UI::Composition::ICompositorInterop> interop;
            compositor_hr = device->compositor.As(&interop);
            if (SUCCEEDED(compositor_hr))
            {
                compositor_hr = interop->CreateGraphicsDevice(device->d2d_device,
                                                              &device->composition_graphics_device);
                reach_d2d_log_hresult(L"ICompositorInterop::CreateGraphicsDevice", compositor_hr);
            }
        }
        if (FAILED(compositor_hr))
        {
            device->composition_graphics_device.Reset();
            device->compositor.Reset();
        }
    }

    if (FAILED(hr))
    {
        reach_windows_destroy_render_device(device);
        return REACH_ERROR;
    }

    *out_device = device;
    return REACH_OK;
}

void reach_windows_destroy_render_device(reach_render_device *device)
{
    if (device == nullptr)
    {
        return;
    }

    reach_d2d_device_release_device_resources(device);
    reach_d2d_device_release_fonts(device);
    device->composition_graphics_device.Reset();
    device->compositor.Reset();
    if (device->d2d_device != nullptr)
    {
        device->d2d_device->Release();
    }
    if (device->dxgi_device != nullptr)
    {
        device->dxgi_device->Release();
    }
    if (device->d3d_device != nullptr)
    {
        device->d3d_device->Release();
    }
    if (device->wic_factory != nullptr)
    {
        device->wic_factory->Release();
    }
    if (device->text_factory != nullptr)
    {
        device->text_factory->Release();
    }
    if (device->factory != nullptr)
    {
        device->factory->Release();
    }
    if (device->ro_initialized)
    {
        RoUninitialize();
    }

    delete device;
}

void reach_d2d_device_surface_activated(reach_render_backend *backend)
{
    if (backend == nullptr || backend->device == nullptr)
    {
        return;
    }

    reach_d2d_surface_count_activate(&backend->surface_active, &backend->device->active_surfaces);
}

void reach_d2d_device_surface_deactivated(reach_render_backend *backend)
{
    if (backend == nullptr || backend->device == nullptr)
    {
        return;
    }

    reach_d2d_surface_count_deactivate(&backend->surface_active, &backend->device->active_surfaces);
}

void reach_d2d_device_release_device_resources(reach_render_device *device)
{
    if (device == nullptr)
    {
        return;
    }

    for (reach_d2d_icon_cache_entry &entry : device->icon_cache)
    {
        if (entry.bitmap != nullptr)
        {
            entry.bitmap->Release();
        }
    }
    device->icon_cache.clear();
    device->cover_cache.clear();

    for (reach_d2d_shadow_cache_entry &entry : device->shadow_cache)
    {
        if (entry.bitmap != nullptr)
        {
            entry.bitmap->Release();
        }
    }
    device->shadow_cache.clear();

    ComPtr<ID2D1DeviceContext> &shadow_bake = device->shadow_bake_context;
    ComPtr<ID2D1DeviceContext> &shadow_fallback = device->shadow_fallback_context;
    ComPtr<ID2D1DeviceContext> &cover_bake = device->cover_bake_context;
    for (ID2D1DeviceContext *context : {shadow_bake.Get(), shadow_fallback.Get(), cover_bake.Get()})
    {
        if (context != nullptr)
        {
            context->SetTarget(nullptr);
        }
    }
    shadow_bake.Reset();
    shadow_fallback.Reset();
    cover_bake.Reset();
    device->shadow_fallback_parent_device = nullptr;
}

void reach_d2d_device_trim_if_idle(reach_render_device *device)
{
    if (device == nullptr || device->active_surfaces != 0)
    {
        return;
    }

    if (device->d2d_device != nullptr)
    {
        (void)device->d2d_device->ClearResources(0);
    }
    if (device->d3d_device != nullptr)
    {
        ComPtr<ID3D11DeviceContext> immediate;
        device->d3d_device->GetImmediateContext(&immediate);
        if (immediate != nullptr)
        {
            immediate->ClearState();
            immediate->Flush();
        }
    }
    ComPtr<IDXGIDevice3> dxgi_device;
    if (device->dxgi_device != nullptr &&
        SUCCEEDED(device->dxgi_device->QueryInterface(IID_PPV_ARGS(&dxgi_device))))
    {
        dxgi_device->Trim();
    }
}
