#include "render_d2d_internal.h"
#include "reach/support/workload.h"

#include <dwmapi.h>

static reach_result reach_d2d_begin_frame_impl(reach_render_backend *backend)
{
    REACH_ASSERT(backend != nullptr);

    reach_result sync_result = reach_d2d_backend_sync_device(backend);
    if (sync_result != REACH_OK)
    {
        return sync_result;
    }

    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (backend == nullptr || target == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_d2d_device_surface_activated(backend);

    RECT client = {};
    GetClientRect(backend->hwnd, &client);

    UINT width = static_cast<UINT>(client.right - client.left);
    UINT height = static_cast<UINT>(client.bottom - client.top);

    if (width == 0)
    {
        width = 1;
    }
    if (height == 0)
    {
        height = 1;
    }

    if (width != backend->target_width || height != backend->target_height)
    {
        if (backend->d2d_context != nullptr && backend->swap_chain != nullptr)
        {
            backend->d2d_context->SetTarget(nullptr);

            if (backend->swap_chain_bitmap != nullptr)
            {
                backend->swap_chain_bitmap->Release();
                backend->swap_chain_bitmap = nullptr;
            }

            HRESULT resize_hr =
                backend->swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

            reach_result resize_result =
                FAILED(resize_hr)
                    ? reach_d2d_device_check(backend->device, resize_hr, L"ResizeBuffers")
                    : reach_dcomp_create_target_bitmap(backend);
            if (resize_result != REACH_OK)
            {
                return resize_result;
            }

            if (backend->root_visual != nullptr)
            {
                (void)reach_visual_set_size(backend->root_visual.Get(), static_cast<float>(width),
                                            static_cast<float>(height));
            }
            if (backend->backdrop_visual != nullptr)
            {
                (void)reach_visual_set_size(backend->backdrop_visual.Get(),
                                            static_cast<float>(width), static_cast<float>(height));
            }
            if (backend->swap_chain_visual != nullptr)
            {
                (void)reach_visual_set_size(backend->swap_chain_visual.Get(),
                                            static_cast<float>(width), static_cast<float>(height));
            }
        }

        if (backend->root_visual != nullptr)
            (void)reach_visual_set_size(backend->root_visual.Get(), (float)width, (float)height);
        if (backend->backdrop_visual != nullptr)
            (void)reach_visual_set_size(backend->backdrop_visual.Get(), (float)width,
                                        (float)height);

        backend->target_width = width;
        backend->target_height = height;
        backend->backdrop_content_rect = {};
    }

    target = reach_d2d_target(backend);
    if (target == nullptr)
    {
        return REACH_ERROR;
    }

    if (!reach_d2d_retained_supported(backend))
    {
        reach_workload_add(REACH_WORKLOAD_RASTER_PASSES, 1);
        target->BeginDraw();
        target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
    }

    return REACH_OK;
}

reach_result reach_d2d_begin_frame(reach_render_backend *backend)
{
    REACH_ASSERT(backend != nullptr);
    if (backend == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    return reach_d2d_device_result(backend->device, reach_d2d_begin_frame_impl(backend),
                                   L"begin_frame");
}

static reach_result reach_d2d_end_frame_impl(reach_render_backend *backend)
{
    REACH_ASSERT(backend != nullptr);

    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (backend == nullptr || target == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    HRESULT hr = reach_d2d_retained_supported(backend) ? S_OK : target->EndDraw();
    if (FAILED(hr))
    {
        return reach_d2d_device_check(backend->device, hr, L"EndDraw");
    }

    if (backend->swap_chain != nullptr && !reach_d2d_retained_supported(backend))
    {
        hr = backend->swap_chain->Present(1, 0);
        reach_workload_add(REACH_WORKLOAD_SWAP_CHAIN_PRESENTS, 1);
        if (FAILED(hr))
        {
            return reach_d2d_device_check(backend->device, hr, L"Present");
        }
    }

    if (backend->dcomp_device != nullptr)
    {
        hr = backend->dcomp_device->Commit();
        reach_workload_add(REACH_WORKLOAD_DCOMP_COMMITS, 1);
        if (FAILED(hr))
        {
            return reach_d2d_device_check(backend->device, hr, L"DCompCommit");
        }
    }

    return REACH_OK;
}

reach_result reach_d2d_end_frame(reach_render_backend *backend)
{
    REACH_ASSERT(backend != nullptr);
    if (backend == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    return reach_d2d_device_result(backend->device, reach_d2d_end_frame_impl(backend),
                                   L"end_frame");
}

reach_result reach_d2d_synchronize(reach_render_backend *backend)
{
    if (backend == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (backend->dcomp_device != nullptr)
    {
        HRESULT hr = backend->dcomp_device->WaitForCommitCompletion();
        if (FAILED(hr))
        {
            return reach_d2d_device_check(backend->device, hr, L"WaitForCommitCompletion");
        }
    }
    return SUCCEEDED(DwmFlush()) ? REACH_OK : REACH_ERROR;
}
