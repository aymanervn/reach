#include "../windows_adapters_internal.h"

#include "render_d2d_internal.h"

void reach_d2d_destroy(reach_render_backend *backend)
{
    if (backend == nullptr)
    {
        return;
    }

    reach_d2d_release_retained(backend);

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
    backend->composition_graphics_device.Reset();
    backend->swap_chain_visual.Reset();
    backend->backdrop_visual.Reset();
    backend->root_visual.Reset();
    backend->composition_target.Reset();
    backend->compositor.Reset();

    if (backend->swap_chain != nullptr)
    {
        backend->swap_chain->Release();
    }
    if (backend->d2d_context != nullptr)
    {
        backend->d2d_context->Release();
    }
    reach_d2d_device_surface_deactivated(backend);
    reach_d2d_release_fonts(backend);

    delete backend;
}

static void reach_d2d_release_composition_target_state(reach_render_backend *backend)
{
    if (backend == nullptr)
    {
        return;
    }

    reach_d2d_release_retained(backend);

    backend->composition_surface.Reset();
    backend->composition_graphics_device.Reset();
    backend->swap_chain_visual.Reset();
    backend->backdrop_visual.Reset();
    backend->root_visual.Reset();

    if (backend->composition_target != nullptr)
    {
        (void)backend->composition_target->put_Root(nullptr);
    }

    backend->composition_target.Reset();
    backend->compositor.Reset();

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
}

static void reach_d2d_release_dcomp_target_state(reach_render_backend *backend)
{
    reach_d2d_release_retained(backend);

    if (backend->d2d_context != nullptr)
    {
        backend->d2d_context->SetTarget(nullptr);
    }
    if (backend->swap_chain_bitmap != nullptr)
    {
        backend->swap_chain_bitmap->Release();
        backend->swap_chain_bitmap = nullptr;
    }
    if (backend->dcomp_visual != nullptr)
    {
        backend->dcomp_visual->Release();
        backend->dcomp_visual = nullptr;
    }
    if (backend->dcomp_target != nullptr)
    {
        backend->dcomp_target->Release();
        backend->dcomp_target = nullptr;
    }
    if (backend->dcomp_device != nullptr)
    {
        backend->dcomp_device->Release();
        backend->dcomp_device = nullptr;
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
}

static reach_result reach_d2d_backend_reset_for_device(reach_render_backend *backend)
{
    reach_render_device *device = backend->device;
    if (backend->presentation_result == REACH_DEVICE_LOST)
    {
        backend->presentation_result = REACH_OK;
    }

    if (backend->dcomp_device != nullptr)
    {
        reach_d2d_release_dcomp_target_state(backend);
        return reach_dcomp_create_target(backend);
    }

    reach_d2d_release_swap_chain(backend);
    if (backend->d2d_context != nullptr)
    {
        backend->d2d_context->SetTarget(nullptr);
        backend->d2d_context->Release();
        backend->d2d_context = nullptr;
    }

    HRESULT hr = device->d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                                         &backend->d2d_context);
    if (FAILED(hr))
    {
        return reach_d2d_device_check(device, hr, L"CreateDeviceContext");
    }

    reach_d2d_invalidate_retained_content(backend);
    return REACH_OK;
}

reach_result reach_d2d_backend_sync_device(reach_render_backend *backend)
{
    reach_render_device *device = backend->device;
    if (backend->device_generation == device->generation)
    {
        return REACH_OK;
    }
    if (device->lost)
    {
        return REACH_DEVICE_LOST;
    }

    reach_result result = reach_d2d_backend_reset_for_device(backend);
    if (result == REACH_OK)
    {
        backend->device_generation = device->generation;
    }
    return result;
}

reach_result reach_windows_create_dcomp_render_backend(reach_render_device *device,
                                                       reach_platform_window *window,
                                                       reach_render_backend_port *out_port)
{
    REACH_ASSERT(device != nullptr);
    REACH_ASSERT(window != nullptr);
    REACH_ASSERT(out_port != nullptr);

    if (device == nullptr || window == nullptr || out_port == nullptr)
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
    backend->device = device;
    backend->device_generation = device->generation;

    HRESULT hr = reach_wuc_create_target(backend) == REACH_OK ? S_OK : E_FAIL;

    if (FAILED(hr))
    {
        reach_d2d_release_composition_target_state(backend);
        hr = reach_dcomp_create_blur_target(backend) == REACH_OK ? S_OK : E_FAIL;
    }

    if (FAILED(hr))
    {
        reach_d2d_destroy(backend);
        return REACH_ERROR;
    }

    out_port->backend = backend;
    out_port->ops.begin_frame = reach_d2d_begin_frame;
    out_port->ops.end_frame = reach_d2d_end_frame;
    out_port->ops.synchronize = reach_d2d_synchronize;
    out_port->ops.retained_supported = reach_d2d_retained_supported;
    out_port->ops.presentation_complete = reach_d2d_presentation_complete;
    out_port->ops.suspend = reach_d2d_suspend;
    out_port->ops.content_sampling_required = reach_d2d_content_sampling_required;
    out_port->ops.sample_content = reach_d2d_sample_content;
    out_port->ops.execute = reach_d2d_execute;
    out_port->ops.release_icon = reach_d2d_release_icon_cache_entry;
    out_port->ops.set_ui_font = reach_d2d_set_ui_font;
    out_port->ops.measure_text = reach_d2d_measure_text;
    out_port->ops.destroy = reach_d2d_destroy;

    return REACH_OK;
}
