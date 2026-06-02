#include "../windows_adapters_internal.h"

#include "render_d2d_internal.h"

void reach_d2d_destroy(reach_render_backend *backend)
{
    if (backend == nullptr)
    {
        return;
    }

    reach_d2d_clear_icon_cache(backend);

    if (backend->d2d_context != nullptr)
    {
        backend->d2d_context->SetTarget(nullptr);
    }

    if (backend->swap_chain_bitmap != nullptr)
    {
        backend->swap_chain_bitmap->Release();
    }
    if (backend->dcomp_visual != nullptr)
    {
        backend->dcomp_visual->Release();
    }
    if (backend->dcomp_target != nullptr)
    {
        backend->dcomp_target->Release();
    }
    if (backend->dcomp_device != nullptr)
    {
        backend->dcomp_device->Release();
    }

    if (backend->composition_target != nullptr)
    {
        (void)backend->composition_target->put_Root(nullptr);
    }

    backend->composition_surface.Reset();
    backend->swap_chain_visual.Reset();
    backend->backdrop_visual.Reset();
    backend->root_visual.Reset();
    backend->composition_target.Reset();
    backend->compositor.Reset();

    if (backend->ro_initialized)
    {
        RoUninitialize();
        backend->ro_initialized = 0;
    }

    if (backend->swap_chain != nullptr)
    {
        backend->swap_chain->Release();
    }
    if (backend->d2d_context != nullptr)
    {
        backend->d2d_context->Release();
    }
    if (backend->d2d_device != nullptr)
    {
        backend->d2d_device->Release();
    }
    if (backend->dxgi_device != nullptr)
    {
        backend->dxgi_device->Release();
    }
    if (backend->d3d_device != nullptr)
    {
        backend->d3d_device->Release();
    }
    if (backend->target != nullptr)
    {
        backend->target->Release();
    }
    if (backend->text_factory != nullptr)
    {
        backend->text_factory->Release();
    }
    if (backend->wic_factory != nullptr)
    {
        backend->wic_factory->Release();
    }
    if (backend->factory != nullptr)
    {
        backend->factory->Release();
    }

    delete backend;
}

static void reach_d2d_release_composition_target_state(reach_render_backend *backend)
{
    if (backend == nullptr)
    {
        return;
    }

    backend->composition_surface.Reset();
    backend->swap_chain_visual.Reset();
    backend->backdrop_visual.Reset();
    backend->root_visual.Reset();

    if (backend->composition_target != nullptr)
    {
        (void)backend->composition_target->put_Root(nullptr);
    }

    backend->composition_target.Reset();
    backend->compositor.Reset();

    if (backend->ro_initialized)
    {
        RoUninitialize();
        backend->ro_initialized = 0;
    }

    if (backend->swap_chain_bitmap != nullptr)
    {
        backend->swap_chain_bitmap->Release();
        backend->swap_chain_bitmap = nullptr;
    }
    if (backend->swap_chain != nullptr)
    {
        backend->swap_chain->Release();
        backend->swap_chain = nullptr;
    }
    if (backend->d2d_context != nullptr)
    {
        backend->d2d_context->Release();
        backend->d2d_context = nullptr;
    }
    if (backend->d2d_device != nullptr)
    {
        backend->d2d_device->Release();
        backend->d2d_device = nullptr;
    }
    if (backend->dxgi_device != nullptr)
    {
        backend->dxgi_device->Release();
        backend->dxgi_device = nullptr;
    }
    if (backend->d3d_device != nullptr)
    {
        backend->d3d_device->Release();
        backend->d3d_device = nullptr;
    }
}

reach_result reach_windows_create_d2d_render_backend(reach_platform_window *window,
                                                     reach_render_backend_port *out_port)
{
    REACH_ASSERT(window != nullptr);
    REACH_ASSERT(out_port != nullptr);

    if (window == nullptr || out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    void *native_window = reach_windows_platform_window_native_handle(window);
    if (native_window == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};

    reach_render_backend *backend = new (std::nothrow) reach_render_backend();
    if (backend == nullptr)
    {
        return REACH_ERROR;
    }

    backend->hwnd = static_cast<HWND>(native_window);

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &backend->factory);

    if (SUCCEEDED(hr))
    {
        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                 reinterpret_cast<IUnknown **>(&backend->text_factory));
    }

    if (SUCCEEDED(hr))
    {
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&backend->wic_factory));
    }

    if (SUCCEEDED(hr))
    {
        hr = reach_d2d_create_target(backend) == REACH_OK ? S_OK : E_FAIL;
    }

    if (FAILED(hr))
    {
        reach_d2d_destroy(backend);
        return REACH_ERROR;
    }

    out_port->backend = backend;
    out_port->ops.begin_frame = reach_d2d_begin_frame;
    out_port->ops.end_frame = reach_d2d_end_frame;
    out_port->ops.execute = reach_d2d_execute;
    out_port->ops.release_icon = reach_d2d_release_icon_cache_entry;
    out_port->ops.destroy = reach_d2d_destroy;

    return REACH_OK;
}

reach_result reach_windows_create_dcomp_render_backend(reach_platform_window *window,
                                                       reach_render_backend_port *out_port)
{
    REACH_ASSERT(window != nullptr);
    REACH_ASSERT(out_port != nullptr);

    if (window == nullptr || out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    void *native_window = reach_windows_platform_window_native_handle(window);
    if (native_window == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};

    reach_render_backend *backend = new (std::nothrow) reach_render_backend();
    if (backend == nullptr)
    {
        return REACH_ERROR;
    }

    backend->hwnd = static_cast<HWND>(native_window);

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &backend->factory);

    if (SUCCEEDED(hr))
    {
        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                 reinterpret_cast<IUnknown **>(&backend->text_factory));
    }

    if (SUCCEEDED(hr))
    {
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&backend->wic_factory));
    }

    if (SUCCEEDED(hr))
    {
        hr = reach_wuc_create_target(backend) == REACH_OK ? S_OK : E_FAIL;

        if (FAILED(hr))
        {
            reach_d2d_release_composition_target_state(backend);
            hr = reach_dcomp_create_blur_target(backend) == REACH_OK ? S_OK : E_FAIL;
        }
    }

    if (FAILED(hr))
    {
        reach_d2d_destroy(backend);
        return REACH_ERROR;
    }

    out_port->backend = backend;
    out_port->ops.begin_frame = reach_d2d_begin_frame;
    out_port->ops.end_frame = reach_d2d_end_frame;
    out_port->ops.execute = reach_d2d_execute;
    out_port->ops.release_icon = reach_d2d_release_icon_cache_entry;
    out_port->ops.destroy = reach_d2d_destroy;

    return REACH_OK;
}
