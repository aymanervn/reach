#include "render_d2d_internal.h"

reach_result reach_d2d_begin_frame(reach_render_backend *backend)
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

            HRESULT resize_hr = backend->swap_chain->ResizeBuffers(
                0,
                width,
                height,
                DXGI_FORMAT_UNKNOWN,
                0);

            if (FAILED(resize_hr) || reach_dcomp_create_target_bitmap(backend) != REACH_OK) {
                return REACH_ERROR;
            }

            if (backend->root_visual != nullptr) {
                (void)reach_visual_set_size(
                    backend->root_visual.Get(),
                    static_cast<float>(width),
                    static_cast<float>(height));
            }
            if (backend->backdrop_visual != nullptr) {
                (void)reach_visual_set_size(
                    backend->backdrop_visual.Get(),
                    static_cast<float>(width),
                    static_cast<float>(height));
            }
            if (backend->swap_chain_visual != nullptr) {
                (void)reach_visual_set_size(
                    backend->swap_chain_visual.Get(),
                    static_cast<float>(width),
                    static_cast<float>(height));
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
    if (target == nullptr) {
        return REACH_ERROR;
    }

    target->BeginDraw();
    target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    return REACH_OK;
}

reach_result reach_d2d_end_frame(reach_render_backend *backend)
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
