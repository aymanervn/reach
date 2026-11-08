#include "../windows_adapters_internal.h"

#include "render_d2d_internal.h"

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
        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                 reinterpret_cast<IUnknown **>(&device->text_factory));
    }
    if (SUCCEEDED(hr))
    {
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&device->wic_factory));
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
