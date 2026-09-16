#include "render_d2d_internal.h"

HRESULT reach_winrt_activate_compositor(ABI::Windows::UI::Composition::ICompositor **out_compositor)
{
    if (out_compositor == nullptr)
    {
        return E_INVALIDARG;
    }

    *out_compositor = nullptr;

    HSTRING class_name = nullptr;
    HRESULT hr = WindowsCreateString(
        L"Windows.UI.Composition.Compositor",
        static_cast<UINT32>(wcslen(L"Windows.UI.Composition.Compositor")), &class_name);

    if (SUCCEEDED(hr))
    {
        IInspectable *inspectable = nullptr;
        hr = RoActivateInstance(class_name, &inspectable);
        if (SUCCEEDED(hr) && inspectable != nullptr)
        {
            hr = inspectable->QueryInterface(IID_PPV_ARGS(out_compositor));
            inspectable->Release();
        }
        WindowsDeleteString(class_name);
    }

    return hr;
}

HRESULT reach_visual_set_size(IInspectable *inspectable, float width, float height)
{
    if (inspectable == nullptr)
    {
        return E_INVALIDARG;
    }

    ComPtr<ABI::Windows::UI::Composition::IVisual> visual;
    HRESULT hr = inspectable->QueryInterface(IID_PPV_ARGS(&visual));

    if (SUCCEEDED(hr))
    {
        ABI::Windows::Foundation::Numerics::Vector2 size = {};
        size.X = width;
        size.Y = height;
        hr = visual->put_Size(size);
    }

    return hr;
}

int32_t reach_d2d_animated_text_command_equal(const reach_render_command *left,
                                              const reach_render_command *right)
{
    if (left->rect.x != right->rect.x || left->rect.y != right->rect.y ||
        left->rect.width != right->rect.width || left->rect.height != right->rect.height ||
        left->scissor_rect.x != right->scissor_rect.x ||
        left->scissor_rect.y != right->scissor_rect.y ||
        left->scissor_rect.width != right->scissor_rect.width ||
        left->scissor_rect.height != right->scissor_rect.height ||
        left->color.r != right->color.r || left->color.g != right->color.g ||
        left->color.b != right->color.b || left->color.a != right->color.a ||
        left->text_weight != right->text_weight || left->text_size != right->text_size ||
        left->text_alignment != right->text_alignment ||
        left->animation_offset_x != right->animation_offset_x ||
        left->animation_hold_seconds != right->animation_hold_seconds ||
        left->animation_travel_seconds != right->animation_travel_seconds)
    {
        return 0;
    }
    for (size_t index = 0; index < 260; ++index)
    {
        if (left->text[index] != right->text[index])
        {
            return 0;
        }
        if (left->text[index] == 0)
        {
            return 1;
        }
    }
    return 1;
}

static HRESULT reach_d2d_create_animated_text_visual(reach_render_backend *backend,
                                                     reach_d2d_animated_text_layer *layer)
{
    HRESULT hr = backend->compositor->CreateContainerVisual(&layer->clip_visual);
    if (SUCCEEDED(hr))
    {
        hr = backend->compositor->CreateSpriteVisual(&layer->content_visual);
    }

    ComPtr<ABI::Windows::UI::Composition::IVisualCollection> clip_children;
    if (SUCCEEDED(hr))
    {
        hr = layer->clip_visual->get_Children(&clip_children);
    }
    ComPtr<ABI::Windows::UI::Composition::IVisual> content_base;
    if (SUCCEEDED(hr))
    {
        hr = layer->content_visual.As(&content_base);
    }
    if (SUCCEEDED(hr))
    {
        hr = clip_children->InsertAtTop(content_base.Get());
    }

    ComPtr<ABI::Windows::UI::Composition::IVisualCollection> root_children;
    if (SUCCEEDED(hr))
    {
        hr = backend->root_visual->get_Children(&root_children);
    }
    ComPtr<ABI::Windows::UI::Composition::IVisual> clip_base;
    if (SUCCEEDED(hr))
    {
        hr = layer->clip_visual.As(&clip_base);
    }
    if (SUCCEEDED(hr))
    {
        hr = root_children->InsertAtTop(clip_base.Get());
    }
    if (FAILED(hr))
    {
        layer->content_visual.Reset();
        layer->clip_visual.Reset();
    }
    return hr;
}

static HRESULT reach_d2d_draw_animated_text_surface(reach_render_backend *backend,
                                                    reach_d2d_animated_text_layer *layer,
                                                    const reach_render_command *command)
{
    float width = ceilf(command->rect.width);
    float height = ceilf(command->rect.height);
    if (width <= 0.0f || height <= 0.0f)
    {
        return E_INVALIDARG;
    }

    ABI::Windows::Foundation::Size size = {width, height};
    layer->surface.Reset();
    layer->brush.Reset();
    HRESULT hr = backend->composition_graphics_device->CreateDrawingSurface(
        size, ABI::Windows::Graphics::DirectX::DirectXPixelFormat_B8G8R8A8UIntNormalized,
        ABI::Windows::Graphics::DirectX::DirectXAlphaMode_Premultiplied, &layer->surface);
    ComPtr<ABI::Windows::UI::Composition::ICompositionSurface> surface_base;
    if (SUCCEEDED(hr))
    {
        hr = layer->surface.As(&surface_base);
    }
    if (SUCCEEDED(hr))
    {
        hr = backend->compositor->CreateSurfaceBrushWithSurface(surface_base.Get(), &layer->brush);
    }
    if (SUCCEEDED(hr))
    {
        hr = layer->brush->put_Stretch(ABI::Windows::UI::Composition::CompositionStretch_None);
    }
    ComPtr<ABI::Windows::UI::Composition::ICompositionBrush> brush_base;
    if (SUCCEEDED(hr))
    {
        hr = layer->brush.As(&brush_base);
    }
    if (SUCCEEDED(hr))
    {
        hr = layer->content_visual->put_Brush(brush_base.Get());
    }

    ComPtr<ABI::Windows::UI::Composition::ICompositionDrawingSurfaceInterop> interop;
    if (SUCCEEDED(hr))
    {
        hr = layer->surface.As(&interop);
    }
    ComPtr<ID2D1DeviceContext> context;
    POINT offset = {};
    if (SUCCEEDED(hr))
    {
        hr = interop->BeginDraw(nullptr, IID_PPV_ARGS(&context), &offset);
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
        HRESULT end_hr = interop->EndDraw();
        if (SUCCEEDED(hr))
        {
            hr = end_hr;
        }
    }
    return hr;
}

static HRESULT reach_d2d_set_animated_text_geometry(reach_render_backend *backend,
                                                    reach_d2d_animated_text_layer *layer,
                                                    const reach_render_command *command,
                                                    const reach_transform_f32 *transform)
{
    ComPtr<ABI::Windows::UI::Composition::IVisual> clip_base;
    HRESULT hr = layer->clip_visual.As(&clip_base);
    ABI::Windows::Foundation::Numerics::Vector3 clip_offset = {
        command->scissor_rect.x * transform->scale_x + transform->offset_x,
        command->scissor_rect.y * transform->scale_y + transform->offset_y, 0.0f};
    ABI::Windows::Foundation::Numerics::Vector3 clip_scale = {transform->scale_x,
                                                              transform->scale_y, 1.0f};
    ABI::Windows::Foundation::Numerics::Vector2 clip_size = {command->scissor_rect.width,
                                                             command->scissor_rect.height};
    if (SUCCEEDED(hr))
    {
        hr = clip_base->put_Offset(clip_offset);
    }
    if (SUCCEEDED(hr))
    {
        hr = clip_base->put_Scale(clip_scale);
    }
    if (SUCCEEDED(hr))
    {
        hr = clip_base->put_Size(clip_size);
    }

    ComPtr<ABI::Windows::UI::Composition::IInsetClip> inset;
    if (SUCCEEDED(hr))
    {
        hr = backend->compositor->CreateInsetClipWithInsets(0.0f, 0.0f, 0.0f, 0.0f, &inset);
    }
    ComPtr<ABI::Windows::UI::Composition::ICompositionClip> clip;
    if (SUCCEEDED(hr))
    {
        hr = inset.As(&clip);
    }
    if (SUCCEEDED(hr))
    {
        hr = clip_base->put_Clip(clip.Get());
    }

    ComPtr<ABI::Windows::UI::Composition::IVisual> content_base;
    if (SUCCEEDED(hr))
    {
        hr = layer->content_visual.As(&content_base);
    }
    ABI::Windows::Foundation::Numerics::Vector2 content_size = {command->rect.width,
                                                                command->rect.height};
    if (SUCCEEDED(hr))
    {
        hr = content_base->put_Size(content_size);
    }
    return hr;
}

static HRESULT reach_d2d_start_animated_text(reach_render_backend *backend,
                                             reach_d2d_animated_text_layer *layer,
                                             const reach_render_command *command)
{
    ComPtr<ABI::Windows::UI::Composition::IVisual> content_base;
    HRESULT hr = layer->content_visual.As(&content_base);
    ComPtr<ABI::Windows::UI::Composition::ICompositionObject> object;
    if (SUCCEEDED(hr))
    {
        hr = layer->content_visual.As(&object);
    }
    HSTRING property = nullptr;
    if (SUCCEEDED(hr))
    {
        hr = WindowsCreateString(L"Offset.X", 8, &property);
    }
    if (SUCCEEDED(hr))
    {
        (void)object->StopAnimation(property);
    }

    float base_x = command->rect.x - command->scissor_rect.x;
    ABI::Windows::Foundation::Numerics::Vector3 offset = {
        base_x, command->rect.y - command->scissor_rect.y, 0.0f};
    if (SUCCEEDED(hr))
    {
        hr = content_base->put_Offset(offset);
    }

    float hold = command->animation_hold_seconds;
    float travel = command->animation_travel_seconds;
    float cycle = (hold + travel) * 2.0f;
    if (SUCCEEDED(hr) && command->animation_offset_x != 0.0f && hold >= 0.0f && travel > 0.0f &&
        cycle > 0.0f)
    {
        ComPtr<ABI::Windows::UI::Composition::IScalarKeyFrameAnimation> scalar;
        hr = backend->compositor->CreateScalarKeyFrameAnimation(&scalar);
        ComPtr<ABI::Windows::UI::Composition::IKeyFrameAnimation> key_frames;
        if (SUCCEEDED(hr))
        {
            hr = scalar.As(&key_frames);
        }
        ABI::Windows::Foundation::TimeSpan duration = {};
        duration.Duration = (INT64)((double)cycle * 10000000.0);
        if (SUCCEEDED(hr))
        {
            hr = key_frames->put_Duration(duration);
        }
        if (SUCCEEDED(hr))
        {
            hr = key_frames->put_IterationBehavior(
                ABI::Windows::UI::Composition::AnimationIterationBehavior_Forever);
        }

        ABI::Windows::Foundation::Numerics::Vector2 control1 = {0.645f, 0.045f};
        ABI::Windows::Foundation::Numerics::Vector2 control2 = {0.355f, 1.0f};
        ComPtr<ABI::Windows::UI::Composition::ICubicBezierEasingFunction> cubic;
        if (SUCCEEDED(hr))
        {
            hr = backend->compositor->CreateCubicBezierEasingFunction(control1, control2, &cubic);
        }
        ComPtr<ABI::Windows::UI::Composition::ICompositionEasingFunction> easing;
        if (SUCCEEDED(hr))
        {
            hr = cubic.As(&easing);
        }

        float end_x = base_x + command->animation_offset_x;
        if (SUCCEEDED(hr))
        {
            hr = scalar->InsertKeyFrame(0.0f, base_x);
        }
        if (SUCCEEDED(hr))
        {
            hr = scalar->InsertKeyFrame(hold / cycle, base_x);
        }
        if (SUCCEEDED(hr))
        {
            hr = scalar->InsertKeyFrameWithEasingFunction((hold + travel) / cycle, end_x,
                                                          easing.Get());
        }
        if (SUCCEEDED(hr))
        {
            hr = scalar->InsertKeyFrame((hold * 2.0f + travel) / cycle, end_x);
        }
        if (SUCCEEDED(hr))
        {
            hr = scalar->InsertKeyFrameWithEasingFunction(1.0f, base_x, easing.Get());
        }
        ComPtr<ABI::Windows::UI::Composition::ICompositionAnimation> animation;
        if (SUCCEEDED(hr))
        {
            hr = scalar.As(&animation);
        }
        if (SUCCEEDED(hr))
        {
            hr = object->StartAnimation(property, animation.Get());
        }
    }
    if (property != nullptr)
    {
        WindowsDeleteString(property);
    }
    return hr;
}

reach_result reach_d2d_sync_animated_text(reach_render_backend *backend,
                                          const reach_render_command *command,
                                          const reach_transform_f32 *transform)
{
    if (backend == nullptr || command == nullptr || transform == nullptr || !command->has_scissor)
    {
        return REACH_NOT_IMPLEMENTED;
    }
    if (backend->compositor == nullptr || backend->composition_graphics_device == nullptr ||
        backend->root_visual == nullptr)
    {
        return reach_dcomp_sync_animated_text(backend, command, transform);
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
        HRESULT geometry_hr =
            reach_d2d_set_animated_text_geometry(backend, layer, command, transform);
        if (FAILED(geometry_hr))
        {
            reach_d2d_log_hresult(L"animated text geometry", geometry_hr);
            return REACH_ERROR;
        }
        layer->transform = *transform;
        return REACH_OK;
    }

    HRESULT hr = layer->clip_visual != nullptr && layer->content_visual != nullptr
                     ? S_OK
                     : reach_d2d_create_animated_text_visual(backend, layer);
    if (SUCCEEDED(hr))
    {
        hr = reach_d2d_draw_animated_text_surface(backend, layer, command);
    }
    if (SUCCEEDED(hr))
    {
        hr = reach_d2d_set_animated_text_geometry(backend, layer, command, transform);
    }
    if (SUCCEEDED(hr))
    {
        hr = reach_d2d_start_animated_text(backend, layer, command);
    }
    ComPtr<ABI::Windows::UI::Composition::IVisual> clip_base;
    if (SUCCEEDED(hr))
    {
        hr = layer->clip_visual.As(&clip_base);
    }
    if (SUCCEEDED(hr))
    {
        hr = clip_base->put_IsVisible(TRUE);
    }
    if (FAILED(hr))
    {
        reach_d2d_log_hresult(L"animated text layer", hr);
        return REACH_ERROR;
    }
    layer->command = *command;
    layer->transform = *transform;
    layer->has_command = 1;
    layer->visible = 1;
    return REACH_OK;
}

void reach_d2d_begin_animated_text_sync(reach_render_backend *backend)
{
    if (backend == nullptr)
    {
        return;
    }
    backend->animated_text_layer.seen = 0;
}

void reach_d2d_end_animated_text_sync(reach_render_backend *backend)
{
    if (backend == nullptr)
    {
        return;
    }
    reach_d2d_animated_text_layer &layer = backend->animated_text_layer;
    if (layer.seen || !layer.visible)
    {
        return;
    }
    if (layer.clip_visual != nullptr)
    {
        ComPtr<ABI::Windows::UI::Composition::IVisual> clip_base;
        if (SUCCEEDED(layer.clip_visual.As(&clip_base)))
        {
            (void)clip_base->put_IsVisible(FALSE);
        }
        layer.visible = 0;
    }
    else
    {
        reach_dcomp_hide_animated_text(backend);
    }
}

reach_result reach_wuc_apply_content_clip(reach_render_backend *backend,
                                          reach_rect_f32 content_rect)
{
    if (backend == nullptr || backend->backdrop_visual == nullptr)
    {
        return REACH_OK;
    }

    if (backend->backdrop_content_rect.x == content_rect.x &&
        backend->backdrop_content_rect.y == content_rect.y &&
        backend->backdrop_content_rect.width == content_rect.width &&
        backend->backdrop_content_rect.height == content_rect.height)
    {
        return REACH_OK;
    }

    float right = (float)backend->target_width - (content_rect.x + content_rect.width);
    float bottom = (float)backend->target_height - (content_rect.y + content_rect.height);

    ComPtr<ABI::Windows::UI::Composition::IInsetClip> inset;
    HRESULT hr = backend->compositor->CreateInsetClipWithInsets(
        content_rect.x, content_rect.y, right > 0.0f ? right : 0.0f, bottom > 0.0f ? bottom : 0.0f,
        &inset);

    ComPtr<ABI::Windows::UI::Composition::IVisual> backdrop;
    if (SUCCEEDED(hr))
    {
        hr = backend->backdrop_visual.As(&backdrop);
    }

    ComPtr<ABI::Windows::UI::Composition::ICompositionClip> clip;
    if (SUCCEEDED(hr))
    {
        hr = inset.As(&clip);
    }
    if (SUCCEEDED(hr))
    {
        hr = backdrop->put_Clip(clip.Get());
    }

    if (FAILED(hr))
    {
        reach_d2d_log_hresult(L"backdrop content clip", hr);
        return REACH_ERROR;
    }

    backend->backdrop_content_rect = content_rect;
    return REACH_OK;
}

reach_result reach_wuc_create_target(reach_render_backend *backend)
{
    if (backend == nullptr || backend->hwnd == nullptr || backend->factory == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    BOOL use_host_backdrop = TRUE;
    (void)DwmSetWindowAttribute(backend->hwnd, DWMWA_USE_HOSTBACKDROPBRUSH, &use_host_backdrop,
                                sizeof(use_host_backdrop));

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

    HRESULT hr = RoInitialize(RO_INIT_SINGLETHREADED);
    reach_d2d_log_hresult(L"RoInitialize", hr);

    if (SUCCEEDED(hr))
    {
        backend->ro_initialized = 1;
    }
    else if (hr == RPC_E_CHANGED_MODE)
    {
        hr = S_OK;
    }

    D3D_FEATURE_LEVEL actual_level = D3D_FEATURE_LEVEL_11_0;

    if (SUCCEEDED(hr))
    {
        hr = reach_d3d11_create_device(&backend->d3d_device, &actual_level);
        reach_d2d_log_hresult(L"D3D11CreateDevice", hr);
    }
    if (SUCCEEDED(hr))
    {
        hr = backend->d3d_device->QueryInterface(IID_PPV_ARGS(&backend->dxgi_device));
        reach_d2d_log_hresult(L"ID3D11Device::QueryInterface IDXGIDevice", hr);
    }
    if (SUCCEEDED(hr))
    {
        hr = backend->factory->CreateDevice(backend->dxgi_device, &backend->d2d_device);
        reach_d2d_log_hresult(L"ID2D1Factory1::CreateDevice", hr);
    }
    if (SUCCEEDED(hr))
    {
        hr = backend->d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                                      &backend->d2d_context);
        reach_d2d_log_hresult(L"ID2D1Device::CreateDeviceContext", hr);
    }
    if (SUCCEEDED(hr))
    {
        hr = reach_dcomp_create_swap_chain(backend, width, height) == REACH_OK ? S_OK : E_FAIL;
        reach_d2d_log_hresult(L"CreateSwapChainForComposition", hr);
    }
    if (SUCCEEDED(hr))
    {
        hr = reach_winrt_activate_compositor(&backend->compositor);
        static int32_t logged_compositor_activation_failure = 0;
        if (FAILED(hr) && !logged_compositor_activation_failure)
        {
            logged_compositor_activation_failure = 1;
            reach_d2d_log_hresult(L"RoActivateInstance(Compositor)", hr);
        }
    }

    ComPtr<ABI::Windows::UI::Composition::Desktop::ICompositorDesktopInterop> desktop_interop;
    if (SUCCEEDED(hr))
    {
        hr = backend->compositor.As(&desktop_interop);
        reach_d2d_log_hresult(L"ICompositorDesktopInterop", hr);
    }

    ComPtr<ABI::Windows::UI::Composition::Desktop::IDesktopWindowTarget> desktop_target;
    if (SUCCEEDED(hr))
    {
        hr = desktop_interop->CreateDesktopWindowTarget(backend->hwnd, TRUE, &desktop_target);
        reach_d2d_log_hresult(L"CreateDesktopWindowTarget", hr);
    }

    if (SUCCEEDED(hr))
    {
        hr = desktop_target.As(&backend->composition_target);
        reach_d2d_log_hresult(L"ICompositionTarget", hr);
    }
    if (SUCCEEDED(hr))
    {
        hr = backend->compositor->CreateContainerVisual(&backend->root_visual);
        reach_d2d_log_hresult(L"CreateContainerVisual", hr);
    }
    if (SUCCEEDED(hr))
    {
        hr = backend->compositor->CreateSpriteVisual(&backend->backdrop_visual);
    }

    ComPtr<ABI::Windows::UI::Composition::ICompositor3> compositor3;
    if (SUCCEEDED(hr))
    {
        hr = backend->compositor.As(&compositor3);
    }

    ComPtr<ABI::Windows::UI::Composition::ICompositionBackdropBrush> backdrop_brush;
    if (SUCCEEDED(hr))
    {
        hr = compositor3->CreateHostBackdropBrush(&backdrop_brush);
        reach_d2d_log_hresult(L"CreateHostBackdropBrush", hr);
    }

    ComPtr<ABI::Windows::UI::Composition::ICompositionBrush> backdrop_base_brush;
    if (SUCCEEDED(hr))
    {
        hr = backdrop_brush.As(&backdrop_base_brush);
    }
    if (SUCCEEDED(hr))
    {
        hr = backend->backdrop_visual->put_Brush(backdrop_base_brush.Get());
    }

    ComPtr<ABI::Windows::UI::Composition::ICompositorInterop> compositor_interop;
    if (SUCCEEDED(hr))
    {
        hr = backend->compositor.As(&compositor_interop);
    }
    if (SUCCEEDED(hr))
    {
        hr = compositor_interop->CreateGraphicsDevice(
            backend->d2d_device, &backend->composition_graphics_device);
        reach_d2d_log_hresult(L"CreateCompositionGraphicsDevice", hr);
    }
    if (SUCCEEDED(hr))
    {
        hr = compositor_interop->CreateCompositionSurfaceForSwapChain(
            backend->swap_chain, &backend->composition_surface);
        reach_d2d_log_hresult(L"CreateCompositionSurfaceForSwapChain", hr);
    }

    ComPtr<ABI::Windows::UI::Composition::ICompositionSurfaceBrush> surface_brush;
    if (SUCCEEDED(hr))
    {
        hr = backend->compositor->CreateSurfaceBrushWithSurface(backend->composition_surface.Get(),
                                                                &surface_brush);
    }

    if (SUCCEEDED(hr))
    {
        hr = backend->compositor->CreateSpriteVisual(&backend->swap_chain_visual);
    }

    ComPtr<ABI::Windows::UI::Composition::ICompositionBrush> surface_base_brush;
    if (SUCCEEDED(hr))
    {
        hr = surface_brush.As(&surface_base_brush);
    }
    if (SUCCEEDED(hr))
    {
        hr = backend->swap_chain_visual->put_Brush(surface_base_brush.Get());
    }

    ComPtr<ABI::Windows::UI::Composition::IVisualCollection> children;
    if (SUCCEEDED(hr))
    {
        hr = backend->root_visual->get_Children(&children);
    }

    ComPtr<ABI::Windows::UI::Composition::IVisual> backdrop_base_visual;
    if (SUCCEEDED(hr))
    {
        hr = backend->backdrop_visual.As(&backdrop_base_visual);
    }
    if (SUCCEEDED(hr))
    {
        hr = children->InsertAtTop(backdrop_base_visual.Get());
    }

    ComPtr<ABI::Windows::UI::Composition::IVisual> swap_chain_base_visual;
    if (SUCCEEDED(hr))
    {
        hr = backend->swap_chain_visual.As(&swap_chain_base_visual);
    }
    if (SUCCEEDED(hr))
    {
        hr = children->InsertAtTop(swap_chain_base_visual.Get());
    }

    ComPtr<ABI::Windows::UI::Composition::IVisual> root_base_visual;
    if (SUCCEEDED(hr))
    {
        hr = backend->root_visual.As(&root_base_visual);
    }
    if (SUCCEEDED(hr))
    {
        hr = backend->composition_target->put_Root(root_base_visual.Get());
    }

    if (SUCCEEDED(hr))
    {
        hr = reach_visual_set_size(root_base_visual.Get(), static_cast<float>(width),
                                   static_cast<float>(height));
    }
    if (SUCCEEDED(hr))
    {
        hr = reach_visual_set_size(backdrop_base_visual.Get(), static_cast<float>(width),
                                   static_cast<float>(height));
    }
    if (SUCCEEDED(hr))
    {
        hr = reach_visual_set_size(swap_chain_base_visual.Get(), static_cast<float>(width),
                                   static_cast<float>(height));
    }
    if (SUCCEEDED(hr))
    {
        hr = reach_dcomp_create_target_bitmap(backend) == REACH_OK ? S_OK : E_FAIL;
    }
    if (SUCCEEDED(hr))
    {
        backend->target_width = width;
        backend->target_height = height;
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}
