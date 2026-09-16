#include "render_d2d_internal.h"

static HRESULT reach_dcomp_create_animated_text_visual(reach_render_backend *backend,
                                                       reach_d2d_animated_text_layer *layer)
{
    HRESULT hr = backend->dcomp_device->CreateVisual(&layer->dcomp_clip_visual);
    if (SUCCEEDED(hr))
    {
        hr = backend->dcomp_device->CreateVisual(&layer->dcomp_content_visual);
    }
    if (SUCCEEDED(hr))
    {
        hr = layer->dcomp_clip_visual->AddVisual(layer->dcomp_content_visual.Get(), TRUE, nullptr);
    }
    if (FAILED(hr))
    {
        layer->dcomp_content_visual.Reset();
        layer->dcomp_clip_visual.Reset();
    }
    return hr;
}

static HRESULT reach_dcomp_draw_animated_text_surface(reach_render_backend *backend,
                                                      reach_d2d_animated_text_layer *layer,
                                                      const reach_render_command *command)
{
    UINT width = static_cast<UINT>(ceilf(command->rect.width));
    UINT height = static_cast<UINT>(ceilf(command->rect.height));
    if (width == 0 || height == 0)
    {
        return E_INVALIDARG;
    }

    layer->dcomp_surface.Reset();
    HRESULT hr = backend->dcomp_device->CreateSurface(
        width, height, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_PREMULTIPLIED,
        &layer->dcomp_surface);

    ComPtr<ID2D1DeviceContext> context;
    POINT offset = {};
    if (SUCCEEDED(hr))
    {
        hr = layer->dcomp_surface->BeginDraw(nullptr, IID_PPV_ARGS(&context), &offset);
    }
    if (SUCCEEDED(hr))
    {
        context->SetTransform(D2D1::Matrix3x2F::Translation((float)offset.x, (float)offset.y));
        context->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
        reach_render_command local = *command;
        local.type = REACH_RENDER_COMMAND_TEXT;
        local.rect = {0.0f, 0.0f, command->rect.width, command->rect.height};
        local.has_scissor = 0;
        hr = reach_d2d_draw_text_to_target(backend, context.Get(), &local) == REACH_OK ? S_OK
                                                                                       : E_FAIL;
        HRESULT end_hr = layer->dcomp_surface->EndDraw();
        if (SUCCEEDED(hr))
        {
            hr = end_hr;
        }
    }
    if (SUCCEEDED(hr))
    {
        hr = layer->dcomp_content_visual->SetContent(layer->dcomp_surface.Get());
    }
    return hr;
}

static HRESULT reach_dcomp_set_animated_text_geometry(reach_d2d_animated_text_layer *layer,
                                                      const reach_render_command *command,
                                                      const reach_transform_f32 *transform)
{
    D2D_RECT_F clip = D2D1::RectF(0.0f, 0.0f, command->scissor_rect.width,
                                  command->scissor_rect.height);
    D2D_MATRIX_3X2_F scale = D2D1::Matrix3x2F::Scale(transform->scale_x, transform->scale_y);
    HRESULT hr = layer->dcomp_clip_visual->SetOffsetX(command->scissor_rect.x * transform->scale_x +
                                                      transform->offset_x);
    if (SUCCEEDED(hr))
    {
        hr = layer->dcomp_clip_visual->SetOffsetY(command->scissor_rect.y * transform->scale_y +
                                                  transform->offset_y);
    }
    if (SUCCEEDED(hr))
    {
        hr = layer->dcomp_clip_visual->SetTransform(scale);
    }
    if (SUCCEEDED(hr))
    {
        hr = layer->dcomp_clip_visual->SetClip(clip);
    }
    if (SUCCEEDED(hr))
    {
        hr = layer->dcomp_content_visual->SetOffsetY(command->rect.y - command->scissor_rect.y);
    }
    return hr;
}

static HRESULT reach_dcomp_start_animated_text(reach_render_backend *backend,
                                               reach_d2d_animated_text_layer *layer,
                                               const reach_render_command *command)
{
    float base_x = command->rect.x - command->scissor_rect.x;
    float hold = command->animation_hold_seconds;
    float travel = command->animation_travel_seconds;
    float cycle = (hold + travel) * 2.0f;
    if (command->animation_offset_x == 0.0f || hold < 0.0f || travel <= 0.0f || cycle <= 0.0f)
    {
        layer->dcomp_animation.Reset();
        return layer->dcomp_content_visual->SetOffsetX(base_x);
    }

    layer->dcomp_animation.Reset();
    HRESULT hr = backend->dcomp_device->CreateAnimation(&layer->dcomp_animation);
    if (SUCCEEDED(hr) && hold > 0.0f)
    {
        hr = layer->dcomp_animation->AddCubic(0.0, base_x, 0.0f, 0.0f, 0.0f);
    }

    float offset = command->animation_offset_x;
    float inverse_travel = 1.0f / travel;
    float quadratic = 3.0f * offset * inverse_travel * inverse_travel;
    float cubic = -2.0f * offset * inverse_travel * inverse_travel * inverse_travel;
    if (SUCCEEDED(hr))
    {
        hr = layer->dcomp_animation->AddCubic(hold, base_x, 0.0f, quadratic, cubic);
    }
    if (SUCCEEDED(hr) && hold > 0.0f)
    {
        hr = layer->dcomp_animation->AddCubic(hold + travel, base_x + offset, 0.0f, 0.0f, 0.0f);
    }
    if (SUCCEEDED(hr))
    {
        hr = layer->dcomp_animation->AddCubic(hold * 2.0f + travel, base_x + offset, 0.0f,
                                              -quadratic, -cubic);
    }
    if (SUCCEEDED(hr))
    {
        hr = layer->dcomp_animation->AddRepeat(cycle, cycle);
    }
    if (SUCCEEDED(hr))
    {
        hr = layer->dcomp_content_visual->SetOffsetX(layer->dcomp_animation.Get());
    }
    return hr;
}

reach_result reach_dcomp_sync_animated_text(reach_render_backend *backend,
                                            const reach_render_command *command,
                                            const reach_transform_f32 *transform)
{
    if (backend == nullptr || backend->dcomp_device == nullptr || backend->dcomp_visual == nullptr ||
        command == nullptr || transform == nullptr || !command->has_scissor)
    {
        return REACH_NOT_IMPLEMENTED;
    }

    reach_d2d_animated_text_layer *layer = &backend->animated_text_layer;
    layer->seen = 1;
    int32_t same_command =
        layer->has_command && reach_d2d_animated_text_command_equal(&layer->command, command);
    if (same_command && layer->visible)
    {
        if (memcmp(&layer->transform, transform, sizeof(*transform)) == 0)
        {
            return REACH_OK;
        }
        HRESULT geometry_hr = reach_dcomp_set_animated_text_geometry(layer, command, transform);
        if (FAILED(geometry_hr))
        {
            reach_d2d_log_hresult(L"DirectComposition animated text geometry", geometry_hr);
            return REACH_ERROR;
        }
        layer->transform = *transform;
        return REACH_OK;
    }

    HRESULT hr = layer->dcomp_clip_visual != nullptr && layer->dcomp_content_visual != nullptr
                     ? S_OK
                     : reach_dcomp_create_animated_text_visual(backend, layer);
    if (SUCCEEDED(hr))
    {
        hr = reach_dcomp_draw_animated_text_surface(backend, layer, command);
    }
    if (SUCCEEDED(hr))
    {
        hr = reach_dcomp_set_animated_text_geometry(layer, command, transform);
    }
    if (SUCCEEDED(hr))
    {
        hr = reach_dcomp_start_animated_text(backend, layer, command);
    }
    if (SUCCEEDED(hr) && !layer->visible)
    {
        hr = backend->dcomp_visual->AddVisual(layer->dcomp_clip_visual.Get(), TRUE, nullptr);
    }
    if (FAILED(hr))
    {
        reach_d2d_log_hresult(L"DirectComposition animated text layer", hr);
        return REACH_ERROR;
    }

    layer->command = *command;
    layer->transform = *transform;
    layer->has_command = 1;
    layer->visible = 1;
    return REACH_OK;
}

void reach_dcomp_hide_animated_text(reach_render_backend *backend)
{
    if (backend == nullptr || backend->dcomp_visual == nullptr)
    {
        return;
    }
    reach_d2d_animated_text_layer &layer = backend->animated_text_layer;
    if (layer.visible && layer.dcomp_clip_visual != nullptr)
    {
        (void)backend->dcomp_visual->RemoveVisual(layer.dcomp_clip_visual.Get());
        layer.visible = 0;
    }
}

reach_result reach_dcomp_create_target_bitmap(reach_render_backend *backend)
{
    if (backend == nullptr || backend->swap_chain == nullptr || backend->d2d_context == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    IDXGISurface *surface = nullptr;
    HRESULT hr = backend->swap_chain->GetBuffer(0, IID_PPV_ARGS(&surface));

    if (SUCCEEDED(hr))
    {
        D2D1_BITMAP_PROPERTIES1 properties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

        hr = backend->d2d_context->CreateBitmapFromDxgiSurface(surface, &properties,
                                                               &backend->swap_chain_bitmap);
    }

    if (surface != nullptr)
    {
        surface->Release();
    }

    if (SUCCEEDED(hr))
    {
        backend->d2d_context->SetTarget(backend->swap_chain_bitmap);
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

reach_result reach_dcomp_create_swap_chain(reach_render_backend *backend, UINT width, UINT height)
{
    if (backend == nullptr || backend->dxgi_device == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    IDXGIAdapter *adapter = nullptr;
    IDXGIFactory2 *factory = nullptr;

    HRESULT hr = backend->dxgi_device->GetAdapter(&adapter);
    if (SUCCEEDED(hr))
    {
        hr = adapter->GetParent(IID_PPV_ARGS(&factory));
    }

    if (SUCCEEDED(hr))
    {
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

        hr = factory->CreateSwapChainForComposition(backend->d3d_device, &desc, nullptr,
                                                    &backend->swap_chain);
    }

    if (factory != nullptr)
    {
        factory->Release();
    }
    if (adapter != nullptr)
    {
        adapter->Release();
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

reach_result reach_dcomp_create_target(reach_render_backend *backend)
{
    if (backend == nullptr || backend->hwnd == nullptr || backend->factory == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

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

    D3D_FEATURE_LEVEL actual_level = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = reach_d3d11_create_device(&backend->d3d_device, &actual_level);
    if (SUCCEEDED(hr))
    {
        hr = backend->d3d_device->QueryInterface(IID_PPV_ARGS(&backend->dxgi_device));
    }
    if (SUCCEEDED(hr))
    {
        hr = backend->factory->CreateDevice(backend->dxgi_device, &backend->d2d_device);
    }
    if (SUCCEEDED(hr))
    {
        hr = backend->d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                                      &backend->d2d_context);
    }
    if (SUCCEEDED(hr))
    {
        hr = reach_dcomp_create_swap_chain(backend, width, height) == REACH_OK ? S_OK : E_FAIL;
    }
    if (SUCCEEDED(hr))
    {
        hr = DCompositionCreateDevice2(backend->d2d_device, IID_PPV_ARGS(&backend->dcomp_device));
    }
    if (SUCCEEDED(hr))
    {
        hr =
            backend->dcomp_device->CreateTargetForHwnd(backend->hwnd, TRUE, &backend->dcomp_target);
    }
    if (SUCCEEDED(hr))
    {
        hr = backend->dcomp_device->CreateVisual(&backend->dcomp_visual);
    }
    if (SUCCEEDED(hr))
    {
        hr = backend->dcomp_visual->SetContent(backend->swap_chain);
    }
    if (SUCCEEDED(hr))
    {
        hr = backend->dcomp_target->SetRoot(backend->dcomp_visual);
    }
    if (SUCCEEDED(hr))
    {
        hr = reach_dcomp_create_target_bitmap(backend) == REACH_OK ? S_OK : E_FAIL;
    }
    if (SUCCEEDED(hr))
    {
        hr = backend->dcomp_device->Commit();
    }
    if (SUCCEEDED(hr))
    {
        backend->target_width = width;
        backend->target_height = height;
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

reach_result reach_dcomp_create_blur_target(reach_render_backend *backend)
{
    if (backend == nullptr || backend->hwnd == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    return reach_dcomp_create_target(backend);
}
