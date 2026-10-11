#include "render_d2d_internal.h"

#include <algorithm>
#include <math.h>
#include <wrl/event.h>

namespace
{
using namespace ABI::Windows::UI::Composition;
using ABI::Windows::Foundation::Numerics::Vector2;
using ABI::Windows::Foundation::Numerics::Vector3;

struct motion_segment
{
    double start;
    double end;
    float value;
    float linear;
    float quadratic;
    float cubic;
};

static float scalar_value(const reach_motion &motion, float fallback)
{
    return motion.generation == 0 ? fallback : reach_motion_value(&motion);
}

static size_t motion_segments(const reach_motion &motion, motion_segment *segments)
{
    double elapsed = std::max(0.0, reach_motion_now_seconds() - motion.started_seconds);
    if (motion.duration_seconds <= elapsed)
    {
        return 0;
    }
    double duration = motion.duration_seconds;
    double span = motion.to - motion.from;
    double split = motion.easing == REACH_EASING_EASE_IN_OUT ? duration * 0.5 : duration;
    size_t count = 0;
    double at = elapsed;
    while (at < duration && count < 2)
    {
        double t = at / duration;
        double end = at < split ? split : duration;
        double a = 0.0;
        double b = 0.0;
        double c = 0.0;
        double d = 0.0;
        if (motion.easing == REACH_EASING_EASE_IN)
        {
            d = 1.0;
        }
        else if (motion.easing == REACH_EASING_EASE_OUT)
        {
            b = 3.0;
            c = -3.0;
            d = 1.0;
        }
        else if (t < 0.5)
        {
            d = 4.0;
        }
        else
        {
            a = -3.0;
            b = 12.0;
            c = -12.0;
            d = 4.0;
        }
        segments[count++] = {at - elapsed,
                             end - elapsed,
                             (float)(motion.from + span * (a + b * t + c * t * t + d * t * t * t)),
                             (float)(span * (b + 2.0 * c * t + 3.0 * d * t * t) / duration),
                             (float)(span * (c + 3.0 * d * t) / (duration * duration)),
                             (float)(span * d / (duration * duration * duration))};
        at = end;
    }
    return count;
}

static HRESULT wuc_motion(reach_render_backend *backend, IInspectable *visual,
                          const wchar_t *property_name, const reach_motion &motion)
{
    ComPtr<ICompositionObject> object;
    HRESULT hr = visual->QueryInterface(IID_PPV_ARGS(&object));
    HSTRING property = nullptr;
    if (SUCCEEDED(hr))
    {
        hr = WindowsCreateString(property_name, (UINT32)wcslen(property_name), &property);
    }
    motion_segment segments[2] = {};
    size_t count = motion_segments(motion, segments);
    if (SUCCEEDED(hr))
    {
        hr = object->StopAnimation(property);
    }
    ComPtr<IScalarKeyFrameAnimation> scalar;
    ComPtr<IKeyFrameAnimation> keyframes;
    ComPtr<ICompositionAnimation> animation;
    double duration = count > 0 ? segments[count - 1].end : 0.0;
    if (SUCCEEDED(hr) && count > 0)
    {
        hr = backend->compositor->CreateScalarKeyFrameAnimation(&scalar);
    }
    if (SUCCEEDED(hr) && count > 0)
    {
        hr = scalar.As(&keyframes);
    }
    if (SUCCEEDED(hr) && count > 0)
    {
        ABI::Windows::Foundation::TimeSpan timespan = {
            (INT64)(std::max(0.001, duration) * 10000000.0)};
        hr = keyframes->put_Duration(timespan);
    }
    if (SUCCEEDED(hr) && count > 0)
    {
        hr = scalar->InsertKeyFrame(0.0f, segments[0].value);
    }
    for (size_t index = 0; index < count && SUCCEEDED(hr); ++index)
    {
        const motion_segment &segment = segments[index];
        double dt = segment.end - segment.start;
        float end_value = (float)(segment.value + segment.linear * dt +
                                  segment.quadratic * dt * dt + segment.cubic * dt * dt * dt);
        float distance = end_value - segment.value;
        float end_derivative =
            (float)(segment.linear + 2.0 * segment.quadratic * dt + 3.0 * segment.cubic * dt * dt);
        Vector2 control1 = {
            1.0f / 3.0f, distance != 0.0f ? (float)(segment.linear * dt / (3.0 * distance)) : 0.0f};
        Vector2 control2 = {
            2.0f / 3.0f,
            distance != 0.0f ? 1.0f - (float)(end_derivative * dt / (3.0 * distance)) : 1.0f};
        ComPtr<ICubicBezierEasingFunction> cubic;
        ComPtr<ICompositionEasingFunction> easing;
        hr = backend->compositor->CreateCubicBezierEasingFunction(control1, control2, &cubic);
        if (SUCCEEDED(hr))
        {
            hr = cubic.As(&easing);
        }
        if (SUCCEEDED(hr))
        {
            hr = scalar->InsertKeyFrameWithEasingFunction((float)(segment.end / duration),
                                                          end_value, easing.Get());
        }
    }
    if (SUCCEEDED(hr) && count > 0)
    {
        hr = scalar.As(&animation);
    }
    if (SUCCEEDED(hr) && count > 0)
    {
        hr = object->StartAnimation(property, animation.Get());
    }
    if (property != nullptr)
    {
        WindowsDeleteString(property);
    }
    return hr;
}

static HRESULT dcomp_motion(reach_render_backend *backend, const reach_motion &motion,
                            ComPtr<IDCompositionAnimation> *animation)
{
    motion_segment segments[2] = {};
    size_t count = motion_segments(motion, segments);
    if (count == 0)
    {
        return S_FALSE;
    }
    HRESULT hr = backend->dcomp_device->CreateAnimation(animation->ReleaseAndGetAddressOf());
    for (size_t index = 0; index < count && SUCCEEDED(hr); ++index)
    {
        const motion_segment &segment = segments[index];
        hr = (*animation)
                 ->AddCubic(segment.start, segment.value, segment.linear, segment.quadratic,
                            segment.cubic);
    }
    if (SUCCEEDED(hr))
    {
        hr = (*animation)->End(segments[count - 1].end, motion.to);
    }
    return hr;
}

static HRESULT wuc_properties(reach_render_backend *backend, IInspectable *inspectable,
                              const reach_render_group &group, float base_x, float base_y,
                              bool preserve_repeat = false, unsigned mask = 31)
{
    ComPtr<IVisual> visual;
    HRESULT hr = inspectable->QueryInterface(IID_PPV_ARGS(&visual));
    if (SUCCEEDED(hr))
    {
        hr = visual->put_Offset(
            {base_x + scalar_value(group.x, 0.0f), base_y + scalar_value(group.y, 0.0f), 0.0f});
    }
    if (SUCCEEDED(hr))
    {
        if (mask & 4)
            hr = visual->put_Opacity(scalar_value(group.opacity, 1.0f));
    }
    if (SUCCEEDED(hr))
    {
        float scale = scalar_value(group.scale, 1.0f);
        if (mask & 24)
            hr = visual->put_Scale({scale, scale, 1.0f});
    }
    reach_motion x = group.x;
    x.from += base_x;
    x.to += base_x;
    reach_motion y = group.y;
    y.from += base_y;
    y.to += base_y;
    if (SUCCEEDED(hr) && !preserve_repeat && (mask & 1))
        hr = wuc_motion(backend, inspectable, L"Offset.X", x);
    if (SUCCEEDED(hr) && (mask & 2))
        hr = wuc_motion(backend, inspectable, L"Offset.Y", y);
    if (SUCCEEDED(hr) && (mask & 4))
        hr = wuc_motion(backend, inspectable, L"Opacity", group.opacity);
    if (SUCCEEDED(hr) && (mask & 8))
        hr = wuc_motion(backend, inspectable, L"Scale.X", group.scale);
    if (SUCCEEDED(hr) && (mask & 16))
        hr = wuc_motion(backend, inspectable, L"Scale.Y", group.scale);
    return hr;
}

static HRESULT dcomp_properties(reach_render_backend *backend, IDCompositionVisual *visual,
                                IDCompositionScaleTransform *scale,
                                IDCompositionEffectGroup *opacity, const reach_render_group &group,
                                float base_x, float base_y, bool preserve_repeat = false,
                                unsigned mask = 31)
{
    HRESULT hr = preserve_repeat || !(mask & 1)
                     ? S_OK
                     : visual->SetOffsetX(base_x + scalar_value(group.x, 0.0f));
    if (SUCCEEDED(hr) && (mask & 2))
        hr = visual->SetOffsetY(base_y + scalar_value(group.y, 0.0f));
    if (SUCCEEDED(hr) && (mask & 4))
        hr = opacity->SetOpacity(scalar_value(group.opacity, 1.0f));
    if (SUCCEEDED(hr) && (mask & 8))
        hr = scale->SetScaleX(scalar_value(group.scale, 1.0f));
    if (SUCCEEDED(hr) && (mask & 16))
        hr = scale->SetScaleY(scalar_value(group.scale, 1.0f));
    reach_motion motions[5] = {group.x, group.y, group.opacity, group.scale, group.scale};
    motions[0].from += base_x;
    motions[0].to += base_x;
    motions[1].from += base_y;
    motions[1].to += base_y;
    for (size_t index = 0; index < 5 && SUCCEEDED(hr); ++index)
    {
        if ((mask & (1u << index)) == 0 || (index == 0 && preserve_repeat))
            continue;
        ComPtr<IDCompositionAnimation> animation;
        HRESULT motion_hr = dcomp_motion(backend, motions[index], &animation);
        if (FAILED(motion_hr))
            return motion_hr;
        if (motion_hr == S_FALSE)
            continue;
        switch (index)
        {
        case 0:
            hr = visual->SetOffsetX(animation.Get());
            break;
        case 1:
            hr = visual->SetOffsetY(animation.Get());
            break;
        case 2:
            hr = opacity->SetOpacity(animation.Get());
            break;
        case 3:
            hr = scale->SetScaleX(animation.Get());
            break;
        case 4:
            hr = scale->SetScaleY(animation.Get());
            break;
        }
    }
    return hr;
}

static unsigned property_mask(const reach_render_group &group, const reach_render_group &previous,
                              float base_x, float base_y, bool initialized)
{
    if (!initialized)
        return 31;
    float previous_x = previous.has_clip ? previous.bounds.x - previous.clip.x : 0.0f;
    float previous_y = previous.has_clip ? previous.bounds.y - previous.clip.y : 0.0f;
    unsigned mask = 0;
    if (memcmp(&group.x, &previous.x, sizeof(reach_motion)) != 0 || base_x != previous_x)
        mask |= 1;
    if (memcmp(&group.y, &previous.y, sizeof(reach_motion)) != 0 || base_y != previous_y)
        mask |= 2;
    if (memcmp(&group.opacity, &previous.opacity, sizeof(reach_motion)) != 0)
        mask |= 4;
    if (memcmp(&group.scale, &previous.scale, sizeof(reach_motion)) != 0)
        mask |= 24;
    return mask;
}

static void retire_swap_chain(reach_render_backend *backend)
{
    backend->composition_surface.Reset();
    backend->swap_chain_visual.Reset();
    if (backend->d2d_context != nullptr)
        backend->d2d_context->SetTarget(nullptr);
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
}

static HRESULT create_scene(reach_render_backend *backend)
{
    if (backend->scene_visual != nullptr || backend->dcomp_scene_visual != nullptr)
        return S_OK;
    if (backend->compositor != nullptr)
    {
        HRESULT hr = backend->compositor->CreateContainerVisual(&backend->scene_visual);
        ComPtr<IVisualCollection> children;
        ComPtr<IVisual> scene;
        ComPtr<IVisual> old;
        if (SUCCEEDED(hr))
            hr = backend->root_visual->get_Children(&children);
        if (SUCCEEDED(hr))
            hr = backend->scene_visual.As(&scene);
        if (SUCCEEDED(hr) && backend->swap_chain_visual != nullptr)
        {
            hr = backend->swap_chain_visual.As(&old);
            if (SUCCEEDED(hr))
                (void)children->Remove(old.Get());
        }
        if (SUCCEEDED(hr))
            hr = children->InsertAtTop(scene.Get());
        if (SUCCEEDED(hr))
            retire_swap_chain(backend);
        return hr;
    }
    HRESULT hr = backend->dcomp_device->CreateVisual(&backend->dcomp_scene_visual);
    if (SUCCEEDED(hr))
        hr = backend->dcomp_visual->SetContent(nullptr);
    if (SUCCEEDED(hr))
        hr = backend->dcomp_visual->AddVisual(backend->dcomp_scene_visual.Get(), TRUE, nullptr);
    if (SUCCEEDED(hr))
        hr = backend->dcomp_device->CreateScaleTransform(&backend->scene_scale);
    if (SUCCEEDED(hr))
        hr = backend->dcomp_device->CreateEffectGroup(&backend->scene_opacity);
    if (SUCCEEDED(hr))
        hr = backend->dcomp_visual->SetEffect(backend->scene_opacity.Get());
    if (SUCCEEDED(hr))
        hr = backend->dcomp_visual->SetTransform(backend->scene_scale.Get());
    if (SUCCEEDED(hr))
        retire_swap_chain(backend);
    return hr;
}

static HRESULT create_layer(reach_render_backend *backend, reach_d2d_retained_layer &layer)
{
    if (backend->compositor != nullptr)
    {
        HRESULT hr = backend->compositor->CreateContainerVisual(&layer.container);
        if (SUCCEEDED(hr))
            hr = backend->compositor->CreateSpriteVisual(&layer.visual);
        ComPtr<IVisualCollection> children;
        ComPtr<IVisual> visual;
        if (SUCCEEDED(hr))
            hr = layer.container->get_Children(&children);
        if (SUCCEEDED(hr))
            hr = layer.visual.As(&visual);
        if (SUCCEEDED(hr))
            hr = children->InsertAtTop(visual.Get());
        return hr;
    }
    HRESULT hr = backend->dcomp_device->CreateVisual(&layer.dcomp_container);
    if (SUCCEEDED(hr))
        hr = backend->dcomp_device->CreateVisual(&layer.dcomp_visual);
    if (SUCCEEDED(hr))
        hr = layer.dcomp_container->AddVisual(layer.dcomp_visual.Get(), TRUE, nullptr);
    if (SUCCEEDED(hr))
        hr = backend->dcomp_device->CreateScaleTransform(&layer.dcomp_scale);
    if (SUCCEEDED(hr))
        hr = backend->dcomp_device->CreateEffectGroup(&layer.dcomp_opacity);
    if (SUCCEEDED(hr))
        hr = layer.dcomp_visual->SetEffect(layer.dcomp_opacity.Get());
    if (SUCCEEDED(hr))
        hr = layer.dcomp_visual->SetTransform(layer.dcomp_scale.Get());
    return hr;
}

static HRESULT draw_layer(reach_render_backend *backend, reach_d2d_retained_layer &layer,
                          const std::vector<reach_render_command> &commands, UINT width,
                          UINT height)
{
    HRESULT hr = S_OK;
    ComPtr<ID2D1DeviceContext> context;
    POINT offset = {};
    ComPtr<ICompositionDrawingSurfaceInterop> interop;
    bool resized = width != layer.width || height != layer.height;
    if (backend->compositor != nullptr)
    {
        if (resized || layer.surface == nullptr)
        {
            layer.surface.Reset();
            hr = backend->composition_graphics_device->CreateDrawingSurface(
                {(float)width, (float)height},
                ABI::Windows::Graphics::DirectX::DirectXPixelFormat_B8G8R8A8UIntNormalized,
                ABI::Windows::Graphics::DirectX::DirectXAlphaMode_Premultiplied, &layer.surface);
            ComPtr<ICompositionSurface> surface;
            ComPtr<ICompositionSurfaceBrush> brush;
            ComPtr<ICompositionBrush> base;
            if (SUCCEEDED(hr))
                hr = layer.surface.As(&surface);
            if (SUCCEEDED(hr))
                hr = backend->compositor->CreateSurfaceBrushWithSurface(surface.Get(), &brush);
            if (SUCCEEDED(hr))
                hr = brush->put_Stretch(CompositionStretch_None);
            if (SUCCEEDED(hr))
                hr = brush.As(&base);
            if (SUCCEEDED(hr))
                hr = layer.visual->put_Brush(base.Get());
        }
        if (SUCCEEDED(hr))
            hr = layer.surface.As(&interop);
        if (SUCCEEDED(hr))
            hr = interop->BeginDraw(nullptr, IID_PPV_ARGS(&context), &offset);
    }
    else
    {
        if (resized || layer.dcomp_surface == nullptr)
        {
            layer.dcomp_surface.Reset();
            hr = backend->dcomp_device->CreateSurface(width, height, DXGI_FORMAT_B8G8R8A8_UNORM,
                                                      DXGI_ALPHA_MODE_PREMULTIPLIED,
                                                      &layer.dcomp_surface);
            if (SUCCEEDED(hr))
                hr = layer.dcomp_visual->SetContent(layer.dcomp_surface.Get());
        }
        if (SUCCEEDED(hr))
            hr = layer.dcomp_surface->BeginDraw(nullptr, IID_PPV_ARGS(&context), &offset);
    }
    if (FAILED(hr))
        return hr;
    context->SetTransform(D2D1::Matrix3x2F::Translation((float)offset.x, (float)offset.y));
    context->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
    ID2D1DeviceContext *previous = backend->d2d_context;
    backend->d2d_context = context.Get();
    reach_result result = reach_d2d_execute_commands(backend, commands.data(), commands.size());
    backend->d2d_context = previous;
    HRESULT end_hr = interop != nullptr ? interop->EndDraw() : layer.dcomp_surface->EndDraw();
    if (result != REACH_OK)
        return E_FAIL;
    if (FAILED(end_hr))
        return end_hr;
    layer.width = width;
    layer.height = height;
    layer.commands = commands;
    return S_OK;
}

static HRESULT repeat_motion(reach_render_backend *backend, reach_d2d_retained_layer &layer,
                             const reach_render_group &group, float base_x)
{
    float hold = group.repeat_hold_seconds;
    float travel = group.repeat_travel_seconds;
    float cycle = 2.0f * (hold + travel);
    if (travel <= 0.0f || hold < 0.0f || group.repeat_offset_x == 0.0f)
        return S_OK;
    if (backend->compositor != nullptr)
    {
        ComPtr<IScalarKeyFrameAnimation> scalar;
        ComPtr<IKeyFrameAnimation> keyframes;
        ComPtr<ICompositionAnimation> animation;
        ComPtr<ICompositionObject> object;
        ComPtr<ICubicBezierEasingFunction> cubic;
        ComPtr<ICompositionEasingFunction> easing;
        HRESULT hr = backend->compositor->CreateScalarKeyFrameAnimation(&scalar);
        if (SUCCEEDED(hr))
            hr = scalar.As(&keyframes);
        if (SUCCEEDED(hr))
            hr = keyframes->put_Duration({(INT64)((double)cycle * 10000000.0)});
        if (SUCCEEDED(hr))
            hr = keyframes->put_IterationBehavior(AnimationIterationBehavior_Forever);
        if (SUCCEEDED(hr))
            hr = backend->compositor->CreateCubicBezierEasingFunction({0.645f, 0.045f},
                                                                      {0.355f, 1.0f}, &cubic);
        if (SUCCEEDED(hr))
            hr = cubic.As(&easing);
        if (SUCCEEDED(hr))
            hr = scalar->InsertKeyFrame(0.0f, base_x);
        if (SUCCEEDED(hr) && hold > 0.0f)
            hr = scalar->InsertKeyFrame(hold / cycle, base_x);
        if (SUCCEEDED(hr))
            hr = scalar->InsertKeyFrameWithEasingFunction(
                (hold + travel) / cycle, base_x + group.repeat_offset_x, easing.Get());
        if (SUCCEEDED(hr) && hold > 0.0f)
            hr = scalar->InsertKeyFrame((hold * 2.0f + travel) / cycle,
                                        base_x + group.repeat_offset_x);
        if (SUCCEEDED(hr))
            hr = scalar->InsertKeyFrameWithEasingFunction(1.0f, base_x, easing.Get());
        if (SUCCEEDED(hr))
            hr = scalar.As(&animation);
        if (SUCCEEDED(hr))
            hr = layer.visual.As(&object);
        HSTRING property = nullptr;
        if (SUCCEEDED(hr))
            hr = WindowsCreateString(L"Offset.X", 8, &property);
        if (SUCCEEDED(hr))
            hr = object->StartAnimation(property, animation.Get());
        if (property != nullptr)
            WindowsDeleteString(property);
        return hr;
    }
    ComPtr<IDCompositionAnimation> animation;
    HRESULT hr = backend->dcomp_device->CreateAnimation(&animation);
    float inverse = 1.0f / travel;
    float quadratic = 3.0f * group.repeat_offset_x * inverse * inverse;
    float cubic = -2.0f * group.repeat_offset_x * inverse * inverse * inverse;
    if (SUCCEEDED(hr) && hold > 0.0f)
        hr = animation->AddCubic(0.0, base_x, 0.0f, 0.0f, 0.0f);
    if (SUCCEEDED(hr))
        hr = animation->AddCubic(hold, base_x, 0.0f, quadratic, cubic);
    if (SUCCEEDED(hr) && hold > 0.0f)
        hr = animation->AddCubic(hold + travel, base_x + group.repeat_offset_x, 0.0f, 0.0f, 0.0f);
    if (SUCCEEDED(hr))
        hr = animation->AddCubic(hold * 2.0f + travel, base_x + group.repeat_offset_x, 0.0f,
                                 -quadratic, -cubic);
    if (SUCCEEDED(hr))
        hr = animation->AddRepeat(cycle, cycle);
    if (SUCCEEDED(hr))
        hr = layer.dcomp_visual->SetOffsetX(animation.Get());
    return hr;
}

static reach_rect_f32 command_bounds(const reach_render_command &command)
{
    reach_rect_f32 rect = command.rect;
    if (command.type == REACH_RENDER_COMMAND_SHADOW)
    {
        float margin = ceilf(command.blur_radius * 3.0f) + 2.0f;
        rect.x += command.shadow_offset_x - margin;
        rect.y += command.shadow_offset_y - margin;
        rect.width += margin * 2.0f;
        rect.height += margin * 2.0f;
    }
    if (command.has_scissor)
    {
        float right =
            std::min(rect.x + rect.width, command.scissor_rect.x + command.scissor_rect.width);
        float bottom =
            std::min(rect.y + rect.height, command.scissor_rect.y + command.scissor_rect.height);
        rect.x = std::max(rect.x, command.scissor_rect.x);
        rect.y = std::max(rect.y, command.scissor_rect.y);
        rect.width = std::max(0.0f, right - rect.x);
        rect.height = std::max(0.0f, bottom - rect.y);
    }
    return rect;
}

static reach_rect_f32 run_bounds(const reach_render_command *commands, size_t count)
{
    reach_rect_f32 bounds = {};
    bool has_bounds = false;
    for (size_t index = 0; index < count; ++index)
    {
        reach_rect_f32 rect = command_bounds(commands[index]);
        if (rect.width <= 0.0f || rect.height <= 0.0f)
            continue;
        if (!has_bounds)
        {
            bounds = rect;
            has_bounds = true;
        }
        else
        {
            float right = std::max(bounds.x + bounds.width, rect.x + rect.width);
            float bottom = std::max(bounds.y + bounds.height, rect.y + rect.height);
            bounds.x = std::min(bounds.x, rect.x);
            bounds.y = std::min(bounds.y, rect.y);
            bounds.width = right - bounds.x;
            bounds.height = bottom - bounds.y;
        }
    }
    float right = ceilf(bounds.x + bounds.width);
    float bottom = ceilf(bounds.y + bounds.height);
    bounds.x = floorf(bounds.x);
    bounds.y = floorf(bounds.y);
    bounds.width = right - bounds.x;
    bounds.height = bottom - bounds.y;
    return bounds;
}

static void localize(reach_render_command &command, reach_rect_f32 bounds)
{
    command.rect.x -= bounds.x;
    command.rect.y -= bounds.y;
    if (command.has_scissor)
    {
        command.scissor_rect.x -= bounds.x;
        command.scissor_rect.y -= bounds.y;
    }
    if (command.has_clip_rect || command.type == REACH_RENDER_COMMAND_CLIPPED_ROUNDED_RECT)
    {
        command.clip_rect.x -= bounds.x;
        command.clip_rect.y -= bounds.y;
    }
    if (command.type == REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT ||
        command.type == REACH_RENDER_COMMAND_SHADOW)
    {
        command.notch_center_x -= bounds.x;
    }
}

static HRESULT sync_layer(reach_render_backend *backend, reach_d2d_retained_layer &layer,
                          const reach_render_group &group, const reach_render_command *commands,
                          size_t count)
{
    if (!isfinite(group.bounds.width) || !isfinite(group.bounds.height) ||
        group.bounds.width > 16384.0f || group.bounds.height > 16384.0f)
        return E_INVALIDARG;
    UINT width = (UINT)std::max(1.0f, ceilf(group.bounds.width));
    UINT height = (UINT)std::max(1.0f, ceilf(group.bounds.height));
    uint64_t bytes = (uint64_t)width * height * 4;
    for (const auto &other : backend->retained_layers)
        if (&other != &layer)
            bytes += (uint64_t)other.width * other.height * 4;
    if (bytes > UINT64_C(128) * 1024 * 1024)
        return E_OUTOFMEMORY;
    std::vector<reach_render_command> local(commands, commands + count);
    for (auto &command : local)
        localize(command, group.bounds);
    bool initialized = layer.width != 0 && layer.height != 0;
    bool content_changed =
        width != layer.width || height != layer.height || local.size() != layer.commands.size() ||
        (!local.empty() && memcmp(local.data(), layer.commands.data(),
                                  local.size() * sizeof(reach_render_command)) != 0);
    HRESULT hr = content_changed ? draw_layer(backend, layer, local, width, height) : S_OK;
    bool geometry_changed = memcmp(&group, &layer.group, sizeof(group)) != 0;
    float anchor_x = group.has_clip ? group.clip.x : group.bounds.x;
    float anchor_y = group.has_clip ? group.clip.y : group.bounds.y;
    float base_x = group.bounds.x - anchor_x;
    float base_y = group.bounds.y - anchor_y;
    float old_base_x = layer.group.has_clip ? layer.group.bounds.x - layer.group.clip.x : 0.0f;
    bool repeat_changed = layer.group.repeat_offset_x != group.repeat_offset_x ||
                          layer.group.repeat_hold_seconds != group.repeat_hold_seconds ||
                          layer.group.repeat_travel_seconds != group.repeat_travel_seconds ||
                          old_base_x != base_x ||
                          (content_changed && group.repeat_travel_seconds > 0.0f);
    bool preserve_repeat = group.repeat_travel_seconds > 0.0f && !repeat_changed;
    unsigned mask = property_mask(group, layer.group, base_x, base_y, initialized);
    if (layer.group.repeat_travel_seconds > 0.0f && group.repeat_travel_seconds <= 0.0f)
        mask |= 1;
    if (SUCCEEDED(hr) && geometry_changed && backend->compositor != nullptr)
    {
        ComPtr<IVisual> container;
        ComPtr<IVisual> visual;
        hr = layer.container.As(&container);
        if (SUCCEEDED(hr))
            hr = layer.visual.As(&visual);
        if (SUCCEEDED(hr))
            hr = container->put_Offset({anchor_x, anchor_y, 0.0f});
        if (SUCCEEDED(hr))
            hr = visual->put_Size({(float)width, (float)height});
        if (SUCCEEDED(hr) && group.has_clip)
        {
            hr = container->put_Size({group.clip.width, group.clip.height});
            ComPtr<IInsetClip> inset;
            ComPtr<ICompositionClip> clip;
            if (SUCCEEDED(hr))
                hr = backend->compositor->CreateInsetClipWithInsets(0.0f, 0.0f, 0.0f, 0.0f, &inset);
            if (SUCCEEDED(hr))
                hr = inset.As(&clip);
            if (SUCCEEDED(hr))
                hr = container->put_Clip(clip.Get());
        }
        else if (SUCCEEDED(hr))
            hr = container->put_Clip(nullptr);
        if (SUCCEEDED(hr))
            hr = wuc_properties(backend, layer.visual.Get(), group, base_x, base_y, preserve_repeat,
                                mask);
    }
    else if (SUCCEEDED(hr) && geometry_changed)
    {
        hr = layer.dcomp_container->SetOffsetX(anchor_x);
        if (SUCCEEDED(hr))
            hr = layer.dcomp_container->SetOffsetY(anchor_y);
        if (SUCCEEDED(hr) && group.has_clip)
        {
            D2D_RECT_F clip = {0.0f, 0.0f, group.clip.width, group.clip.height};
            hr = layer.dcomp_container->SetClip(clip);
        }
        else if (SUCCEEDED(hr))
            hr = layer.dcomp_container->SetClip((IDCompositionClip *)nullptr);
        if (SUCCEEDED(hr))
            hr = dcomp_properties(backend, layer.dcomp_visual.Get(), layer.dcomp_scale.Get(),
                                  layer.dcomp_opacity.Get(), group, base_x, base_y, preserve_repeat,
                                  mask);
    }
    if (SUCCEEDED(hr) && repeat_changed && group.repeat_travel_seconds > 0.0f)
    {
        hr = repeat_motion(backend, layer, group, base_x);
    }
    if (SUCCEEDED(hr))
    {
        layer.group = group;
        layer.seen = 1;
    }
    return hr;
}
}

int32_t reach_d2d_retained_supported(const reach_render_backend *backend)
{
    return backend != nullptr &&
           ((backend->compositor != nullptr && backend->composition_graphics_device != nullptr) ||
            backend->dcomp_device != nullptr);
}

void reach_d2d_release_retained(reach_render_backend *backend)
{
    if (backend == nullptr)
        return;
    if (backend->presentation_batch != nullptr)
        (void)backend->presentation_batch->remove_Completed(backend->presentation_token);
    backend->presentation_batch.Reset();
    backend->presentation_completed.reset();
    backend->submitted_presentation_generation = 0;
    backend->reconciled_presentation_generation = 0;
    if (backend->root_visual != nullptr)
    {
        ComPtr<ABI::Windows::UI::Composition::ICompositionObject> root;
        if (SUCCEEDED(backend->root_visual.As(&root)))
        {
            const wchar_t *properties[] = {L"Offset.X", L"Offset.Y", L"Opacity", L"Scale.X",
                                           L"Scale.Y"};
            for (const wchar_t *property_name : properties)
            {
                HSTRING property = nullptr;
                if (SUCCEEDED(WindowsCreateString(property_name, (UINT32)wcslen(property_name),
                                                  &property)))
                {
                    (void)root->StopAnimation(property);
                    WindowsDeleteString(property);
                }
            }
        }
    }
    if (backend->root_visual != nullptr && backend->scene_visual != nullptr)
    {
        ComPtr<ABI::Windows::UI::Composition::IVisualCollection> children;
        ComPtr<ABI::Windows::UI::Composition::IVisual> scene;
        if (SUCCEEDED(backend->root_visual->get_Children(&children)) &&
            SUCCEEDED(backend->scene_visual.As(&scene)))
            (void)children->Remove(scene.Get());
    }
    if (backend->dcomp_visual != nullptr && backend->dcomp_scene_visual != nullptr)
    {
        (void)backend->dcomp_visual->RemoveVisual(backend->dcomp_scene_visual.Get());
        (void)backend->dcomp_visual->SetTransform((IDCompositionTransform *)nullptr);
        (void)backend->dcomp_visual->SetEffect(nullptr);
        (void)backend->dcomp_visual->SetOffsetX(0.0f);
        (void)backend->dcomp_visual->SetOffsetY(0.0f);
    }
    backend->retained_layers.clear();
    backend->scene_visual.Reset();
    backend->dcomp_scene_visual.Reset();
    backend->scene_scale.Reset();
    backend->scene_opacity.Reset();
    backend->has_scene_presentation = 0;
}

void reach_d2d_suspend(reach_render_backend *backend)
{
    reach_d2d_release_retained(backend);
    if (backend != nullptr && backend->dcomp_device != nullptr)
        (void)backend->dcomp_device->Commit();
}

int32_t reach_d2d_presentation_complete(reach_render_backend *backend, uint64_t generation)
{
    if (backend == nullptr || !reach_d2d_retained_supported(backend))
        return 1;
    if (generation != backend->submitted_presentation_generation)
        return 1;
    if (backend->presentation_completed != nullptr)
        return backend->presentation_completed->load() == generation;
    if (reach_motion_active(&backend->scene_presentation.opacity) ||
        reach_motion_active(&backend->scene_presentation.x) ||
        reach_motion_active(&backend->scene_presentation.y) ||
        reach_motion_active(&backend->scene_presentation.scale))
        return 0;
    if (backend->reconciled_presentation_generation != generation)
    {
        if (backend->dcomp_device != nullptr)
        {
            reach_render_group settled = backend->scene_presentation;
            settled.x.duration_seconds = settled.y.duration_seconds = 0.0;
            settled.opacity.duration_seconds = settled.scale.duration_seconds = 0.0;
            if (FAILED(dcomp_properties(backend, backend->dcomp_visual, backend->scene_scale.Get(),
                                        backend->scene_opacity.Get(), settled, 0.0f, 0.0f)) ||
                FAILED(backend->dcomp_device->Commit()))
                return 0;
        }
        if (reach_d2d_synchronize(backend) != REACH_OK)
            return 0;
        backend->reconciled_presentation_generation = generation;
    }
    return 1;
}

reach_result reach_d2d_sync_retained(reach_render_backend *backend,
                                     const reach_render_command_buffer *commands)
{
    HRESULT hr = create_scene(backend);
    if (FAILED(hr))
    {
        reach_d2d_release_retained(backend);
        return REACH_ERROR;
    }
    if (reach_wuc_apply_content_clip(backend, commands->content_rect) != REACH_OK)
        return REACH_ERROR;
    ComPtr<IVisualCollection> children;
    if (backend->scene_visual != nullptr)
    {
        hr = backend->scene_visual->get_Children(&children);
        if (SUCCEEDED(hr))
            hr = children->RemoveAll();
        ComPtr<IVisual> scene;
        if (SUCCEEDED(hr))
            hr = backend->scene_visual.As(&scene);
        if (SUCCEEDED(hr))
        {
            ABI::Windows::Foundation::Numerics::Matrix4x4 matrix = {};
            matrix.M11 = commands->content_transform.scale_x;
            matrix.M22 = commands->content_transform.scale_y;
            matrix.M33 = 1.0f;
            matrix.M44 = 1.0f;
            matrix.M41 = commands->content_transform.offset_x;
            matrix.M42 = commands->content_transform.offset_y;
            hr = scene->put_TransformMatrix(matrix);
        }
    }
    else
    {
        hr = backend->dcomp_scene_visual->RemoveAllVisuals();
        D2D_MATRIX_3X2_F matrix = {commands->content_transform.scale_x,
                                   0.0f,
                                   0.0f,
                                   commands->content_transform.scale_y,
                                   commands->content_transform.offset_x,
                                   commands->content_transform.offset_y};
        if (SUCCEEDED(hr))
            hr = backend->dcomp_scene_visual->SetTransform(matrix);
    }
    std::vector<uint64_t> active_ids;
    for (size_t index = 0; index < commands->count;)
    {
        uint64_t id = commands->commands[index].group_id;
        active_ids.push_back(id != 0 ? id : UINT64_C(0x8000000000000000) | (uint64_t)(index + 1));
        size_t end = index + 1;
        while (end < commands->count && commands->commands[end].group_id == id)
            ++end;
        index = end;
    }
    backend->retained_layers.erase(
        std::remove_if(backend->retained_layers.begin(), backend->retained_layers.end(),
                       [&](const reach_d2d_retained_layer &layer)
                       {
                           return std::find(active_ids.begin(), active_ids.end(), layer.id) ==
                                  active_ids.end();
                       }),
        backend->retained_layers.end());
    for (auto &layer : backend->retained_layers)
        layer.seen = 0;
    for (size_t index = 0; index < commands->count && SUCCEEDED(hr);)
    {
        const reach_render_command &first = commands->commands[index];
        size_t end = index + 1;
        reach_render_group group = {};
        group.id = first.group_id;
        if (group.id != 0)
        {
            while (end < commands->count && commands->commands[end].group_id == group.id)
                ++end;
            auto found = std::find_if(commands->groups, commands->groups + commands->group_count,
                                      [&](const reach_render_group &candidate)
                                      { return candidate.id == group.id; });
            if (found == commands->groups + commands->group_count)
                return REACH_INVALID_ARGUMENT;
            group = *found;
            if (group.bounds.width <= 0.0f || group.bounds.height <= 0.0f)
                group.bounds = run_bounds(commands->commands + index, end - index);
        }
        else
        {
            while (end < commands->count && commands->commands[end].group_id == 0)
                ++end;
            group.id = UINT64_C(0x8000000000000000) | (uint64_t)(index + 1);
            group.bounds = run_bounds(commands->commands + index, end - index);
        }
        if (group.bounds.width <= 0.0f || group.bounds.height <= 0.0f)
        {
            index = end;
            continue;
        }
        auto found = std::find_if(backend->retained_layers.begin(), backend->retained_layers.end(),
                                  [&](const reach_d2d_retained_layer &layer)
                                  { return layer.id == group.id; });
        if (found == backend->retained_layers.end())
        {
            if (backend->retained_layers.size() >= REACH_MAX_RENDER_GROUPS * 2 + 1)
                return REACH_ERROR;
            backend->retained_layers.emplace_back();
            found = backend->retained_layers.end() - 1;
            found->id = group.id;
            hr = create_layer(backend, *found);
        }
        if (SUCCEEDED(hr))
            hr = sync_layer(backend, *found, group, commands->commands + index, end - index);
        if (SUCCEEDED(hr) && children != nullptr)
        {
            ComPtr<IVisual> visual;
            hr = found->container.As(&visual);
            if (SUCCEEDED(hr))
                hr = children->InsertAtTop(visual.Get());
        }
        else if (SUCCEEDED(hr))
            hr = backend->dcomp_scene_visual->AddVisual(found->dcomp_container.Get(), FALSE,
                                                        nullptr);
        index = end;
    }
    backend->retained_layers.erase(
        std::remove_if(backend->retained_layers.begin(), backend->retained_layers.end(),
                       [](const reach_d2d_retained_layer &layer) { return !layer.seen; }),
        backend->retained_layers.end());
    reach_render_group presentation =
        commands->has_presentation ? commands->presentation : reach_render_group{};
    if (SUCCEEDED(hr) &&
        (!backend->has_scene_presentation ||
         memcmp(&presentation, &backend->scene_presentation, sizeof(presentation)) != 0))
    {
        bool animation_changed =
            !backend->has_scene_presentation ||
            presentation.opacity.generation != backend->scene_presentation.opacity.generation;
        if (animation_changed)
        {
            if (backend->presentation_batch != nullptr)
                (void)backend->presentation_batch->remove_Completed(backend->presentation_token);
            backend->presentation_batch.Reset();
            backend->presentation_completed.reset();
            backend->submitted_presentation_generation = presentation.opacity.generation;
            backend->reconciled_presentation_generation = 0;
            if (backend->compositor != nullptr && presentation.opacity.generation != 0)
            {
                backend->presentation_completed = std::make_shared<std::atomic<uint64_t>>(0);
                auto completed = backend->presentation_completed;
                uint64_t generation = presentation.opacity.generation;
                HWND hwnd = backend->hwnd;
                hr = backend->compositor->CreateScopedBatch(CompositionBatchTypes_Animation,
                                                            &backend->presentation_batch);
                auto handler = Microsoft::WRL::Callback<Microsoft::WRL::Implements<
                    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
                    ABI::Windows::Foundation::ITypedEventHandler<
                        IInspectable *, CompositionBatchCompletedEventArgs *>,
                    Microsoft::WRL::FtmBase>>(
                    [completed, generation, hwnd](IInspectable *,
                                                  ICompositionBatchCompletedEventArgs *) -> HRESULT
                    {
                        completed->store(generation);
                        PostMessageW(hwnd, WM_NULL, 0, 0);
                        return S_OK;
                    });
                if (SUCCEEDED(hr) && handler == nullptr)
                    hr = E_OUTOFMEMORY;
                if (SUCCEEDED(hr))
                    hr = backend->presentation_batch->add_Completed(handler.Get(),
                                                                    &backend->presentation_token);
                if (FAILED(hr))
                {
                    backend->presentation_batch.Reset();
                    backend->presentation_completed.reset();
                    backend->submitted_presentation_generation = 0;
                    return REACH_ERROR;
                }
            }
        }
        float center_x = presentation.bounds.x + presentation.bounds.width * 0.5f;
        float center_y = presentation.bounds.y + presentation.bounds.height * 0.5f;
        if (backend->root_visual != nullptr)
        {
            ComPtr<IVisual> root;
            hr = backend->root_visual.As(&root);
            if (SUCCEEDED(hr))
                hr = root->put_CenterPoint({center_x, center_y, 0.0f});
            if (SUCCEEDED(hr))
                hr = wuc_properties(backend, backend->root_visual.Get(), presentation, 0.0f, 0.0f,
                                    false,
                                    property_mask(presentation, backend->scene_presentation, 0.0f,
                                                  0.0f, backend->has_scene_presentation != 0));
        }
        else
        {
            hr = backend->scene_scale->SetCenterX(center_x);
            if (SUCCEEDED(hr))
                hr = backend->scene_scale->SetCenterY(center_y);
            if (SUCCEEDED(hr))
                hr = dcomp_properties(backend, backend->dcomp_visual, backend->scene_scale.Get(),
                                      backend->scene_opacity.Get(), presentation, 0.0f, 0.0f, false,
                                      property_mask(presentation, backend->scene_presentation, 0.0f,
                                                    0.0f, backend->has_scene_presentation != 0));
        }
        if (SUCCEEDED(hr))
        {
            backend->scene_presentation = presentation;
            backend->has_scene_presentation = 1;
        }
        if (animation_changed && backend->presentation_batch != nullptr)
        {
            HRESULT end_hr = backend->presentation_batch->End();
            if (SUCCEEDED(hr))
                hr = end_hr;
        }
    }
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}
