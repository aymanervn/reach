#include "render_d2d_internal.h"
#include "render_d2d_retained_policy.h"
#include "reach/support/workload.h"

#include <algorithm>
#include <dxgi1_3.h>
#include <math.h>
#include <wrl/event.h>
#include <wrl/wrappers/corewrappers.h>

namespace
{
using namespace ABI::Windows::UI::Composition;
using ABI::Windows::Foundation::Numerics::Vector2;
using ABI::Windows::Foundation::Numerics::Vector3;

static double motion_sample_time(reach_render_backend *backend)
{
    if (backend->motion_sample_seconds == 0)
        backend->motion_sample_seconds = reach_motion_now_seconds();
    return backend->motion_sample_seconds;
}

struct motion_sample_scope
{
    reach_render_backend *backend;
    double previous;

    explicit motion_sample_scope(reach_render_backend *value)
        : backend(value), previous(value->motion_sample_seconds)
    {
        backend->motion_sample_seconds = 0;
    }

    ~motion_sample_scope()
    {
        backend->motion_sample_seconds = previous;
    }
};

static void stop_wuc_repetition(IInspectable *value, const wchar_t *name)
{
    if (value == nullptr)
        return;
    ComPtr<ICompositionObject> object;
    HSTRING property = nullptr;
    if (SUCCEEDED(value->QueryInterface(IID_PPV_ARGS(&object))) &&
        SUCCEEDED(WindowsCreateString(name, (UINT32)wcslen(name), &property)))
        (void)object->StopAnimation(property);
    if (property != nullptr)
        WindowsDeleteString(property);
}

static void stop_rounded_clip_motion(reach_d2d_retained_layer &layer)
{
    const wchar_t *properties[] = {L"Offset.X", L"Offset.Y",       L"Size.X",
                                   L"Size.Y",   L"CornerRadius.X", L"CornerRadius.Y"};
    for (const auto *property : properties)
        stop_wuc_repetition(layer.rounded_geometry.Get(), property);
}

static void stop_layer_repetition(reach_render_backend *backend, reach_d2d_retained_layer &layer)
{
    const reach_render_group &group = layer.group;
    bool x = group.x.repeating;
    const reach_motion motions[] = {group.x, group.y, group.opacity, group.scale,
                                    reach_render_group_y_scale(&group)};
    bool active = x || group.tint_rgb_mix.repeating || group.clip_radius_motion.repeating;
    for (const auto &motion : motions)
        active = active || motion.repeating;
    for (const auto &motion : group.clip_insets)
        active = active || motion.repeating;
    for (const auto &motion : group.tint)
        active = active || motion.repeating;
    if (!active)
        return;
    reach_workload_add(REACH_WORKLOAD_PROPERTY_SUBMISSIONS, 1);
    if (backend->compositor != nullptr)
    {
        const wchar_t *properties[] = {L"Offset.X", L"Offset.Y", L"Opacity", L"Scale.X",
                                       L"Scale.Y"};
        for (size_t index = 0; index < 5; ++index)
            if (motions[index].repeating || (index == 0 && x))
                stop_wuc_repetition(layer.visual.Get(), properties[index]);
        if (group.tint_rgb_mix.repeating)
            stop_wuc_repetition(layer.tint_properties.Get(), L"Mix");
        const wchar_t *clips[] = {L"LeftInset", L"TopInset", L"RightInset", L"BottomInset"};
        if (group.clip_radius_motion.repeating)
            stop_rounded_clip_motion(layer);
        for (const auto &motion : group.clip_insets)
            if (motion.repeating)
            {
                stop_rounded_clip_motion(layer);
                break;
            }
        const wchar_t *tints[] = {L"Red", L"Green", L"Blue", L"Alpha"};
        for (size_t index = 0; index < 4; ++index)
        {
            if (group.clip_insets[index].repeating)
                stop_wuc_repetition(layer.inset_clip.Get(), clips[index]);
            if (group.tint[index].repeating)
                stop_wuc_repetition(layer.tint_properties.Get(), tints[index]);
        }
    }
    else
    {
        if (layer.dcomp_visual != nullptr)
        {
            if (x)
                (void)layer.dcomp_visual->SetOffsetX(0.0f);
            if (group.y.repeating)
                (void)layer.dcomp_visual->SetOffsetY(0.0f);
            if (group.opacity.repeating)
            {
                ComPtr<IDCompositionVisual3> visual;
                if (SUCCEEDED(layer.dcomp_visual.As(&visual)))
                    (void)visual->SetOpacity(0.0f);
            }
        }
        if (layer.dcomp_scale != nullptr)
        {
            if (group.scale.repeating)
                (void)layer.dcomp_scale->SetScaleX(1.0f);
            if (reach_render_group_y_scale(&group).repeating)
                (void)layer.dcomp_scale->SetScaleY(1.0f);
        }
        if (layer.dcomp_clip != nullptr)
        {
            bool rounded = reach_render_group_has_rounded_clip(&group) != 0;
            bool clip_repeating = group.clip_radius_motion.repeating != 0;
            for (const auto &motion : group.clip_insets)
                clip_repeating = clip_repeating || motion.repeating;
            if (rounded && clip_repeating)
            {
                (void)layer.dcomp_clip->SetTopLeftRadiusX(0.0f);
                (void)layer.dcomp_clip->SetTopLeftRadiusY(0.0f);
                (void)layer.dcomp_clip->SetTopRightRadiusX(0.0f);
                (void)layer.dcomp_clip->SetTopRightRadiusY(0.0f);
                (void)layer.dcomp_clip->SetBottomLeftRadiusX(0.0f);
                (void)layer.dcomp_clip->SetBottomLeftRadiusY(0.0f);
                (void)layer.dcomp_clip->SetBottomRightRadiusX(0.0f);
                (void)layer.dcomp_clip->SetBottomRightRadiusY(0.0f);
            }
            if (group.clip_insets[0].repeating)
                (void)layer.dcomp_clip->SetLeft(0.0f);
            if (group.clip_insets[1].repeating)
                (void)layer.dcomp_clip->SetTop(0.0f);
            if (group.clip_insets[2].repeating)
                (void)layer.dcomp_clip->SetRight(0.0f);
            if (group.clip_insets[3].repeating)
                (void)layer.dcomp_clip->SetBottom(0.0f);
        }
        if (layer.dcomp_tint != nullptr)
        {
            for (size_t index = 0; index < 4; ++index)
                if (group.tint[index].repeating)
                    (void)layer.dcomp_tint->SetMatrixElement(index == 3 ? 3 : 4, (UINT)index, 0.0f);
        }
        if (layer.dcomp_tint_mix != nullptr && group.tint_rgb_mix.repeating)
            for (UINT index = 0; index < 3; ++index)
            {
                (void)layer.dcomp_tint_mix->SetMatrixElement(index, index, 1.0f);
                (void)layer.dcomp_tint_mix->SetMatrixElement(4, index, 0.0f);
            }
    }
}

static float scalar_value(reach_render_backend *backend, const reach_motion &motion, float fallback)
{
    if (motion.generation == 0)
        return fallback;
    double now = motion_sample_time(backend);
    return reach_motion_value_at(&motion, now);
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
    reach_motion_segment segments[REACH_MOTION_MAX_REPEAT_SEGMENTS] = {};
    double now = motion_sample_time(backend);
    size_t count = motion.repeating ? reach_motion_compile_repeat_at(&motion, now, segments)
                                    : reach_motion_compile_at(&motion, now, segments);
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
        if (SUCCEEDED(hr) && motion.repeating)
            hr = keyframes->put_IterationBehavior(AnimationIterationBehavior_Forever);
        if (SUCCEEDED(hr) && motion.repeating && motion.started_seconds > now)
            hr = keyframes->put_DelayTime({(INT64)((motion.started_seconds - now) * 10000000.0)});
    }
    if (SUCCEEDED(hr) && count > 0)
    {
        hr = scalar->InsertKeyFrame(0.0f, segments[0].value);
    }
    for (size_t index = 0; index < count && SUCCEEDED(hr); ++index)
    {
        const reach_motion_segment &segment = segments[index];
        double dt = segment.end - segment.start;
        float end_value = segment.end_value;
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
        if (distance != 0 && segment.linear == 0 && segment.quadratic == 0 && segment.cubic == 0)
        {
            ComPtr<ICompositor2> factory;
            ComPtr<IStepEasingFunction> step;
            hr = backend->compositor.As(&factory);
            if (SUCCEEDED(hr))
                hr = factory->CreateStepEasingFunctionWithStepCount(1, &step);
            if (SUCCEEDED(hr))
                hr = step->put_IsFinalStepSingleFrame(true);
            if (SUCCEEDED(hr))
                hr = step.As(&easing);
        }
        else
        {
            hr = backend->compositor->CreateCubicBezierEasingFunction(control1, control2, &cubic);
            if (SUCCEEDED(hr))
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
    reach_motion_segment segments[REACH_MOTION_MAX_REPEAT_SEGMENTS] = {};
    double now = motion_sample_time(backend);
    size_t count = motion.repeating ? reach_motion_compile_repeat_at(&motion, now, segments)
                                    : reach_motion_compile_at(&motion, now, segments);
    if (count == 0)
    {
        return S_FALSE;
    }
    HRESULT hr = backend->dcomp_device->CreateAnimation(animation->ReleaseAndGetAddressOf());
    double delay = motion.repeating ? fmax(0.0, motion.started_seconds - now) : 0;
    if (SUCCEEDED(hr) && delay > 0)
        hr = (*animation)->AddCubic(0, reach_motion_value_at(&motion, now), 0, 0, 0);
    for (size_t index = 0; index < count && SUCCEEDED(hr); ++index)
    {
        const reach_motion_segment &segment = segments[index];
        hr = (*animation)
                 ->AddCubic(delay + segment.start, segment.value, segment.linear, segment.quadratic,
                            segment.cubic);
    }
    if (SUCCEEDED(hr))
    {
        hr = motion.repeating
                 ? (*animation)->AddRepeat(delay + segments[count - 1].end, segments[count - 1].end)
                 : (*animation)->End(segments[count - 1].end, segments[count - 1].end_value);
    }
    return hr;
}

static HRESULT sync_tint(reach_render_backend *backend, reach_d2d_retained_layer &layer,
                         const reach_render_group &group, bool content_changed)
{
    if (!group.has_tint && !layer.group.has_tint)
        return S_OK;
    bool mixing = group.tint_rgb_mix.generation != 0;
    bool mix_changed = !reach_motion_equal(&group.tint_rgb_mix, &layer.group.tint_rgb_mix) ||
                       group.tint_rgb_target.r != layer.group.tint_rgb_target.r ||
                       group.tint_rgb_target.g != layer.group.tint_rgb_target.g ||
                       group.tint_rgb_target.b != layer.group.tint_rgb_target.b;
    bool tint_changed = group.has_tint != layer.group.has_tint || mix_changed;
    for (size_t index = 0; index < REACH_RENDER_COLOR_CHANNEL_COUNT; ++index)
        tint_changed =
            tint_changed || !reach_motion_equal(&group.tint[index], &layer.group.tint[index]);
    if (!content_changed && !tint_changed)
        return S_OK;
    reach_workload_add(REACH_WORKLOAD_PROPERTY_SUBMISSIONS, 1);
    if (!group.has_tint)
    {
        HRESULT hr;
        if (backend->compositor != nullptr)
        {
            ComPtr<ICompositionBrush> brush;
            hr = layer.solid ? layer.solid_brush.As(&brush) : layer.surface_brush.As(&brush);
            if (SUCCEEDED(hr))
                hr = layer.visual->put_Brush(brush.Get());
        }
        else
            hr = layer.dcomp_visual->SetEffect(nullptr);
        if (SUCCEEDED(hr))
        {
            layer.tint_mask.Reset();
            layer.tint_brush.Reset();
            layer.tint_properties.Reset();
            layer.dcomp_tint_mix.Reset();
            layer.dcomp_tint.Reset();
        }
        return hr;
    }
    if (backend->compositor != nullptr)
    {
        using Microsoft::WRL::Wrappers::HStringReference;
        HRESULT hr = S_OK;
        bool initialized = layer.tint_brush != nullptr && layer.tint_mask != nullptr &&
                           layer.tint_properties != nullptr;
        if (!initialized)
        {
            ComPtr<ICompositor2> compositor;
            hr = backend->compositor->QueryInterface(IID_PPV_ARGS(&compositor));
            if (SUCCEEDED(hr))
                hr = compositor->CreateMaskBrush(layer.tint_mask.ReleaseAndGetAddressOf());
            if (SUCCEEDED(hr))
                hr = backend->compositor->CreateColorBrush(
                    layer.tint_brush.ReleaseAndGetAddressOf());
            if (SUCCEEDED(hr))
                hr = backend->compositor->CreatePropertySet(
                    layer.tint_properties.ReleaseAndGetAddressOf());
        }
        const wchar_t *names[] = {L"Red", L"Green", L"Blue", L"Alpha"};
        for (size_t index = 0; index < REACH_RENDER_COLOR_CHANNEL_COUNT && SUCCEEDED(hr); ++index)
        {
            if (initialized && reach_motion_equal(&group.tint[index], &layer.group.tint[index]))
                continue;
            reach_motion channel = reach_motion_affine(&group.tint[index], 255.0f, 0.0f);
            hr = layer.tint_properties->InsertScalar(HStringReference(names[index]).Get(),
                                                     reach_motion_value(&channel));
            if (SUCCEEDED(hr))
                hr = wuc_motion(backend, layer.tint_properties.Get(), names[index], channel);
        }
        if (SUCCEEDED(hr) && mixing && (!initialized || mix_changed))
        {
            hr = layer.tint_properties->InsertScalar(HStringReference(L"Mix").Get(),
                                                     reach_motion_value(&group.tint_rgb_mix));
            if (SUCCEEDED(hr))
                hr = wuc_motion(backend, layer.tint_properties.Get(), L"Mix", group.tint_rgb_mix);
            const wchar_t *targets[] = {L"TargetRed", L"TargetGreen", L"TargetBlue"};
            float channels[] = {group.tint_rgb_target.r, group.tint_rgb_target.g,
                                group.tint_rgb_target.b};
            for (size_t index = 0; index < 3 && SUCCEEDED(hr); ++index)
                hr = layer.tint_properties->InsertScalar(HStringReference(targets[index]).Get(),
                                                         channels[index] * 255);
        }
        if (SUCCEEDED(hr) && !mixing && layer.group.tint_rgb_mix.generation != 0)
        {
            stop_wuc_repetition(layer.tint_properties.Get(), L"Mix");
            hr = layer.tint_properties->InsertScalar(HStringReference(L"Mix").Get(), 0);
        }
        if (SUCCEEDED(hr) && (!initialized || mixing != (layer.group.tint_rgb_mix.generation != 0)))
        {
            ComPtr<IExpressionAnimation> expression;
            ComPtr<ICompositionAnimation> animation;
            ComPtr<ICompositionObject> properties;
            ComPtr<ICompositionObject> brush;
            hr = backend->compositor->CreateExpressionAnimationWithExpression(
                HStringReference(
                    mixing
                        ? L"ColorRGB(channels.Alpha, "
                          L"channels.Red*(1-channels.Mix)+channels.TargetRed*channels.Mix, "
                          L"channels.Green*(1-channels.Mix)+channels.TargetGreen*channels.Mix, "
                          L"channels.Blue*(1-channels.Mix)+channels.TargetBlue*channels.Mix)"
                        : L"ColorRGB(channels.Alpha, channels.Red, channels.Green, channels.Blue)")
                    .Get(),
                &expression);
            if (SUCCEEDED(hr))
                hr = expression.As(&animation);
            if (SUCCEEDED(hr))
                hr = layer.tint_properties.As(&properties);
            if (SUCCEEDED(hr))
                hr = animation->SetReferenceParameter(HStringReference(L"channels").Get(),
                                                      properties.Get());
            if (SUCCEEDED(hr))
                hr = layer.tint_brush.As(&brush);
            if (SUCCEEDED(hr))
                hr = brush->StartAnimation(HStringReference(L"Color").Get(), animation.Get());
        }
        ComPtr<ICompositionBrush> source, mask, brush;
        if (SUCCEEDED(hr))
            hr = layer.tint_brush.As(&source);
        if (SUCCEEDED(hr) && layer.solid)
            brush = source;
        else
        {
            if (SUCCEEDED(hr))
                hr = layer.surface_brush.As(&mask);
            if (SUCCEEDED(hr))
                hr = layer.tint_mask->put_Source(source.Get());
            if (SUCCEEDED(hr))
                hr = layer.tint_mask->put_Mask(mask.Get());
            if (SUCCEEDED(hr))
                hr = layer.tint_mask.As(&brush);
        }
        if (SUCCEEDED(hr))
            hr = layer.visual->put_Brush(brush.Get());
        if (FAILED(hr))
        {
            layer.tint_mask.Reset();
            layer.tint_brush.Reset();
            layer.tint_properties.Reset();
        }
        return hr;
    }
    HRESULT hr = S_OK;
    bool initialized = layer.dcomp_tint != nullptr;
    if (!initialized)
    {
        ComPtr<IDCompositionDevice3> device;
        hr = backend->dcomp_device->QueryInterface(IID_PPV_ARGS(&device));
        if (SUCCEEDED(hr))
            hr = device->CreateColorMatrixEffect(&layer.dcomp_tint);
        D2D1_MATRIX_5X4_F matrix = {};
        matrix._44 = 1.0f;
        if (SUCCEEDED(hr))
            hr = layer.dcomp_tint->SetMatrix(matrix);
        if (SUCCEEDED(hr))
            hr = layer.dcomp_tint->SetAlphaMode(D2D1_COLORMATRIX_ALPHA_MODE_PREMULTIPLIED);
        if (SUCCEEDED(hr))
            hr = layer.dcomp_tint->SetClampOutput(TRUE);
    }
    for (size_t index = 0; index < REACH_RENDER_COLOR_CHANNEL_COUNT && SUCCEEDED(hr); ++index)
    {
        if (initialized && reach_motion_equal(&group.tint[index], &layer.group.tint[index]))
            continue;
        int row = index == REACH_RENDER_COLOR_ALPHA ? 3 : 4;
        hr = layer.dcomp_tint->SetMatrixElement(row, (int)index,
                                                reach_motion_value(&group.tint[index]));
        ComPtr<IDCompositionAnimation> animation;
        HRESULT motion_hr =
            SUCCEEDED(hr) ? dcomp_motion(backend, group.tint[index], &animation) : hr;
        if (FAILED(motion_hr))
            hr = motion_hr;
        else if (motion_hr == S_OK)
            hr = layer.dcomp_tint->SetMatrixElement(row, (int)index, animation.Get());
    }
    if (SUCCEEDED(hr) && mixing)
    {
        bool mix_initialized = layer.dcomp_tint_mix != nullptr;
        if (!mix_initialized)
        {
            ComPtr<IDCompositionDevice3> device;
            hr = backend->dcomp_device->QueryInterface(IID_PPV_ARGS(&device));
            if (SUCCEEDED(hr))
                hr = device->CreateColorMatrixEffect(&layer.dcomp_tint_mix);
            D2D1_MATRIX_5X4_F matrix = {};
            matrix._11 = matrix._22 = matrix._33 = matrix._44 = 1;
            if (SUCCEEDED(hr))
                hr = layer.dcomp_tint_mix->SetMatrix(matrix);
            if (SUCCEEDED(hr))
                hr = layer.dcomp_tint_mix->SetAlphaMode(D2D1_COLORMATRIX_ALPHA_MODE_PREMULTIPLIED);
            if (SUCCEEDED(hr))
                hr = layer.dcomp_tint_mix->SetClampOutput(TRUE);
        }
        if (SUCCEEDED(hr))
            hr = layer.dcomp_tint_mix->SetInput(0, layer.dcomp_tint.Get(), 0);
        if (mix_changed || !mix_initialized)
        {
            float target[] = {group.tint_rgb_target.r, group.tint_rgb_target.g,
                              group.tint_rgb_target.b};
            for (UINT channel = 0; channel < 3 && SUCCEEDED(hr); ++channel)
                for (UINT element = 0; element < 2 && SUCCEEDED(hr); ++element)
                {
                    UINT row = element == 0 ? channel : 4;
                    reach_motion motion =
                        element == 0 ? reach_motion_affine(&group.tint_rgb_mix, -1, 1)
                                     : reach_motion_affine(&group.tint_rgb_mix, target[channel], 0);
                    hr = layer.dcomp_tint_mix->SetMatrixElement(row, channel,
                                                                reach_motion_value(&motion));
                    ComPtr<IDCompositionAnimation> animation;
                    HRESULT motion_hr =
                        SUCCEEDED(hr) ? dcomp_motion(backend, motion, &animation) : hr;
                    if (FAILED(motion_hr))
                        hr = motion_hr;
                    else if (motion_hr == S_OK)
                        hr = layer.dcomp_tint_mix->SetMatrixElement(row, channel, animation.Get());
                }
        }
    }
    if (SUCCEEDED(hr))
    {
        hr = layer.dcomp_visual->SetEffect(mixing ? layer.dcomp_tint_mix.Get()
                                                  : layer.dcomp_tint.Get());
        if (!mixing)
            layer.dcomp_tint_mix.Reset();
    }
    if (FAILED(hr))
    {
        layer.dcomp_tint_mix.Reset();
        layer.dcomp_tint.Reset();
    }
    return hr;
}

static void stop_clip_motion(reach_d2d_retained_layer &layer)
{
    const wchar_t *properties[] = {L"LeftInset", L"TopInset", L"RightInset", L"BottomInset"};
    for (const auto *property : properties)
        stop_wuc_repetition(layer.inset_clip.Get(), property);
    stop_rounded_clip_motion(layer);
}

static HRESULT sync_clip(reach_render_backend *backend, reach_d2d_retained_layer &layer,
                         const reach_render_group &group, bool initialized)
{
    if (!group.has_clip)
    {
        HRESULT hr = S_OK;
        if (layer.inset_clip != nullptr || layer.rounded_clip != nullptr)
        {
            ComPtr<IVisual> container;
            hr = layer.container.As(&container);
            if (SUCCEEDED(hr))
                hr = container->put_Clip(nullptr);
            if (SUCCEEDED(hr))
                hr = container->put_BorderMode(CompositionBorderMode_Inherit);
        }
        else if (layer.dcomp_clip != nullptr)
        {
            hr = layer.dcomp_container->SetClip((IDCompositionClip *)nullptr);
            if (SUCCEEDED(hr))
                hr = layer.dcomp_container->SetBorderMode(DCOMPOSITION_BORDER_MODE_INHERIT);
        }
        if (SUCCEEDED(hr))
        {
            stop_clip_motion(layer);
            layer.inset_clip.Reset();
            layer.rounded_geometry.Reset();
            layer.rounded_clip.Reset();
            layer.dcomp_clip.Reset();
        }
        return hr;
    }
    unsigned mask =
        !initialized || !layer.group.has_clip || group.clip.width != layer.group.clip.width ||
                group.clip.height != layer.group.clip.height ||
                (group.clip_radius != layer.group.clip_radius ||
                 group.clip_uniform_radius != layer.group.clip_uniform_radius ||
                 !reach_motion_equal(&group.clip_radius_motion, &layer.group.clip_radius_motion))
            ? 15
            : 0;
    for (size_t edge = 0; edge < REACH_RENDER_CLIP_EDGE_COUNT; ++edge)
        if (!reach_motion_equal(&group.clip_insets[edge], &layer.group.clip_insets[edge]))
            mask |= 1u << edge;
    if (reach_render_group_has_rounded_clip(&group) && backend->compositor != nullptr)
    {
        if (mask == 0 && layer.rounded_clip != nullptr)
            return S_OK;
        reach_rect_motion bounds = {};
        if (reach_render_group_clip_motion(&group, &bounds) != REACH_OK)
            return E_INVALIDARG;
        reach_motion radius_x = {}, radius_y = {};
        if (reach_render_group_clip_radii_motion(&group, &radius_x, &radius_y) != REACH_OK)
            return E_INVALIDARG;
        reach_motion motions[] = {reach_motion_affine(&bounds.x, 1, -group.clip.x),
                                  reach_motion_affine(&bounds.y, 1, -group.clip.y),
                                  bounds.width,
                                  bounds.height,
                                  radius_x,
                                  radius_y};
        reach_workload_add(REACH_WORKLOAD_PROPERTY_SUBMISSIONS, 1);
        ComPtr<ICompositor5> geometry_factory;
        ComPtr<ICompositor6> clip_factory;
        HRESULT hr = backend->compositor.As(&geometry_factory);
        if (SUCCEEDED(hr))
            hr = backend->compositor.As(&clip_factory);
        if (SUCCEEDED(hr) && layer.rounded_geometry == nullptr)
            hr = geometry_factory->CreateRoundedRectangleGeometry(&layer.rounded_geometry);
        double now = motion_sample_time(backend);
        if (SUCCEEDED(hr))
            hr = layer.rounded_geometry->put_Offset(
                {reach_motion_value_at(&motions[0], now), reach_motion_value_at(&motions[1], now)});
        if (SUCCEEDED(hr))
            hr = layer.rounded_geometry->put_Size(
                {reach_motion_value_at(&motions[2], now), reach_motion_value_at(&motions[3], now)});
        if (SUCCEEDED(hr))
            hr = layer.rounded_geometry->put_CornerRadius(
                {reach_motion_value_at(&motions[4], now), reach_motion_value_at(&motions[5], now)});
        const wchar_t *properties[] = {L"Offset.X", L"Offset.Y",       L"Size.X",
                                       L"Size.Y",   L"CornerRadius.X", L"CornerRadius.Y"};
        for (size_t at = 0; at < 6 && SUCCEEDED(hr); ++at)
            hr = wuc_motion(backend, layer.rounded_geometry.Get(), properties[at], motions[at]);
        ComPtr<ICompositionGeometry> geometry;
        if (SUCCEEDED(hr))
            hr = layer.rounded_geometry.As(&geometry);
        if (SUCCEEDED(hr) && layer.rounded_clip == nullptr)
            hr = clip_factory->CreateGeometricClipWithGeometry(geometry.Get(), &layer.rounded_clip);
        ComPtr<ICompositionClip> clip;
        ComPtr<IVisual> container;
        if (SUCCEEDED(hr))
            hr = layer.rounded_clip.As(&clip);
        if (SUCCEEDED(hr))
            hr = layer.container.As(&container);
        if (SUCCEEDED(hr))
            hr = container->put_Clip(clip.Get());
        if (SUCCEEDED(hr) && (!initialized || !reach_render_group_has_rounded_clip(&layer.group)))
            hr = container->put_BorderMode(CompositionBorderMode_Soft);
        if (SUCCEEDED(hr))
        {
            const wchar_t *insets[] = {L"LeftInset", L"TopInset", L"RightInset", L"BottomInset"};
            for (const auto *property : insets)
                stop_wuc_repetition(layer.inset_clip.Get(), property);
            layer.inset_clip.Reset();
        }
        return hr;
    }
    if (mask == 0)
        return S_OK;
    reach_workload_add(REACH_WORKLOAD_PROPERTY_SUBMISSIONS, 1);
    HRESULT hr = S_OK;
    if (backend->compositor != nullptr)
    {
        stop_clip_motion(layer);
        layer.rounded_geometry.Reset();
        layer.rounded_clip.Reset();
        if (layer.inset_clip == nullptr)
        {
            hr = backend->compositor->CreateInsetClip(&layer.inset_clip);
            ComPtr<ICompositionClip> clip;
            ComPtr<IVisual> container;
            if (SUCCEEDED(hr))
                hr = layer.inset_clip.As(&clip);
            if (SUCCEEDED(hr))
                hr = layer.container.As(&container);
            if (SUCCEEDED(hr))
                hr = container->put_Clip(clip.Get());
            if (SUCCEEDED(hr))
                hr = container->put_BorderMode(CompositionBorderMode_Hard);
            if (FAILED(hr))
            {
                layer.inset_clip.Reset();
                return hr;
            }
        }
        static const wchar_t *properties[] = {L"LeftInset", L"TopInset", L"RightInset",
                                              L"BottomInset"};
        for (size_t edge = 0; edge < REACH_RENDER_CLIP_EDGE_COUNT && SUCCEEDED(hr); ++edge)
        {
            if ((mask & (1u << edge)) == 0)
                continue;
            float value = reach_motion_value(&group.clip_insets[edge]);
            switch (edge)
            {
            case REACH_RENDER_CLIP_LEFT:
                hr = layer.inset_clip->put_LeftInset(value);
                break;
            case REACH_RENDER_CLIP_TOP:
                hr = layer.inset_clip->put_TopInset(value);
                break;
            case REACH_RENDER_CLIP_RIGHT:
                hr = layer.inset_clip->put_RightInset(value);
                break;
            case REACH_RENDER_CLIP_BOTTOM:
                hr = layer.inset_clip->put_BottomInset(value);
                break;
            }
            if (SUCCEEDED(hr))
                hr = wuc_motion(backend, layer.inset_clip.Get(), properties[edge],
                                group.clip_insets[edge]);
        }
        return hr;
    }
    bool created = layer.dcomp_clip == nullptr;
    if (created || !initialized ||
        reach_render_group_has_rounded_clip(&group) !=
            reach_render_group_has_rounded_clip(&layer.group))
        hr = layer.dcomp_container->SetBorderMode(reach_render_group_has_rounded_clip(&group)
                                                      ? DCOMPOSITION_BORDER_MODE_SOFT
                                                      : DCOMPOSITION_BORDER_MODE_HARD);
    if (FAILED(hr))
        return hr;
    if (created)
    {
        hr = backend->dcomp_device->CreateRectangleClip(&layer.dcomp_clip);
        if (SUCCEEDED(hr))
            hr = layer.dcomp_container->SetClip(layer.dcomp_clip.Get());
        if (FAILED(hr))
        {
            layer.dcomp_clip.Reset();
            return hr;
        }
    }
    if (created || !initialized ||
        (group.clip_radius != layer.group.clip_radius ||
         group.clip_uniform_radius != layer.group.clip_uniform_radius ||
         !reach_motion_equal(&group.clip_radius_motion, &layer.group.clip_radius_motion)) ||
        (reach_render_group_has_rounded_clip(&group) && mask != 0))
    {
        reach_motion radius_x = {}, radius_y = {};
        reach_motion_set(&radius_x, 0);
        reach_motion_set(&radius_y, 0);
        if (reach_render_group_has_rounded_clip(&group))
        {
            if (reach_render_group_clip_radii_motion(&group, &radius_x, &radius_y) != REACH_OK)
                return E_INVALIDARG;
        }
        ComPtr<IDCompositionAnimation> x_animation, y_animation;
        hr = dcomp_motion(backend, radius_x, &x_animation);
        if (SUCCEEDED(hr))
            hr = dcomp_motion(backend, radius_y, &y_animation);
        float x = reach_motion_value(&radius_x), y = reach_motion_value(&radius_y);
        if (SUCCEEDED(hr))
            hr = x_animation ? layer.dcomp_clip->SetTopLeftRadiusX(x_animation.Get())
                             : layer.dcomp_clip->SetTopLeftRadiusX(x);
        if (SUCCEEDED(hr))
            hr = y_animation ? layer.dcomp_clip->SetTopLeftRadiusY(y_animation.Get())
                             : layer.dcomp_clip->SetTopLeftRadiusY(y);
        if (SUCCEEDED(hr))
            hr = x_animation ? layer.dcomp_clip->SetTopRightRadiusX(x_animation.Get())
                             : layer.dcomp_clip->SetTopRightRadiusX(x);
        if (SUCCEEDED(hr))
            hr = y_animation ? layer.dcomp_clip->SetTopRightRadiusY(y_animation.Get())
                             : layer.dcomp_clip->SetTopRightRadiusY(y);
        if (SUCCEEDED(hr))
            hr = x_animation ? layer.dcomp_clip->SetBottomLeftRadiusX(x_animation.Get())
                             : layer.dcomp_clip->SetBottomLeftRadiusX(x);
        if (SUCCEEDED(hr))
            hr = y_animation ? layer.dcomp_clip->SetBottomLeftRadiusY(y_animation.Get())
                             : layer.dcomp_clip->SetBottomLeftRadiusY(y);
        if (SUCCEEDED(hr))
            hr = x_animation ? layer.dcomp_clip->SetBottomRightRadiusX(x_animation.Get())
                             : layer.dcomp_clip->SetBottomRightRadiusX(x);
        if (SUCCEEDED(hr))
            hr = y_animation ? layer.dcomp_clip->SetBottomRightRadiusY(y_animation.Get())
                             : layer.dcomp_clip->SetBottomRightRadiusY(y);
    }
    for (size_t edge = 0; edge < REACH_RENDER_CLIP_EDGE_COUNT && SUCCEEDED(hr); ++edge)
    {
        if ((mask & (1u << edge)) == 0)
            continue;
        reach_motion motion = group.clip_insets[edge];
        if (edge == REACH_RENDER_CLIP_RIGHT || edge == REACH_RENDER_CLIP_BOTTOM)
        {
            float extent = edge == REACH_RENDER_CLIP_RIGHT ? group.clip.width : group.clip.height;
            motion = reach_motion_affine(&motion, -1.0f, extent);
        }
        float value = reach_motion_value(&motion);
        ComPtr<IDCompositionAnimation> animation;
        hr = dcomp_motion(backend, motion, &animation);
        if (FAILED(hr))
            break;
        switch (edge)
        {
        case REACH_RENDER_CLIP_LEFT:
            hr = animation != nullptr ? layer.dcomp_clip->SetLeft(animation.Get())
                                      : layer.dcomp_clip->SetLeft(value);
            break;
        case REACH_RENDER_CLIP_TOP:
            hr = animation != nullptr ? layer.dcomp_clip->SetTop(animation.Get())
                                      : layer.dcomp_clip->SetTop(value);
            break;
        case REACH_RENDER_CLIP_RIGHT:
            hr = animation != nullptr ? layer.dcomp_clip->SetRight(animation.Get())
                                      : layer.dcomp_clip->SetRight(value);
            break;
        case REACH_RENDER_CLIP_BOTTOM:
            hr = animation != nullptr ? layer.dcomp_clip->SetBottom(animation.Get())
                                      : layer.dcomp_clip->SetBottom(value);
            break;
        }
    }
    return hr;
}

static HRESULT presentation_center(reach_render_backend *backend, const reach_render_group &group,
                                   const reach_motion &x, const reach_motion &y)
{
    reach_workload_add(REACH_WORKLOAD_PROPERTY_SUBMISSIONS, 1);
    float center_x = scalar_value(backend, x, group.bounds.x + group.bounds.width * .5f);
    float center_y = scalar_value(backend, y, group.bounds.y + group.bounds.height * .5f);
    if (backend->root_visual != nullptr)
    {
        ComPtr<IVisual> visual;
        HRESULT hr = backend->root_visual.As(&visual);
        if (SUCCEEDED(hr))
            hr = visual->put_CenterPoint({center_x, center_y, 0});
        if (SUCCEEDED(hr))
            hr = wuc_motion(backend, backend->root_visual.Get(), L"CenterPoint.X", x);
        if (SUCCEEDED(hr))
            hr = wuc_motion(backend, backend->root_visual.Get(), L"CenterPoint.Y", y);
        return hr;
    }
    HRESULT hr = backend->scene_scale->SetCenterX(center_x);
    if (SUCCEEDED(hr))
        hr = backend->scene_scale->SetCenterY(center_y);
    ComPtr<IDCompositionAnimation> animation;
    if (SUCCEEDED(hr))
    {
        HRESULT motion_hr = dcomp_motion(backend, x, &animation);
        if (FAILED(motion_hr))
            return motion_hr;
        if (motion_hr == S_OK)
            hr = backend->scene_scale->SetCenterX(animation.Get());
    }
    if (SUCCEEDED(hr))
    {
        HRESULT motion_hr = dcomp_motion(backend, y, &animation);
        if (FAILED(motion_hr))
            return motion_hr;
        if (motion_hr == S_OK)
            hr = backend->scene_scale->SetCenterY(animation.Get());
    }
    return hr;
}

static HRESULT wuc_properties(reach_render_backend *backend, IInspectable *inspectable,
                              const reach_render_group &group, float base_x, float base_y,
                              unsigned mask = 31)
{
    reach_workload_add(REACH_WORKLOAD_PROPERTY_SUBMISSIONS, 1);
    ComPtr<IVisual> visual;
    HRESULT hr = inspectable->QueryInterface(IID_PPV_ARGS(&visual));
    if (SUCCEEDED(hr))
    {
        hr = visual->put_Offset({base_x + scalar_value(backend, group.x, 0.0f),
                                 base_y + scalar_value(backend, group.y, 0.0f), 0.0f});
    }
    if (SUCCEEDED(hr))
    {
        if (mask & 4)
            hr = visual->put_Opacity(scalar_value(backend, group.opacity, 1.0f));
    }
    if (SUCCEEDED(hr))
    {
        double now = motion_sample_time(backend);
        reach_point_f32 scale = reach_render_group_scale_at(&group, now);
        if (mask & 24)
            hr = visual->put_Scale({scale.x, scale.y, 1.0f});
    }
    reach_motion x = reach_motion_affine(&group.x, 1.0f, base_x);
    reach_motion y = reach_motion_affine(&group.y, 1.0f, base_y);
    if (SUCCEEDED(hr) && (mask & 1))
        hr = wuc_motion(backend, inspectable, L"Offset.X", x);
    if (SUCCEEDED(hr) && (mask & 2))
        hr = wuc_motion(backend, inspectable, L"Offset.Y", y);
    if (SUCCEEDED(hr) && (mask & 4))
        hr = wuc_motion(backend, inspectable, L"Opacity", group.opacity);
    if (SUCCEEDED(hr) && (mask & 8))
        hr = wuc_motion(backend, inspectable, L"Scale.X", group.scale);
    if (SUCCEEDED(hr) && (mask & 16))
        hr = wuc_motion(backend, inspectable, L"Scale.Y", reach_render_group_y_scale(&group));
    return hr;
}

static HRESULT dcomp_properties(reach_render_backend *backend, IDCompositionVisual2 *visual,
                                IDCompositionScaleTransform *scale, const reach_render_group &group,
                                float base_x, float base_y, unsigned mask = 31)
{
    reach_workload_add(REACH_WORKLOAD_PROPERTY_SUBMISSIONS, 1);
    HRESULT hr =
        !(mask & 1) ? S_OK : visual->SetOffsetX(base_x + scalar_value(backend, group.x, 0.0f));
    if (SUCCEEDED(hr) && (mask & 2))
        hr = visual->SetOffsetY(base_y + scalar_value(backend, group.y, 0.0f));
    ComPtr<IDCompositionVisual3> opacity;
    if (SUCCEEDED(hr) && (mask & 4))
        hr = visual->QueryInterface(IID_PPV_ARGS(&opacity));
    if (SUCCEEDED(hr) && (mask & 4))
        hr = opacity->SetOpacity(scalar_value(backend, group.opacity, 1.0f));
    if (SUCCEEDED(hr) && (mask & 8))
        hr = scale->SetScaleX(scalar_value(backend, group.scale, 1.0f));
    if (SUCCEEDED(hr) && (mask & 16))
        hr = scale->SetScaleY(scalar_value(backend, reach_render_group_y_scale(&group), 1.0f));
    reach_motion motions[5] = {group.x, group.y, group.opacity, group.scale,
                               reach_render_group_y_scale(&group)};
    motions[0] = reach_motion_affine(&group.x, 1.0f, base_x);
    motions[1] = reach_motion_affine(&group.y, 1.0f, base_y);
    for (size_t index = 0; index < 5 && SUCCEEDED(hr); ++index)
    {
        if ((mask & (1u << index)) == 0)
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
    if (!reach_motion_equal(&group.x, &previous.x) || base_x != previous_x)
        mask |= 1;
    if (!reach_motion_equal(&group.y, &previous.y) || base_y != previous_y)
        mask |= 2;
    if (!reach_motion_equal(&group.opacity, &previous.opacity))
        mask |= 4;
    if (!reach_motion_equal(&group.scale, &previous.scale))
        mask |= 8;
    reach_motion y = reach_render_group_y_scale(&group);
    reach_motion previous_y_scale = reach_render_group_y_scale(&previous);
    if (!reach_motion_equal(&y, &previous_y_scale))
        mask |= 16;
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

static void release_swap_chain(reach_render_backend *backend)
{
    if (backend->swap_chain_visual != nullptr && backend->root_visual != nullptr)
    {
        ComPtr<IVisualCollection> children;
        ComPtr<IVisual> visual;
        if (SUCCEEDED(backend->root_visual->get_Children(&children)) &&
            SUCCEEDED(backend->swap_chain_visual.As(&visual)))
        {
            reach_workload_add(REACH_WORKLOAD_VISUAL_TREE_MUTATIONS, 1);
            (void)children->Remove(visual.Get());
        }
    }
    if (backend->dcomp_visual != nullptr && backend->swap_chain != nullptr)
        (void)backend->dcomp_visual->SetContent(nullptr);
    retire_swap_chain(backend);
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
            {
                reach_workload_add(REACH_WORKLOAD_VISUAL_TREE_MUTATIONS, 1);
                (void)children->Remove(old.Get());
            }
        }
        if (SUCCEEDED(hr))
        {
            reach_workload_add(REACH_WORKLOAD_VISUAL_TREE_MUTATIONS, 1);
            hr = children->InsertAtTop(scene.Get());
        }
        if (SUCCEEDED(hr))
            retire_swap_chain(backend);
        return hr;
    }
    HRESULT hr = backend->dcomp_device->CreateVisual(&backend->dcomp_scene_visual);
    if (SUCCEEDED(hr))
        hr = backend->dcomp_visual->SetContent(nullptr);
    if (SUCCEEDED(hr))
    {
        reach_workload_add(REACH_WORKLOAD_VISUAL_TREE_MUTATIONS, 1);
        hr = backend->dcomp_visual->AddVisual(backend->dcomp_scene_visual.Get(), TRUE, nullptr);
    }
    if (SUCCEEDED(hr))
        hr = backend->dcomp_device->CreateScaleTransform(&backend->scene_scale);
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
        {
            reach_workload_add(REACH_WORKLOAD_VISUAL_TREE_MUTATIONS, 1);
            hr = children->InsertAtTop(visual.Get());
        }
        return hr;
    }
    HRESULT hr = backend->dcomp_device->CreateVisual(&layer.dcomp_container);
    if (SUCCEEDED(hr))
        hr = backend->dcomp_device->CreateVisual(&layer.dcomp_visual);
    if (SUCCEEDED(hr))
    {
        reach_workload_add(REACH_WORKLOAD_VISUAL_TREE_MUTATIONS, 1);
        hr = layer.dcomp_container->AddVisual(layer.dcomp_visual.Get(), TRUE, nullptr);
    }
    if (SUCCEEDED(hr))
        hr = backend->dcomp_device->CreateScaleTransform(&layer.dcomp_scale);
    if (SUCCEEDED(hr))
        hr = layer.dcomp_visual->SetTransform(layer.dcomp_scale.Get());
    if (SUCCEEDED(hr))
        hr = layer.dcomp_visual->SetOpacityMode(DCOMPOSITION_OPACITY_MODE_MULTIPLY);
    return hr;
}

static void record_retained_bytes(reach_render_backend *backend)
{
    if (!reach_workload_enabled())
        return;
    uint64_t bytes = 0;
    for (const auto &layer : backend->retained_layers)
        bytes += reach_d2d_layer_bytes((float)layer.capacity_width, (float)layer.capacity_height,
                                       true, false) +
                 layer.shadow_bytes;
    reach_workload_adjust_retained_bytes((int64_t)bytes -
                                         (int64_t)backend->reported_retained_bytes);
    backend->reported_retained_bytes = bytes;
}

static uint64_t surface_budget_left(const reach_render_backend *backend,
                                    const reach_d2d_retained_layer &layer)
{
    uint64_t used = layer.shadow_bytes;
    for (const auto &other : backend->retained_layers)
        if (&other != &layer)
            used += reach_d2d_layer_bytes((float)other.capacity_width, (float)other.capacity_height,
                                          true, false) +
                    other.shadow_bytes;
    return used < REACH_D2D_MAX_SURFACE_BYTES ? REACH_D2D_MAX_SURFACE_BYTES - used : 0;
}

static HRESULT draw_layer(reach_render_backend *backend, reach_d2d_retained_layer &layer,
                          const std::vector<reach_render_command> &commands, UINT width,
                          UINT height, bool *recreated = nullptr)
{
    HRESULT hr = S_OK;
    ComPtr<ID2D1DeviceContext> context;
    POINT offset = {};
    ComPtr<ICompositionDrawingSurfaceInterop> interop;
    reach_d2d_surface_size allocation = {width, height};
    if (recreated != nullptr)
        *recreated = false;
    if (backend->compositor != nullptr)
    {
        reach_d2d_surface_size current = {};
        if (layer.surface != nullptr)
            current = {layer.capacity_width, layer.capacity_height};
        allocation = reach_d2d_surface_capacity(current, {width, height},
                                                surface_budget_left(backend, layer),
                                                {backend->target_width, backend->target_height});
        if (layer.surface == nullptr || allocation.width != current.width ||
            allocation.height != current.height)
        {
            if (recreated != nullptr)
                *recreated = true;
            layer.surface.Reset();
            layer.capacity_width = layer.capacity_height = 0;
            hr = backend->composition_graphics_device->CreateDrawingSurface(
                {(float)allocation.width, (float)allocation.height},
                ABI::Windows::Graphics::DirectX::DirectXPixelFormat_B8G8R8A8UIntNormalized,
                ABI::Windows::Graphics::DirectX::DirectXAlphaMode_Premultiplied, &layer.surface);
            ComPtr<ICompositionSurface> surface;
            ComPtr<ICompositionBrush> base;
            if (SUCCEEDED(hr))
                hr = layer.surface.As(&surface);
            if (SUCCEEDED(hr))
                hr = backend->compositor->CreateSurfaceBrushWithSurface(surface.Get(),
                                                                        &layer.surface_brush);
            if (SUCCEEDED(hr))
                hr = layer.surface_brush->put_Stretch(CompositionStretch_None);
            if (SUCCEEDED(hr))
                hr = layer.surface_brush->put_HorizontalAlignmentRatio(0.0f);
            if (SUCCEEDED(hr))
                hr = layer.surface_brush->put_VerticalAlignmentRatio(0.0f);
            if (SUCCEEDED(hr))
                hr = layer.surface_brush.As(&base);
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
        if (width != layer.width || height != layer.height || layer.dcomp_surface == nullptr)
        {
            if (recreated != nullptr)
                *recreated = true;
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
    reach_workload_add(REACH_WORKLOAD_RASTER_PASSES, 1);
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
    layer.capacity_width = allocation.width;
    layer.capacity_height = allocation.height;
    layer.commands = commands;
    record_retained_bytes(backend);
    return S_OK;
}

static bool solid_layer(const reach_render_backend *backend, const reach_render_group &group,
                        const reach_render_command *commands, size_t count, reach_color *out_color)
{
    return backend->compositor != nullptr && !group.has_shadow &&
           reach_d2d_solid_fill(commands, count, group.bounds, out_color) &&
           (!group.has_tint || out_color->a >= 1.0f);
}

static unsigned char color_byte(float value)
{
    return (unsigned char)(std::min(1.0f, std::max(0.0f, value)) * 255.0f + 0.5f);
}

static HRESULT apply_solid_fill(reach_render_backend *backend, reach_d2d_retained_layer &layer,
                                reach_color color,
                                const std::vector<reach_render_command> &commands, UINT width,
                                UINT height)
{
    HRESULT hr = S_OK;
    if (layer.solid_brush == nullptr)
        hr = backend->compositor->CreateColorBrush(layer.solid_brush.ReleaseAndGetAddressOf());
    if (SUCCEEDED(hr))
        hr = layer.solid_brush->put_Color(
            {color_byte(color.a), color_byte(color.r), color_byte(color.g), color_byte(color.b)});
    ComPtr<ICompositionBrush> brush;
    if (SUCCEEDED(hr))
        hr = layer.solid_brush.As(&brush);
    if (SUCCEEDED(hr))
        hr = layer.visual->put_Brush(brush.Get());
    if (FAILED(hr))
        return hr;
    layer.surface.Reset();
    layer.surface_brush.Reset();
    layer.capacity_width = layer.capacity_height = 0;
    layer.solid = true;
    layer.width = width;
    layer.height = height;
    layer.commands = commands;
    record_retained_bytes(backend);
    return S_OK;
}

static reach_rect_f32 command_bounds(const reach_render_command &command)
{
    reach_rect_f32 rect = command.rect;
    if (command.type == REACH_RENDER_COMMAND_LINE)
    {
        float padding = command.stroke_width * 0.5f + 1;
        rect.x = std::min(command.rect.x, command.rect.x + command.rect.width) - padding;
        rect.y = std::min(command.rect.y, command.rect.y + command.rect.height) - padding;
        rect.width = fabsf(command.rect.width) + padding * 2;
        rect.height = fabsf(command.rect.height) + padding * 2;
    }
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

static bool text_snap_transform(const reach_render_group &group)
{
    auto unit = [](const reach_motion &motion)
    {
        return motion.generation == 0 ||
               (motion.duration_seconds <= 0
                    ? reach_motion_value_at(&motion, 0) == 1
                    : !motion.has_mapping && motion.from == 1 && motion.to == 1);
    };
    return unit(group.scale) && unit(reach_render_group_y_scale(&group)) &&
           (group.x.generation == 0 ||
            (!group.x.has_mapping && group.x.from == 0 && group.x.to == 0)) &&
           !group.x.repeating;
}

static bool raster_group_fixed(const reach_render_group &group)
{
    auto fixed = [](const reach_motion &motion, float expected, bool match)
    {
        float minimum = 0, maximum = 0;
        return reach_motion_range(&motion, &minimum, &maximum) == REACH_OK && minimum == maximum &&
               (!match || minimum == expected);
    };
    return fixed(group.x, 0, false) && fixed(group.y, 0, false) &&
           (!group.scale.generation || fixed(group.scale, 1, true)) &&
           (!group.scale_y.generation || fixed(group.scale_y, 1, true));
}

static bool raster_translation_fixed(const reach_render_command_buffer *commands,
                                     const reach_render_group &source)
{
    if (commands->content_transform.scale_x != 1 || commands->content_transform.scale_y != 1)
        return false;
    const reach_render_group *group = &source;
    size_t depth = 0;
    while (group != nullptr && depth++ <= commands->group_count)
    {
        if (!raster_group_fixed(*group))
            return false;
        group = reach_render_command_buffer_group(commands, group->parent_id);
    }
    return group == nullptr;
}

static reach_rect_f32 raster_source_bounds(const reach_render_command_buffer *commands,
                                           const reach_render_group &source)
{
    if (!raster_translation_fixed(commands, source))
        return source.bounds;
    auto transform = reach_render_command_buffer_group_transform_at(commands, source.id, 0);
    float x = transform.offset_x + commands->content_transform.offset_x;
    float y = transform.offset_y + commands->content_transform.offset_y;
    reach_rect_f32 bounds = source.bounds;
    float left = floorf(bounds.x + x), top = floorf(bounds.y + y);
    bounds.width = ceilf(bounds.x + x + bounds.width) - left;
    bounds.height = ceilf(bounds.y + y + bounds.height) - top;
    bounds.x = left - x;
    bounds.y = top - y;
    if (bounds.width > REACH_D2D_MAX_SURFACE_DIMENSION ||
        bounds.height > REACH_D2D_MAX_SURFACE_DIMENSION)
        return source.bounds;
    return bounds;
}

static HRESULT sync_layer(reach_render_backend *backend, reach_d2d_retained_layer &layer,
                          const reach_render_group &group, const reach_render_command *commands,
                          size_t count, reach_point_f32 parent_origin, bool allow_text_snap)
{
    if (!isfinite(group.bounds.width) || !isfinite(group.bounds.height) ||
        group.bounds.width > REACH_D2D_MAX_SURFACE_DIMENSION ||
        group.bounds.height > REACH_D2D_MAX_SURFACE_DIMENSION)
        return E_INVALIDARG;
    UINT width = count != 0 ? (UINT)std::max(1.0f, ceilf(group.bounds.width)) : 0;
    UINT height = count != 0 ? (UINT)std::max(1.0f, ceilf(group.bounds.height)) : 0;
    std::vector<reach_render_command> local;
    if (count != 0)
        local.assign(commands, commands + count);
    for (auto &command : local)
        localize(command, group.bounds);
    bool initialized = layer.initialized;
    bool previous_hard_border =
        initialized && !layer.commands.empty() && layer.group.scale_y.generation != 0;
    reach_color solid_color = {};
    bool solid = count != 0 && solid_layer(backend, group, commands, count, &solid_color);
    bool content_changed =
        width != layer.width || height != layer.height || local.size() != layer.commands.size() ||
        solid != layer.solid ||
        (!local.empty() && memcmp(local.data(), layer.commands.data(),
                                  local.size() * sizeof(reach_render_command)) != 0);
    bool surface_recreated = false;
    HRESULT hr = S_OK;
    if (content_changed && solid)
        hr = apply_solid_fill(backend, layer, solid_color, local, width, height);
    else if (content_changed && count != 0)
    {
        hr = draw_layer(backend, layer, local, width, height, &surface_recreated);
        if (SUCCEEDED(hr))
            layer.solid = false;
    }
    else if (content_changed)
    {
        hr = backend->compositor != nullptr ? layer.visual->put_Brush(nullptr)
                                            : layer.dcomp_visual->SetContent(nullptr);
        if (SUCCEEDED(hr))
        {
            layer.solid = false;
            layer.surface.Reset();
            layer.surface_brush.Reset();
            layer.dcomp_surface.Reset();
            layer.commands.clear();
            layer.width = layer.height = 0;
            layer.capacity_width = layer.capacity_height = 0;
            record_retained_bytes(backend);
        }
    }
    if (content_changed)
    {
        layer.text_raster_phase_y = 0.0f;
        layer.text_phase_valid =
            count == 1 && local[0].type == REACH_RENDER_COMMAND_TEXT &&
            reach_d2d_text_raster_phase(backend, &local[0], &layer.text_raster_phase_y);
    }
    bool hard_border = count != 0 && group.scale_y.generation != 0;
    if (SUCCEEDED(hr) && backend->compositor != nullptr && (hard_border || previous_hard_border) &&
        (!initialized || hard_border != previous_hard_border))
    {
        ComPtr<IVisual> visual;
        hr = layer.visual.As(&visual);
        if (SUCCEEDED(hr))
        {
            reach_workload_add(REACH_WORKLOAD_PROPERTY_SUBMISSIONS, 1);
            hr = visual->put_BorderMode(hard_border ? CompositionBorderMode_Hard
                                                    : CompositionBorderMode_Inherit);
        }
    }
    bool text_pixel_aligned = allow_text_snap && layer.text_phase_valid;
    float text_phase_y = text_pixel_aligned ? layer.text_raster_phase_y : 0.0f;
    bool text_sampling_changed = text_pixel_aligned != layer.text_pixel_aligned;
    bool text_phase_changed = text_phase_y != layer.text_presentation_phase_y;
    if (SUCCEEDED(hr) && count == 0 && text_sampling_changed && layer.dcomp_visual != nullptr)
        hr = layer.dcomp_visual->SetBitmapInterpolationMode(
            DCOMPOSITION_BITMAP_INTERPOLATION_MODE_INHERIT);
    if (SUCCEEDED(hr) && count != 0 && !solid &&
        (text_sampling_changed ||
         (surface_recreated && text_pixel_aligned && backend->compositor != nullptr)))
    {
        reach_workload_add(REACH_WORKLOAD_PROPERTY_SUBMISSIONS, 1);
        hr = backend->compositor != nullptr
                 ? layer.surface_brush->put_BitmapInterpolationMode(
                       text_pixel_aligned ? CompositionBitmapInterpolationMode_NearestNeighbor
                                          : CompositionBitmapInterpolationMode_Linear)
                 : layer.dcomp_visual->SetBitmapInterpolationMode(
                       text_pixel_aligned ? DCOMPOSITION_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR
                                          : DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);
    }
    bool geometry_changed = !initialized || text_phase_changed || text_sampling_changed ||
                            !reach_render_group_equal(&group, &layer.group) ||
                            parent_origin.x != layer.parent_origin.x ||
                            parent_origin.y != layer.parent_origin.y;
    float anchor_x = group.has_clip ? group.clip.x : group.bounds.x;
    float anchor_y = group.has_clip ? group.clip.y : group.bounds.y;
    float base_x = group.bounds.x - anchor_x;
    float base_y = group.bounds.y - anchor_y + text_phase_y;
    reach_render_group previous = layer.group;
    if (previous.has_clip)
        previous.bounds.y += layer.text_presentation_phase_y;
    float comparison_y = group.has_clip ? base_y : 0.0f;
    unsigned mask = property_mask(group, previous, base_x, comparison_y, initialized);
    if (text_phase_changed)
        mask |= 2;
    if (SUCCEEDED(hr) && geometry_changed && backend->compositor != nullptr)
    {
        ComPtr<IVisual> container;
        ComPtr<IVisual> visual;
        hr = layer.container.As(&container);
        if (SUCCEEDED(hr))
            hr = layer.visual.As(&visual);
        if (SUCCEEDED(hr))
            hr = container->put_Offset(
                {anchor_x - parent_origin.x, anchor_y - parent_origin.y, 0.0f});
        if (SUCCEEDED(hr))
            hr = visual->put_Size({count != 0 ? (float)width : group.bounds.width,
                                   count != 0 ? (float)height : group.bounds.height});
        if (SUCCEEDED(hr) && group.has_clip)
        {
            hr = container->put_Size({group.clip.width, group.clip.height});
        }
        if (SUCCEEDED(hr) && mask != 0)
            hr = wuc_properties(backend, layer.visual.Get(), group, base_x, base_y, mask);
    }
    else if (SUCCEEDED(hr) && geometry_changed)
    {
        hr = layer.dcomp_container->SetOffsetX(anchor_x - parent_origin.x);
        if (SUCCEEDED(hr))
            hr = layer.dcomp_container->SetOffsetY(anchor_y - parent_origin.y);
        if (SUCCEEDED(hr) && mask != 0)
            hr = dcomp_properties(backend, layer.dcomp_visual.Get(), layer.dcomp_scale.Get(), group,
                                  base_x, base_y, mask);
    }
    if (SUCCEEDED(hr) && geometry_changed)
        hr = sync_clip(backend, layer, group, initialized);
    if (SUCCEEDED(hr))
        hr = sync_tint(backend, layer, group, content_changed);
    if (SUCCEEDED(hr))
        hr = reach_d2d_sync_shadow(backend, layer, group);
    if (SUCCEEDED(hr))
    {
        layer.group = group;
        layer.parent_origin = parent_origin;
        layer.text_pixel_aligned = text_pixel_aligned;
        layer.text_presentation_phase_y = text_phase_y;
        layer.initialized = true;
    }
    return hr;
}
} // namespace

int32_t reach_d2d_retained_supported(const reach_render_backend *backend)
{
    return backend != nullptr &&
           ((backend->compositor != nullptr && backend->composition_graphics_device != nullptr) ||
            backend->dcomp_device != nullptr);
}

int32_t reach_d2d_content_sampling_required(const reach_render_backend *backend)
{
    return backend != nullptr && backend->requires_content_sampling;
}

static reach_render_command reach_d2d_sampled_image_command(const reach_d2d_sampled_image &source,
                                                            double now_seconds)
{
    reach_render_command command = source.command;
    command.rect_fit_to_clip = 0;
    command.rect = reach_rect_motion_value_at(&source.bounds, now_seconds);
    command.radius = fminf(source.radius, fminf(command.rect.width, command.rect.height) * .5f);
    if (command.rect.width <= 0 || command.rect.height <= 0)
        command.type = REACH_RENDER_COMMAND_NONE;
    return command;
}

static reach_result reach_d2d_sampled_text_command(reach_render_backend *backend,
                                                   const reach_render_command &source,
                                                   const reach_motion &width, double now_seconds,
                                                   reach_render_command *out)
{
    *out = source;
    out->text_fit_to_clip = 0;
    out->rect.width = reach_motion_value_at(&width, now_seconds);
    if (source.text_fit_to_clip != REACH_RENDER_TEXT_FIT_VERTICAL_OVERFLOW)
        return REACH_OK;
    reach_rect_f32 fit = {};
    if (reach_d2d_text_fit_bounds(backend, out, &fit) != REACH_OK)
        return REACH_ERROR;
    reach_rect_f32 clip = {fit.x, fit.y, out->rect.x + out->rect.width - fit.x, fit.height};
    if (out->has_scissor)
    {
        float left = fmaxf(clip.x, out->scissor_rect.x);
        float top = fmaxf(clip.y, out->scissor_rect.y);
        float right = fminf(clip.x + clip.width, out->scissor_rect.x + out->scissor_rect.width);
        float bottom = fminf(clip.y + clip.height, out->scissor_rect.y + out->scissor_rect.height);
        clip = {left, top, fmaxf(0, right - left), fmaxf(0, bottom - top)};
    }
    out->has_scissor = 1;
    out->scissor_rect = clip;
    return REACH_OK;
}

static bool reach_d2d_textbox_color_active(const reach_render_group &color, double now_seconds)
{
    for (const auto &channel : color.tint)
        if (reach_motion_active_at(&channel, now_seconds))
            return true;
    return reach_motion_active_at(&color.tint_rgb_mix, now_seconds) != 0;
}

static reach_render_command
reach_d2d_sampled_textbox_command(const reach_d2d_sampled_textbox &source, double now_seconds)
{
    auto command = source.command;
    command.text_color_group_id = 0;
    command.text_color = reach_render_group_tint_at(&source.color, now_seconds);
    if (command.caret_group_id != 0)
    {
        command.caret_group_id = 0;
        command.caret_visible = 0;
        command.caret_index = 0;
    }
    return command;
}

reach_result reach_d2d_sample_content(reach_render_backend *backend)
{
    if (backend == nullptr || !reach_d2d_retained_supported(backend))
        return REACH_INVALID_ARGUMENT;
    if (backend->sampled_text.empty() && backend->sampled_images.empty() &&
        backend->sampled_textboxes.empty())
    {
        backend->requires_content_sampling = false;
        return REACH_OK;
    }
    reach_workload_add(REACH_WORKLOAD_RENDER_EXECUTIONS, 1);
    motion_sample_scope sample(backend);
    bool active = false;
    for (const auto &source : backend->sampled_text)
    {
        auto layer = std::find_if(backend->retained_layers.begin(), backend->retained_layers.end(),
                                  [&](const reach_d2d_retained_layer &value)
                                  { return value.id == source.owner; });
        if (layer == backend->retained_layers.end())
            return REACH_ERROR;
        reach_render_command command = {};
        if (reach_d2d_sampled_text_command(backend, source.command, source.width,
                                           motion_sample_time(backend), &command) != REACH_OK)
            return REACH_ERROR;
        localize(command, layer->group.bounds);
        if (layer->commands.size() != 1 || layer->commands[0].rect.width != command.rect.width)
        {
            std::vector<reach_render_command> commands = {command};
            if (FAILED(draw_layer(backend, *layer, commands, layer->width, layer->height)))
                return REACH_ERROR;
        }
        active = active || reach_motion_active_at(&source.width, motion_sample_time(backend));
    }
    for (const auto &source : backend->sampled_textboxes)
    {
        auto layer = std::find_if(backend->retained_layers.begin(), backend->retained_layers.end(),
                                  [&](const reach_d2d_retained_layer &value)
                                  { return value.id == source.owner; });
        if (layer == backend->retained_layers.end())
            return REACH_ERROR;
        auto command = reach_d2d_sampled_textbox_command(source, motion_sample_time(backend));
        localize(command, layer->group.bounds);
        if (layer->commands.size() != 1 ||
            memcmp(&layer->commands[0], &command, sizeof(command)) != 0)
        {
            std::vector<reach_render_command> commands = {command};
            if (FAILED(draw_layer(backend, *layer, commands, layer->width, layer->height)))
                return REACH_ERROR;
        }
        active =
            active || reach_d2d_textbox_color_active(source.color, motion_sample_time(backend));
    }
    for (const auto &source : backend->sampled_images)
    {
        auto layer = std::find_if(backend->retained_layers.begin(), backend->retained_layers.end(),
                                  [&](const reach_d2d_retained_layer &value)
                                  { return value.id == source.owner; });
        if (layer == backend->retained_layers.end())
            return REACH_ERROR;
        auto command = reach_d2d_sampled_image_command(source, motion_sample_time(backend));
        localize(command, layer->group.bounds);
        if (layer->commands.size() != 1 ||
            memcmp(&layer->commands[0], &command, sizeof(command)) != 0)
        {
            std::vector<reach_render_command> commands = {command};
            if (FAILED(draw_layer(backend, *layer, commands, layer->width, layer->height)))
                return REACH_ERROR;
        }
        for (const reach_motion *motion :
             {&source.bounds.x, &source.bounds.y, &source.bounds.width, &source.bounds.height})
            active = active || reach_motion_active_at(motion, motion_sample_time(backend));
    }
    reach_result result = reach_d2d_end_frame(backend);
    if (result == REACH_OK)
    {
        backend->requires_content_sampling = active;
        if (!active)
        {
            backend->sampled_text.clear();
            backend->sampled_textboxes.clear();
            backend->sampled_images.clear();
        }
    }
    return result;
}

void reach_d2d_clear_retained_layers(reach_render_backend *backend)
{
    backend->requires_content_sampling = false;
    backend->sampled_text.clear();
    backend->sampled_textboxes.clear();
    backend->sampled_images.clear();
    backend->text_width_owners.clear();
    backend->image_fit_owners.clear();
    for (auto &layer : backend->retained_layers)
        stop_layer_repetition(backend, layer);
    backend->retained_layers.clear();
    backend->retained_topology_valid = false;
    record_retained_bytes(backend);
}

void reach_d2d_release_retained(reach_render_backend *backend)
{
    if (backend == nullptr)
        return;
    if (backend->presentation_batch != nullptr)
        (void)backend->presentation_batch->remove_Completed(backend->presentation_token);
    backend->presentation_batch.Reset();
    backend->presentation_commit.Reset();
    backend->presentation_completed.reset();
    backend->submitted_presentation_generation = 0;
    backend->reconciled_presentation_generation = 0;
    backend->presentation_result = REACH_OK;
    if (backend->root_visual != nullptr)
    {
        ComPtr<ABI::Windows::UI::Composition::ICompositionObject> root;
        if (SUCCEEDED(backend->root_visual.As(&root)))
        {
            const wchar_t *properties[] = {L"Offset.X",     L"Offset.Y", L"Opacity",
                                           L"Scale.X",      L"Scale.Y",  L"CenterPoint.X",
                                           L"CenterPoint.Y"};
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
        {
            reach_workload_add(REACH_WORKLOAD_VISUAL_TREE_MUTATIONS, 1);
            (void)children->Remove(scene.Get());
        }
    }
    if (backend->dcomp_visual != nullptr && backend->dcomp_scene_visual != nullptr)
    {
        reach_workload_add(REACH_WORKLOAD_VISUAL_TREE_MUTATIONS, 1);
        (void)backend->dcomp_visual->RemoveVisual(backend->dcomp_scene_visual.Get());
        (void)backend->dcomp_visual->SetTransform((IDCompositionTransform *)nullptr);
        (void)backend->dcomp_visual->SetEffect(nullptr);
        (void)backend->dcomp_visual->SetOffsetX(0.0f);
        (void)backend->dcomp_visual->SetOffsetY(0.0f);
        ComPtr<IDCompositionVisual3> opacity;
        if (SUCCEEDED(backend->dcomp_visual->QueryInterface(IID_PPV_ARGS(&opacity))))
            (void)opacity->SetOpacity(1.0f);
    }
    reach_d2d_clear_retained_layers(backend);
    backend->retained_topology.clear();
    backend->retained_topology_valid = false;
    backend->has_scene_content_transform = false;
    backend->scene_visual.Reset();
    backend->dcomp_scene_visual.Reset();
    backend->scene_scale.Reset();
    backend->has_scene_presentation = 0;
}

static void trim_devices(reach_render_backend *backend)
{
    if (backend->d2d_device != nullptr)
        (void)backend->d2d_device->ClearResources(0);
    if (backend->d3d_device != nullptr)
    {
        ComPtr<ID3D11DeviceContext> immediate;
        backend->d3d_device->GetImmediateContext(&immediate);
        if (immediate != nullptr)
        {
            immediate->ClearState();
            immediate->Flush();
        }
    }
    ComPtr<IDXGIDevice3> dxgi_device;
    if (backend->dxgi_device != nullptr &&
        SUCCEEDED(backend->dxgi_device->QueryInterface(IID_PPV_ARGS(&dxgi_device))))
        dxgi_device->Trim();
}

void reach_d2d_suspend(reach_render_backend *backend)
{
    if (backend != nullptr)
        backend->requires_content_sampling = false;
    reach_d2d_release_retained(backend);
    bool retained = backend != nullptr && reach_d2d_retained_supported(backend);
    if (retained)
        release_swap_chain(backend);
    if (backend != nullptr && backend->dcomp_device != nullptr)
    {
        reach_workload_add(REACH_WORKLOAD_DCOMP_COMMITS, 1);
        (void)backend->dcomp_device->Commit();
    }
    if (retained)
        trim_devices(backend);
}

static reach_result reach_d2d_fail_presentation(reach_render_backend *backend,
                                                const wchar_t *operation, HRESULT hr)
{
    reach_d2d_log_hresult(operation, hr);
    backend->presentation_result = REACH_ERROR;
    backend->presentation_commit.Reset();
    return REACH_ERROR;
}

reach_result reach_d2d_presentation_complete(reach_render_backend *backend, uint64_t generation,
                                             int32_t *out_complete)
{
    if (out_complete == nullptr)
        return REACH_INVALID_ARGUMENT;
    *out_complete = 0;
    if (backend == nullptr)
        return REACH_INVALID_ARGUMENT;
    if (!reach_d2d_retained_supported(backend) ||
        generation != backend->submitted_presentation_generation)
    {
        *out_complete = 1;
        return REACH_OK;
    }
    if (backend->presentation_result != REACH_OK)
        return backend->presentation_result;
    double now_seconds = reach_motion_now_seconds();
    bool transparent =
        backend->scene_presentation.opacity.generation &&
        reach_motion_value_at(&backend->scene_presentation.opacity, now_seconds) <= 0;
    if (reach_motion_active_at(&backend->scene_presentation.opacity, now_seconds) ||
        reach_motion_active_at(&backend->scene_presentation.x, now_seconds) ||
        reach_motion_active_at(&backend->scene_presentation.y, now_seconds) ||
        reach_motion_active_at(&backend->scene_presentation.scale, now_seconds) ||
        reach_motion_active_at(&backend->scene_presentation.scale_y, now_seconds) ||
        (!transparent && (reach_motion_active_at(&backend->scene_center_x, now_seconds) ||
                          reach_motion_active_at(&backend->scene_center_y, now_seconds))))
        return REACH_OK;
    if (backend->reconciled_presentation_generation == generation)
    {
        *out_complete = 1;
        return REACH_OK;
    }
    if (backend->presentation_commit != nullptr)
    {
        ComPtr<ABI::Windows::Foundation::IAsyncInfo> info;
        AsyncStatus status = Started;
        HRESULT hr = backend->presentation_commit.As(&info);
        if (SUCCEEDED(hr))
            hr = info->get_Status(&status);
        if (FAILED(hr))
            return reach_d2d_fail_presentation(backend, L"presentation commit status", hr);
        if (status == Started)
            return REACH_OK;
        if (status == Error)
        {
            HRESULT error = E_FAIL;
            hr = info->get_ErrorCode(&error);
            return reach_d2d_fail_presentation(backend, L"presentation commit",
                                               FAILED(hr)      ? hr
                                               : FAILED(error) ? error
                                                               : E_FAIL);
        }
        if (status != Completed)
            return reach_d2d_fail_presentation(backend, L"presentation commit canceled", E_ABORT);
        hr = backend->presentation_commit->GetResults();
        if (FAILED(hr))
            return reach_d2d_fail_presentation(backend, L"presentation commit results", hr);
        if (reach_d2d_synchronize(backend) != REACH_OK)
            return reach_d2d_fail_presentation(backend, L"presentation commit synchronization",
                                               E_FAIL);
        backend->reconciled_presentation_generation = generation;
        *out_complete = 1;
        return REACH_OK;
    }
    if (backend->presentation_completed != nullptr &&
        backend->presentation_completed->load() == generation)
    {
        *out_complete = 1;
        return REACH_OK;
    }
    motion_sample_scope sample(backend);
    reach_render_group settled = backend->scene_presentation;
    settled.x.duration_seconds = settled.y.duration_seconds = 0.0;
    settled.opacity.duration_seconds = settled.scale.duration_seconds = 0.0;
    settled.scale_y.duration_seconds = 0.0;
    auto center_x = backend->scene_center_x, center_y = backend->scene_center_y;
    if (center_x.duration_seconds > 0 || center_y.duration_seconds > 0)
    {
        center_x.duration_seconds = center_y.duration_seconds = 0;
        HRESULT center_hr = presentation_center(backend, settled, center_x, center_y);
        if (FAILED(center_hr))
            return reach_d2d_fail_presentation(backend, L"presentation endpoint center", center_hr);
    }
    if (backend->compositor != nullptr)
    {
        HRESULT hr = wuc_properties(backend, backend->root_visual.Get(), settled, 0.0f, 0.0f);
        if (FAILED(hr))
            return reach_d2d_fail_presentation(backend, L"presentation endpoint properties", hr);
        ComPtr<ICompositor5> compositor;
        hr = backend->compositor.As(&compositor);
        if (SUCCEEDED(hr))
            hr = compositor->RequestCommitAsync(&backend->presentation_commit);
        std::weak_ptr<std::atomic<uint64_t>> lifetime = backend->presentation_completed;
        HWND hwnd = backend->hwnd;
        auto handler = Microsoft::WRL::Callback<Microsoft::WRL::Implements<
            Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
            ABI::Windows::Foundation::IAsyncActionCompletedHandler, Microsoft::WRL::FtmBase>>(
            [lifetime, hwnd](ABI::Windows::Foundation::IAsyncAction *, AsyncStatus) -> HRESULT
            {
                if (!lifetime.expired())
                    PostMessageW(hwnd, WM_NULL, 0, 0);
                return S_OK;
            });
        if (SUCCEEDED(hr) && handler == nullptr)
            hr = E_OUTOFMEMORY;
        if (SUCCEEDED(hr))
            hr = backend->presentation_commit->put_Completed(handler.Get());
        if (FAILED(hr))
            return reach_d2d_fail_presentation(backend, L"presentation endpoint commit", hr);
        return REACH_OK;
    }
    if (backend->dcomp_device != nullptr)
    {
        HRESULT hr = dcomp_properties(backend, backend->dcomp_visual, backend->scene_scale.Get(),
                                      settled, 0.0f, 0.0f);
        if (FAILED(hr))
            return reach_d2d_fail_presentation(backend, L"presentation endpoint properties", hr);
        reach_workload_add(REACH_WORKLOAD_DCOMP_COMMITS, 1);
        hr = backend->dcomp_device->Commit();
        if (FAILED(hr))
            return reach_d2d_fail_presentation(backend, L"presentation endpoint commit", hr);
    }
    if (reach_d2d_synchronize(backend) != REACH_OK)
        return reach_d2d_fail_presentation(backend, L"presentation endpoint synchronization",
                                           E_FAIL);
    backend->reconciled_presentation_generation = generation;
    *out_complete = 1;
    return REACH_OK;
}

reach_result reach_d2d_sync_retained(reach_render_backend *backend,
                                     const reach_render_command_buffer *commands)
{
    motion_sample_scope sample(backend);
    backend->requires_content_sampling = false;
    backend->sampled_text.clear();
    backend->sampled_textboxes.clear();
    backend->sampled_images.clear();
    if (reach_render_command_buffer_validate_groups(commands) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    HRESULT hr = create_scene(backend);
    if (FAILED(hr))
    {
        reach_d2d_release_retained(backend);
        return REACH_ERROR;
    }
    if (reach_wuc_apply_content_clip(backend, commands->content_rect) != REACH_OK)
        return REACH_ERROR;
    const reach_transform_f32 &transform = commands->content_transform;
    const reach_transform_f32 &previous_transform = backend->scene_content_transform;
    bool transform_changed = !backend->has_scene_content_transform ||
                             transform.scale_x != previous_transform.scale_x ||
                             transform.scale_y != previous_transform.scale_y ||
                             transform.offset_x != previous_transform.offset_x ||
                             transform.offset_y != previous_transform.offset_y;
    if (transform_changed && backend->scene_visual != nullptr)
    {
        ComPtr<IVisual> scene;
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
            reach_workload_add(REACH_WORKLOAD_SCENE_TRANSFORM_SUBMISSIONS, 1);
            hr = scene->put_TransformMatrix(matrix);
        }
    }
    else if (transform_changed)
    {
        D2D_MATRIX_3X2_F matrix = {commands->content_transform.scale_x,
                                   0.0f,
                                   0.0f,
                                   commands->content_transform.scale_y,
                                   commands->content_transform.offset_x,
                                   commands->content_transform.offset_y};
        if (SUCCEEDED(hr))
        {
            reach_workload_add(REACH_WORKLOAD_SCENE_TRANSFORM_SUBMISSIONS, 1);
            hr = backend->dcomp_scene_visual->SetTransform(matrix);
        }
    }
    if (FAILED(hr))
        return REACH_ERROR;
    if (transform_changed)
    {
        backend->scene_content_transform = transform;
        backend->has_scene_content_transform = true;
    }
    struct retained_plan
    {
        reach_render_group group;
        const reach_render_command *commands = nullptr;
        size_t count = 0;
        std::vector<reach_render_command> owned;
        reach_rect_f32 requested_bounds = {};
        bool requested_bounds_valid = false;
        bool raster_birth_aligned = false;
        bool raster_finite_translation = false;
    };
    std::vector<retained_plan> plans;
    auto preserve_raster_bounds = [&](retained_plan &plan)
    {
        plan.requested_bounds = plan.group.bounds;
        plan.requested_bounds_valid = true;
        auto bounds = raster_source_bounds(commands, plan.group);
        if (!raster_translation_fixed(commands, plan.group) && plan.count == 1)
        {
            auto previous = std::find_if(
                backend->retained_layers.begin(), backend->retained_layers.end(),
                [&](const reach_d2d_retained_layer &layer) { return layer.id == plan.group.id; });
            if (previous != backend->retained_layers.end() && previous->commands.size() == 1)
            {
                auto command = plan.commands[0];
                localize(command, previous->group.bounds);
                if (memcmp(&command, &previous->commands[0], sizeof(command)) == 0)
                    bounds = previous->group.bounds;
            }
        }
        plan.group.bounds = bounds;
    };
    struct text_fit
    {
        uint64_t owner;
        uint64_t prefix;
        reach_render_command command;
        reach_motion width;
        bool expanded;
    };
    std::vector<text_fit> fitted_text;
    struct textbox_fit
    {
        reach_d2d_sampled_textbox source;
        uint64_t ink_owner;
        bool body_painted;
        bool expanded;
    };
    std::vector<textbox_fit> fitted_textboxes;
    struct image_fit
    {
        reach_d2d_sampled_image source;
        uint64_t prefix;
        bool expanded;
    };
    std::vector<image_fit> fitted_images;
    auto plan_for = [&](uint64_t id)
    {
        return std::find_if(plans.begin(), plans.end(),
                            [&](const retained_plan &plan) { return plan.group.id == id; });
    };
    auto sample_text = [&](const text_fit &fit) -> reach_result
    {
        auto base = plan_for(fit.owner);
        reach_render_command command = {};
        if (reach_d2d_sampled_text_command(backend, fit.command, fit.width,
                                           motion_sample_time(backend), &command) != REACH_OK)
            return REACH_ERROR;
        base->owned.assign(1, command);
        base->commands = base->owned.data();
        base->count = 1;
        if (fit.command.text_fit_to_clip == REACH_RENDER_TEXT_FIT_VERTICAL_OVERFLOW)
        {
            base->group.has_clip = 0;
            auto envelope = fit.command;
            if (reach_d2d_text_fit_bounds(backend, &envelope, &base->group.bounds) != REACH_OK)
                return REACH_ERROR;
        }
        preserve_raster_bounds(*base);
        backend->requires_content_sampling =
            backend->requires_content_sampling ||
            reach_motion_active_at(&fit.width, motion_sample_time(backend));
        return REACH_OK;
    };
    auto sample_image = [&](const image_fit &fit)
    {
        auto base = plan_for(fit.source.owner);
        base->owned.assign(
            1, reach_d2d_sampled_image_command(fit.source, motion_sample_time(backend)));
        base->group.has_clip = 0;
        base->commands = base->owned.data();
        base->count = 1;
        preserve_raster_bounds(*base);
        for (const reach_motion *motion : {&fit.source.bounds.x, &fit.source.bounds.y,
                                           &fit.source.bounds.width, &fit.source.bounds.height})
            backend->requires_content_sampling =
                backend->requires_content_sampling ||
                reach_motion_active_at(motion, motion_sample_time(backend));
    };
    auto planned_bytes = [&]()
    {
        uint64_t bytes = 0;
        for (const auto &plan : plans)
        {
            auto existing = std::find_if(
                backend->retained_layers.begin(), backend->retained_layers.end(),
                [&](const reach_d2d_retained_layer &layer) { return layer.id == plan.group.id; });
            reach_d2d_surface_size capacity = {};
            if (existing != backend->retained_layers.end() && existing->surface != nullptr)
                capacity = {existing->capacity_width, existing->capacity_height};
            reach_color solid_color = {};
            bool painted = plan.count != 0 && !solid_layer(backend, plan.group, plan.commands,
                                                           plan.count, &solid_color);
            bytes +=
                reach_d2d_planned_layer_bytes(plan.group.bounds.width, plan.group.bounds.height,
                                              painted, plan.group.has_shadow, capacity);
        }
        return bytes;
    };
    auto fit_bytes = [&](uint64_t prefix)
    {
        double bytes = 0;
        for (const auto &plan : plans)
            if ((plan.group.id & UINT64_C(0xffffffffffff0000)) == prefix)
                bytes += (double)ceilf(plan.group.bounds.width) * ceilf(plan.group.bounds.height);
        return bytes;
    };
    auto limit_fit_sources = [&]() -> reach_result
    {
        if (fitted_text.empty() && fitted_images.empty() && fitted_textboxes.empty())
            return REACH_OK;
        while (planned_bytes() > REACH_D2D_MAX_SURFACE_BYTES ||
               plans.size() > REACH_MAX_RENDER_GROUPS * 2 + 1)
        {
            auto text = fitted_text.end();
            auto image = fitted_images.end();
            double largest_bytes = -1;
            for (auto at = fitted_text.begin(); at != fitted_text.end(); ++at)
                if (at->expanded && fit_bytes(at->prefix) > largest_bytes)
                {
                    largest_bytes = fit_bytes(at->prefix);
                    text = at;
                }
            for (auto at = fitted_images.begin(); at != fitted_images.end(); ++at)
                if (at->expanded && fit_bytes(at->prefix) > largest_bytes)
                {
                    largest_bytes = fit_bytes(at->prefix);
                    image = at;
                }
            auto textbox = fitted_textboxes.end();
            for (auto at = fitted_textboxes.begin(); at != fitted_textboxes.end(); ++at)
                if (at->expanded && at->body_painted)
                {
                    auto ink = plan_for(at->ink_owner);
                    if (ink == plans.end())
                        return REACH_ERROR;
                    double bytes =
                        (double)ceilf(ink->group.bounds.width) * ceilf(ink->group.bounds.height);
                    if (bytes > largest_bytes)
                    {
                        largest_bytes = bytes;
                        textbox = at;
                    }
                }
            if (textbox != fitted_textboxes.end())
            {
                plans.erase(std::remove_if(plans.begin(), plans.end(),
                                           [&](const retained_plan &plan)
                                           { return plan.group.id == textbox->ink_owner; }),
                            plans.end());
                auto base = plan_for(textbox->source.owner);
                base->owned.assign(1, reach_d2d_sampled_textbox_command(
                                          textbox->source, motion_sample_time(backend)));
                base->commands = base->owned.data();
                base->count = 1;
                preserve_raster_bounds(*base);
                textbox->expanded = false;
                backend->requires_content_sampling =
                    backend->requires_content_sampling ||
                    reach_d2d_textbox_color_active(textbox->source.color,
                                                   motion_sample_time(backend));
                continue;
            }
            if (text == fitted_text.end() && image == fitted_images.end())
                break;
            uint64_t prefix = image != fitted_images.end() ? image->prefix : text->prefix;
            plans.erase(std::remove_if(
                            plans.begin(), plans.end(), [&](const retained_plan &plan)
                            { return (plan.group.id & UINT64_C(0xffffffffffff0000)) == prefix; }),
                        plans.end());
            if (image != fitted_images.end())
            {
                sample_image(*image);
                image->expanded = false;
            }
            else
            {
                if (sample_text(*text) != REACH_OK)
                    return REACH_ERROR;
                text->expanded = false;
            }
        }
        return REACH_OK;
    };
    std::vector<reach_d2d_retained_run> runs;
    reach_d2d_retained_runs(commands, &runs);
    for (const reach_d2d_retained_run &run : runs)
    {
        const size_t index = run.begin;
        const size_t end = run.end;
        const reach_render_command &first = commands->commands[index];
        reach_render_group group = {};
        if (first.group_id != 0)
            group = *reach_render_command_buffer_group(commands, first.group_id);
        else
            group.id = run.id;
        if (group.bounds.width <= 0.0f || group.bounds.height <= 0.0f)
            group.bounds = run_bounds(commands->commands + index, end - index);
        if (group.bounds.width > 0.0f && group.bounds.height > 0.0f)
        {
            std::vector<const reach_render_group *> parents;
            const reach_render_group *parent =
                reach_render_command_buffer_group(commands, group.parent_id);
            while (parent != nullptr)
            {
                parents.push_back(parent);
                parent = reach_render_command_buffer_group(commands, parent->parent_id);
            }
            for (auto at = parents.rbegin(); at != parents.rend(); ++at)
                if (plan_for((*at)->id) == plans.end())
                    plans.push_back({**at});
            auto existing = plan_for(group.id);
            if (existing == plans.end())
                plans.push_back({group, commands->commands + index, end - index});
            else
            {
                existing->group = group;
                existing->commands = commands->commands + index;
                existing->count = end - index;
            }
        }
        if (first.type == REACH_RENDER_COMMAND_VECTOR_ICON && end == index + 1)
        {
            auto base = plan_for(group.id);
            if (base != plans.end())
                preserve_raster_bounds(*base);
        }
        if (first.type == REACH_RENDER_COMMAND_RECT && first.rect_fit_to_clip && end == index + 1 &&
            first.radius == 0 && !first.has_scissor && group.has_clip && !group.has_shadow &&
            (group.clip_uniform_radius || !reach_render_group_has_rounded_clip(&group)) &&
            reach_motion_value_at(&group.x, 0) == 0 && reach_motion_value_at(&group.y, 0) == 0 &&
            raster_group_fixed(group) &&
            std::none_of(commands->groups, commands->groups + commands->group_count,
                         [&](const reach_render_group &child)
                         { return child.parent_id == group.id; }))
        {
            reach_rect_motion clip_motion = {};
            bool fixed = reach_render_group_clip_motion(&group, &clip_motion) == REACH_OK;
            for (const reach_motion *motion :
                 {&clip_motion.x, &clip_motion.y, &clip_motion.width, &clip_motion.height})
            {
                float minimum = 0, maximum = 0;
                fixed = fixed && reach_motion_range(motion, &minimum, &maximum) == REACH_OK &&
                        minimum == maximum;
            }
            reach_motion radius_x = {}, radius_y = {};
            float minimum = 0, maximum = 0;
            fixed =
                fixed &&
                reach_render_group_clip_radii_motion(&group, &radius_x, &radius_y) == REACH_OK &&
                reach_motion_range(&radius_x, &minimum, &maximum) == REACH_OK &&
                minimum == maximum &&
                reach_motion_range(&radius_y, &minimum, &maximum) == REACH_OK && minimum == maximum;
            auto clip = reach_rect_motion_value_at(&clip_motion, 0);
            if (fixed && clip.width > 0 && clip.height > 0 && first.rect.x <= clip.x &&
                first.rect.y <= clip.y && first.rect.x + first.rect.width >= clip.x + clip.width &&
                first.rect.y + first.rect.height >= clip.y + clip.height)
            {
                auto base = plan_for(group.id);
                base->owned.assign(1, first);
                base->owned[0].rect_fit_to_clip = 0;
                base->owned[0].rect = clip;
                base->owned[0].radius = reach_motion_value_at(&radius_x, 0);
                base->commands = base->owned.data();
                base->group.has_clip = 0;
                base->group.clip_radius_motion = {};
                base->group.bounds = clip;
                preserve_raster_bounds(*base);
            }
        }
        if (first.type == REACH_RENDER_COMMAND_ICON && first.rect_fit_to_clip)
        {
            reach_rect_motion clip = {};
            if (reach_render_group_clip_motion(&group, &clip) != REACH_OK)
                return REACH_INVALID_ARGUMENT;
            auto owner = std::find_if(
                backend->image_fit_owners.begin(), backend->image_fit_owners.end(),
                [&](const reach_d2d_fit_owner &value) { return value.owner == group.id; });
            if (owner == backend->image_fit_owners.end() &&
                backend->next_image_fit_prefix < UINT64_C(0xe000000000000000))
            {
                backend->image_fit_owners.push_back({group.id, backend->next_image_fit_prefix});
                backend->next_image_fit_prefix += UINT64_C(0x10000);
                owner = backend->image_fit_owners.end() - 1;
            }
            image_fit fit = {{group.id, first, clip, group.clip_radius},
                             owner != backend->image_fit_owners.end() ? owner->prefix : 0,
                             false};
            bool active = false;
            for (const reach_motion *motion : {&clip.x, &clip.y, &clip.width, &clip.height})
                active = active || reach_motion_active_at(motion, motion_sample_time(backend));
            D2D1_SIZE_F bitmap = {};
            bool valid = active && fit.prefix != 0 && !group.has_tint &&
                         reach_d2d_image_size(backend, first.icon_id, &bitmap) == REACH_OK &&
                         bitmap.width > 0 && bitmap.height > 0 &&
                         bitmap.width <= REACH_D2D_MAX_SURFACE_DIMENSION &&
                         bitmap.height <= REACH_D2D_MAX_SURFACE_DIMENSION &&
                         (double)bitmap.width * bitmap.height * 4 <= REACH_D2D_MAX_SURFACE_BYTES;
            retained_plan image = {};
            image.group.id = fit.prefix | 1;
            image.group.parent_id = group.id;
            image.group.bounds = {0, 0, bitmap.width, bitmap.height};
            if (valid)
            {
                image.group.scale = reach_motion_affine(&clip.width, 1 / bitmap.width, 0);
                image.group.scale_y = reach_motion_affine(&clip.height, 1 / bitmap.height, 0);
                image.group.x = clip.x;
                image.group.y = clip.y;
                if (first.icon_crop_to_fill)
                {
                    valid = reach_motion_max(&image.group.scale, &image.group.scale_y,
                                             &image.group.scale) == REACH_OK;
                    image.group.scale_y = image.group.scale;
                    reach_motion offset = {};
                    valid =
                        valid &&
                        reach_motion_combine(&clip.width, .5f, &image.group.scale,
                                             -bitmap.width * .5f, 0, &offset) == REACH_OK &&
                        reach_motion_combine(&clip.x, 1, &offset, 1, 0, &image.group.x) ==
                            REACH_OK &&
                        reach_motion_combine(&clip.height, .5f, &image.group.scale,
                                             -bitmap.height * .5f, 0, &offset) == REACH_OK &&
                        reach_motion_combine(&clip.y, 1, &offset, 1, 0, &image.group.y) == REACH_OK;
                }
            }
            if (valid)
            {
                image.owned.assign(1, first);
                image.owned[0].rect_fit_to_clip = 0;
                image.owned[0].icon_crop_to_fill = 0;
                image.owned[0].rect = image.group.bounds;
                image.commands = image.owned.data();
                image.count = 1;
                auto base = plan_for(group.id);
                base->commands = nullptr;
                base->count = 0;
                plans.push_back(std::move(image));
                fit.expanded = true;
            }
            else
                sample_image(fit);
            fitted_images.push_back(std::move(fit));
        }
        if (first.text_fit_to_clip)
        {
            reach_rect_motion clip = {};
            if (reach_render_group_clip_motion(&group, &clip) != REACH_OK)
                return REACH_INVALID_ARGUMENT;
            auto owner = std::find_if(
                backend->text_width_owners.begin(), backend->text_width_owners.end(),
                [&](const reach_d2d_fit_owner &value) { return value.owner == group.id; });
            if (owner == backend->text_width_owners.end() &&
                backend->next_text_width_prefix < UINT64_C(0xd000000000000000))
            {
                backend->text_width_owners.push_back({group.id, backend->next_text_width_prefix});
                backend->next_text_width_prefix += UINT64_C(0x10000);
                owner = backend->text_width_owners.end() - 1;
            }
            text_fit fit = {group.id, owner != backend->text_width_owners.end() ? owner->prefix : 0,
                            first, clip.width, false};
            const reach_d2d_text_width_layout *layout = nullptr;
            std::vector<retained_plan> parts;
            bool valid = fit.prefix != 0 &&
                         reach_motion_active_at(&fit.width, motion_sample_time(backend)) &&
                         reach_d2d_text_width_layout_for(backend, &first, &layout) == REACH_OK;
            uint64_t parent = group.id;
            if (valid && first.has_scissor)
            {
                retained_plan scissor = {};
                scissor.group.id = fit.prefix;
                scissor.group.parent_id = parent;
                scissor.group.bounds = first.rect;
                scissor.group.has_clip = 1;
                scissor.group.clip = first.scissor_rect;
                parts.push_back(std::move(scissor));
                parent = fit.prefix;
            }
            reach_rect_f32 paint_bounds = first.rect;
            bool overflow = first.text_fit_to_clip == REACH_RENDER_TEXT_FIT_VERTICAL_OVERFLOW;
            if (overflow)
                valid =
                    valid && reach_d2d_text_fit_bounds(backend, &first, &paint_bounds) == REACH_OK;
            float row_height = ceilf(paint_bounds.height) + 2;
            float alignment = first.text_alignment == REACH_TEXT_ALIGNMENT_CENTER     ? .5f
                              : first.text_alignment == REACH_TEXT_ALIGNMENT_TRAILING ? 1
                                                                                      : 0;
            for (size_t begin = 0; valid && begin < layout->widths.size(); begin += 7)
            {
                size_t end = std::min(begin + 7, layout->widths.size());
                float thresholds[8] = {}, values[9] = {};
                size_t count = 0;
                values[0] = begin > 0 ? row_height : 0;
                for (size_t at = begin > 0 ? begin - 1 : begin;
                     at < std::min(end, layout->thresholds.size()); ++at)
                {
                    thresholds[count++] = layout->thresholds[at];
                    size_t state = at + 1;
                    values[count] = state >= begin && state < end
                                        ? -(float)(state - begin) * row_height
                                        : row_height;
                }
                retained_plan part = {};
                part.group.id = fit.prefix | (uint64_t)(begin / 7 + 1);
                part.group.parent_id = parent;
                part.group.bounds = paint_bounds;
                part.group.bounds.height = (float)(end - begin) * row_height;
                part.group.has_clip = 1;
                part.group.clip = paint_bounds;
                part.group.x =
                    reach_motion_affine(&fit.width, alignment, -alignment * first.rect.width);
                valid = reach_motion_lookup(&fit.width, thresholds, values, count, &part.group.y) ==
                        REACH_OK;
                if (!valid)
                    break;
                if (part.group.bounds.width > REACH_D2D_MAX_SURFACE_DIMENSION ||
                    part.group.bounds.height > REACH_D2D_MAX_SURFACE_DIMENSION)
                {
                    valid = false;
                    break;
                }
                float minimum = 0, maximum = 0;
                if (reach_motion_range(&part.group.y, &minimum, &maximum) == REACH_OK &&
                    minimum == row_height && maximum == row_height &&
                    std::none_of(backend->retained_layers.begin(), backend->retained_layers.end(),
                                 [&](const reach_d2d_retained_layer &layer)
                                 { return layer.id == part.group.id; }))
                    continue;
                for (size_t at = begin; at < end; ++at)
                {
                    reach_render_command text = first;
                    text.text_fit_to_clip = 0;
                    text.rect.width = layout->widths[at];
                    text.rect.x += alignment * (first.rect.width - text.rect.width);
                    text.rect.y += (float)(at - begin) * row_height;
                    text.has_scissor = 1;
                    text.scissor_rect = {paint_bounds.x,
                                         paint_bounds.y + (float)(at - begin) * row_height,
                                         paint_bounds.width, paint_bounds.height};
                    part.owned.push_back(text);
                }
                part.commands = part.owned.data();
                part.count = part.owned.size();
                parts.push_back(std::move(part));
            }
            if (valid)
            {
                auto base = plan_for(group.id);
                base->commands = nullptr;
                base->count = 0;
                if (overflow)
                {
                    base->group.clip.width += base->group.clip.x - paint_bounds.x;
                    base->group.clip.x = paint_bounds.x;
                    base->group.clip.y = paint_bounds.y;
                    base->group.clip.height = paint_bounds.height;
                }
                for (auto &part : parts)
                    plans.push_back(std::move(part));
                fit.expanded = true;
            }
            else if (sample_text(fit) != REACH_OK)
                return REACH_ERROR;
            fitted_text.push_back(std::move(fit));
        }
        if ((first.caret_group_id != 0 || first.text_color_group_id != 0) &&
            group.bounds.width > 0 && group.bounds.height > 0)
        {
            auto base = plan_for(group.id);
            base->owned.assign(1, first);
            base->owned[0].caret_visible = 0;
            base->owned[0].caret_index = 0;
            base->owned[0].caret_group_id = 0;
            base->owned[0].text_color_group_id = 0;
            const reach_render_group *text_color =
                reach_render_command_buffer_group(commands, first.text_color_group_id);
            std::vector<retained_plan> text_parts;
            if (text_color != nullptr)
            {
                base->owned[0].text_color = {};
                bool placeholder = first.text[0] == 0 && first.placeholder[0] != 0 &&
                                   first.placeholder_color.a > 0;
                bool selection = first.text[0] != 0 && first.selection_color.a > 0 &&
                                 first.selection_start != first.selection_end;
                if (first.color.a <= 0 && !placeholder && !selection)
                {
                    base->owned.clear();
                    base->commands = nullptr;
                    base->count = 0;
                }
                retained_plan ink = {};
                ink.group = *text_color;
                ink.group.parent_id = group.id;
                ink.group.bounds = group.bounds;
                ink.owned.assign(1, first);
                ink.owned[0].color = {};
                ink.owned[0].text_color = {1, 1, 1, 1};
                ink.owned[0].placeholder_color = {};
                ink.owned[0].selection_color = {};
                ink.owned[0].caret_group_id = 0;
                ink.owned[0].text_color_group_id = 0;
                if (first.caret_group_id != 0)
                    ink.owned[0].caret_visible = 0;
                ink.commands = ink.owned.data();
                ink.count = 1;
                if (first.text[0] != 0 || (first.caret_group_id == 0 && first.caret_visible != 0))
                    text_parts.push_back(std::move(ink));
            }
            base->commands = base->owned.empty() ? nullptr : base->owned.data();
            base->count = base->owned.size();
            reach_render_command line = {};
            if (first.caret_group_id != 0 &&
                reach_d2d_textbox_caret_line(backend, &first, &line) != REACH_OK)
                return REACH_ERROR;
            if (text_color != nullptr)
                line.color = {1, 1, 1, 1};
            if (line.type != REACH_RENDER_COMMAND_NONE && line.rect.height > 0 && line.color.a > 0)
            {
                line.has_scissor = first.has_scissor;
                line.scissor_rect = first.scissor_rect;
                reach_rect_f32 ink = run_bounds(&line, 1);
                reach_rect_f32 raster = {group.bounds.x, group.bounds.y, ceilf(group.bounds.width),
                                         ceilf(group.bounds.height)};
                float right = std::min(ink.x + ink.width, raster.x + raster.width);
                float bottom = std::min(ink.y + ink.height, raster.y + raster.height);
                ink.x = std::max(ink.x, raster.x);
                ink.y = std::max(ink.y, raster.y);
                ink.width = std::max(0.0f, right - ink.x);
                ink.height = std::max(0.0f, bottom - ink.y);
                if (ink.width > 0 && ink.height > 0)
                {
                    retained_plan caret = {};
                    caret.group =
                        *reach_render_command_buffer_group(commands, first.caret_group_id);
                    if (text_color != nullptr)
                    {
                        caret.group.has_tint = 1;
                        for (size_t channel = 0; channel < 4; ++channel)
                            caret.group.tint[channel] = text_color->tint[channel];
                        caret.group.tint_rgb_target = text_color->tint_rgb_target;
                        caret.group.tint_rgb_mix = text_color->tint_rgb_mix;
                        line.color = {1, 1, 1, 1};
                    }
                    caret.group.parent_id = group.id;
                    caret.group.bounds = ink;
                    caret.group.has_clip = 1;
                    caret.group.clip = raster;
                    caret.owned.push_back(line);
                    caret.commands = caret.owned.data();
                    caret.count = 1;
                    text_parts.push_back(std::move(caret));
                }
            }
            if (text_color != nullptr &&
                (first.text[0] != 0 || (first.caret_group_id == 0 && first.caret_visible != 0)))
                fitted_textboxes.push_back(
                    {{group.id, first, *text_color}, text_color->id, base->count != 0, true});
            for (auto &part : text_parts)
                plans.push_back(std::move(part));
        }
        if (limit_fit_sources() != REACH_OK)
            return REACH_ERROR;
    }
    backend->image_fit_owners.erase(
        std::remove_if(backend->image_fit_owners.begin(), backend->image_fit_owners.end(),
                       [&](const reach_d2d_fit_owner &owner)
                       {
                           return std::none_of(fitted_images.begin(), fitted_images.end(),
                                               [&](const image_fit &fit)
                                               { return fit.source.owner == owner.owner; });
                       }),
        backend->image_fit_owners.end());
    backend->text_width_owners.erase(
        std::remove_if(backend->text_width_owners.begin(), backend->text_width_owners.end(),
                       [&](const reach_d2d_fit_owner &owner)
                       {
                           return std::none_of(fitted_text.begin(), fitted_text.end(),
                                               [&](const text_fit &fit)
                                               { return fit.owner == owner.owner; });
                       }),
        backend->text_width_owners.end());
    auto align_ink_sources = [&]()
    {
        for (auto &plan : plans)
        {
            plan.raster_birth_aligned = false;
            plan.raster_finite_translation = false;
            if (plan.count == 0 || plan.group.has_shadow ||
                !std::all_of(plan.commands, plan.commands + plan.count,
                             [](const reach_render_command &paint)
                             {
                                 return paint.type == REACH_RENDER_COMMAND_TEXT ||
                                        paint.type == REACH_RENDER_COMMAND_VECTOR_ICON;
                             }) ||
                std::any_of(plans.begin(), plans.end(), [&](const retained_plan &child)
                            { return child.group.parent_id == plan.group.id; }))
                continue;
            auto requested =
                plan.requested_bounds_valid ? plan.requested_bounds : plan.group.bounds;
            plan.group.bounds = requested;
            auto unit = [](const reach_motion &motion)
            {
                if (motion.generation == 0)
                    return true;
                float minimum = 0, maximum = 0;
                return reach_motion_range(&motion, &minimum, &maximum) == REACH_OK &&
                       minimum == 1 && maximum == 1;
            };
            bool valid = commands->content_transform.scale_x == 1 &&
                         commands->content_transform.scale_y == 1;
            double now = motion_sample_time(backend);
            float x = commands->content_transform.offset_x,
                  y = commands->content_transform.offset_y;
            bool finite_translation = false, constant_translation = true;
            auto inspect_translation = [&](const reach_render_group &transform)
            {
                for (const auto *axis : {&transform.x, &transform.y})
                {
                    if (axis->generation == 0)
                        continue;
                    finite_translation =
                        finite_translation || (axis->duration_seconds > 0 && !axis->repeating);
                    constant_translation =
                        constant_translation && axis->duration_seconds <= 0 && !axis->repeating;
                }
            };
            const reach_render_group *group = &plan.group;
            size_t depth = 0;
            while (valid && group != nullptr && depth++ <= plans.size())
            {
                inspect_translation(*group);
                valid = unit(group->scale) && unit(reach_render_group_y_scale(group));
                x += reach_motion_value_at(&group->x, now);
                y += reach_motion_value_at(&group->y, now);
                if (group->parent_id == 0)
                    group = nullptr;
                else
                {
                    auto parent = plan_for(group->parent_id);
                    if (parent == plans.end())
                    {
                        valid = false;
                        break;
                    }
                    group = &parent->group;
                }
            }
            valid = valid && group == nullptr;
            if (valid && commands->has_presentation)
            {
                const auto &root = commands->presentation;
                inspect_translation(root);
                valid = unit(root.scale) && unit(reach_render_group_y_scale(&root));
                x += reach_motion_value_at(&root.x, now);
                y += reach_motion_value_at(&root.y, now);
            }
            plan.requested_bounds = requested;
            plan.requested_bounds_valid = true;
            if (!valid)
                continue;
            plan.raster_finite_translation = finite_translation;
            auto previous = std::find_if(
                backend->retained_layers.begin(), backend->retained_layers.end(),
                [&](const reach_d2d_retained_layer &layer) { return layer.id == plan.group.id; });
            bool same =
                previous != backend->retained_layers.end() && previous->raster_birth_aligned &&
                !(previous->raster_finite_translation && constant_translation) &&
                memcmp(&requested, &previous->raster_requested_bounds, sizeof(requested)) == 0 &&
                previous->commands.size() == plan.count;
            for (size_t at = 0; same && at < plan.count; ++at)
            {
                auto paint = plan.commands[at];
                localize(paint, previous->group.bounds);
                same = memcmp(&paint, &previous->commands[at], sizeof(paint)) == 0;
            }
            if (same)
            {
                plan.group.bounds = previous->group.bounds;
                plan.raster_birth_aligned = true;
                continue;
            }
            auto bounds = requested;
            float left = floorf(bounds.x + x), top = floorf(bounds.y + y);
            bounds.width = ceilf(bounds.x + x + bounds.width) - left;
            bounds.height = ceilf(bounds.y + y + bounds.height) - top;
            bounds.x = left - x;
            bounds.y = top - y;
            if (bounds.width > 0 && bounds.height > 0 &&
                bounds.width <= REACH_D2D_MAX_SURFACE_DIMENSION &&
                bounds.height <= REACH_D2D_MAX_SURFACE_DIMENSION)
            {
                plan.group.bounds = bounds;
                plan.raster_birth_aligned = true;
            }
        }
    };
    auto expanded_fit_count = [&]()
    {
        auto expanded = [](const auto &fit) { return fit.expanded; };
        return std::count_if(fitted_text.begin(), fitted_text.end(), expanded) +
               std::count_if(fitted_images.begin(), fitted_images.end(), expanded) +
               std::count_if(fitted_textboxes.begin(), fitted_textboxes.end(), expanded);
    };
    align_ink_sources();
    while (planned_bytes() > REACH_D2D_MAX_SURFACE_BYTES ||
           plans.size() > REACH_MAX_RENDER_GROUPS * 2 + 1)
    {
        auto expanded = expanded_fit_count();
        if (limit_fit_sources() != REACH_OK || expanded_fit_count() >= expanded)
            return REACH_ERROR;
        align_ink_sources();
    }
    for (const auto &fit : fitted_textboxes)
        if (!fit.expanded &&
            reach_d2d_textbox_color_active(fit.source.color, motion_sample_time(backend)))
            backend->sampled_textboxes.push_back(fit.source);
    for (const auto &fit : fitted_text)
        if (!fit.expanded && reach_motion_active_at(&fit.width, motion_sample_time(backend)))
            backend->sampled_text.push_back({fit.owner, fit.command, fit.width});
    for (const auto &fit : fitted_images)
        if (!fit.expanded)
        {
            bool active = false;
            for (const reach_motion *motion : {&fit.source.bounds.x, &fit.source.bounds.y,
                                               &fit.source.bounds.width, &fit.source.bounds.height})
                active = active || reach_motion_active_at(motion, motion_sample_time(backend));
            if (active)
                backend->sampled_images.push_back(fit.source);
        }

    bool topology_changed =
        !backend->retained_topology_valid || plans.size() != backend->retained_topology.size() ||
        !std::equal(plans.begin(), plans.end(), backend->retained_topology.begin(),
                    [](const retained_plan &left, const reach_d2d_retained_attachment &right)
                    {
                        return left.group.id == right.id &&
                               left.group.parent_id == right.parent_id &&
                               left.group.has_shadow == right.has_shadow;
                    });
    ComPtr<IVisualCollection> children;
    if (topology_changed)
    {
        backend->retained_topology_valid = false;
        if (backend->compositor != nullptr)
        {
            hr = backend->scene_visual->get_Children(&children);
            if (SUCCEEDED(hr))
            {
                reach_workload_add(REACH_WORKLOAD_VISUAL_TREE_MUTATIONS, 1);
                hr = children->RemoveAll();
            }
        }
        else
        {
            reach_workload_add(REACH_WORKLOAD_VISUAL_TREE_MUTATIONS, 1);
            hr = backend->dcomp_scene_visual->RemoveAllVisuals();
        }
        if (FAILED(hr))
            return REACH_ERROR;
        for (auto &layer : backend->retained_layers)
        {
            if (layer.has_children)
            {
                if (backend->compositor != nullptr)
                {
                    ComPtr<IContainerVisual> content;
                    ComPtr<IVisualCollection> descendants;
                    if (auto *owner = reach_d2d_shadow_wuc_owner(layer))
                        content = owner;
                    else
                        hr = layer.visual.As(&content);
                    if (SUCCEEDED(hr))
                        hr = content->get_Children(&descendants);
                    if (SUCCEEDED(hr))
                    {
                        reach_workload_add(REACH_WORKLOAD_VISUAL_TREE_MUTATIONS, 1);
                        hr = descendants->RemoveAll();
                    }
                }
                else
                {
                    reach_workload_add(REACH_WORKLOAD_VISUAL_TREE_MUTATIONS, 1);
                    auto *owner = reach_d2d_shadow_dcomp_owner(layer);
                    hr = (owner ? owner : layer.dcomp_visual.Get())->RemoveAllVisuals();
                }
                if (FAILED(hr))
                    return REACH_ERROR;
            }
            layer.has_children = false;
        }
    }
    for (auto &layer : backend->retained_layers)
        if (plan_for(layer.id) == plans.end())
            stop_layer_repetition(backend, layer);
    backend->retained_layers.erase(std::remove_if(backend->retained_layers.begin(),
                                                  backend->retained_layers.end(),
                                                  [&](const reach_d2d_retained_layer &layer)
                                                  { return plan_for(layer.id) == plans.end(); }),
                                   backend->retained_layers.end());
    for (const retained_plan &plan : plans)
    {
        auto found = std::find_if(backend->retained_layers.begin(), backend->retained_layers.end(),
                                  [&](const reach_d2d_retained_layer &layer)
                                  { return layer.id == plan.group.id; });
        if (found == backend->retained_layers.end())
        {
            if (backend->retained_layers.size() >= REACH_MAX_RENDER_GROUPS * 2 + 1)
                return REACH_ERROR;
            backend->retained_layers.emplace_back();
            found = backend->retained_layers.end() - 1;
            found->id = plan.group.id;
            hr = create_layer(backend, *found);
        }
        reach_point_f32 origin = {};
        if (plan.group.parent_id != 0)
        {
            const reach_render_group &parent = plan_for(plan.group.parent_id)->group;
            origin = {parent.bounds.x, parent.bounds.y};
        }
        bool allow_text_snap =
            (!commands->has_presentation || text_snap_transform(commands->presentation)) &&
            (commands->content_transform.scale_x == 1 &&
             commands->content_transform.scale_y == 1) &&
            text_snap_transform(plan.group) &&
            std::none_of(plans.begin(), plans.end(), [&](const retained_plan &candidate)
                         { return candidate.group.parent_id == plan.group.id; });
        uint64_t ancestor_id = plan.group.parent_id;
        while (ancestor_id != 0 && allow_text_snap)
        {
            const reach_render_group &ancestor = plan_for(ancestor_id)->group;
            allow_text_snap = text_snap_transform(ancestor);
            ancestor_id = ancestor.parent_id;
        }
        found->raster_birth_aligned = false;
        found->raster_finite_translation = false;
        if (SUCCEEDED(hr))
            hr = sync_layer(backend, *found, plan.group, plan.commands, plan.count, origin,
                            allow_text_snap);
        if (FAILED(hr))
            return REACH_ERROR;
    }
    if (topology_changed)
    {
        for (const retained_plan &plan : plans)
        {
            auto found = std::find_if(
                backend->retained_layers.begin(), backend->retained_layers.end(),
                [&](const reach_d2d_retained_layer &layer) { return layer.id == plan.group.id; });
            reach_d2d_retained_layer *parent = nullptr;
            if (plan.group.parent_id != 0)
            {
                auto owner =
                    std::find_if(backend->retained_layers.begin(), backend->retained_layers.end(),
                                 [&](const reach_d2d_retained_layer &layer)
                                 { return layer.id == plan.group.parent_id; });
                parent = &*owner;
                parent->has_children = true;
            }
            if (backend->compositor != nullptr)
            {
                ComPtr<IVisualCollection> siblings = children;
                ComPtr<IContainerVisual> owner;
                ComPtr<IVisual> visual;
                if (parent != nullptr)
                {
                    if (auto *caster = reach_d2d_shadow_wuc_owner(*parent))
                        owner = caster;
                    else
                        hr = parent->visual.As(&owner);
                    if (SUCCEEDED(hr))
                        hr = owner->get_Children(&siblings);
                }
                if (SUCCEEDED(hr))
                    hr = found->container.As(&visual);
                if (SUCCEEDED(hr))
                {
                    reach_workload_add(REACH_WORKLOAD_VISUAL_TREE_MUTATIONS, 1);
                    hr = siblings->InsertAtTop(visual.Get());
                }
            }
            else
            {
                IDCompositionVisual2 *owner = parent != nullptr
                                                  ? reach_d2d_shadow_dcomp_owner(*parent)
                                                  : backend->dcomp_scene_visual.Get();
                if (parent != nullptr && owner == nullptr)
                    owner = parent->dcomp_visual.Get();
                reach_workload_add(REACH_WORKLOAD_VISUAL_TREE_MUTATIONS, 1);
                hr = owner->AddVisual(found->dcomp_container.Get(), FALSE, nullptr);
            }
            if (FAILED(hr))
                return REACH_ERROR;
        }
        backend->retained_topology.clear();
        backend->retained_topology.reserve(plans.size());
        for (const auto &plan : plans)
            backend->retained_topology.push_back(
                {plan.group.id, plan.group.parent_id, plan.group.has_shadow});
        backend->retained_topology_valid = true;
    }
    reach_render_group presentation =
        commands->has_presentation ? commands->presentation : reach_render_group{};
    reach_motion center_x = commands->presentation_center_x;
    reach_motion center_y = commands->presentation_center_y;
    bool center_changed = !backend->has_scene_presentation ||
                          !reach_motion_equal(&center_x, &backend->scene_center_x) ||
                          !reach_motion_equal(&center_y, &backend->scene_center_y);
    if (SUCCEEDED(hr) &&
        (center_changed || !reach_render_group_equal(&presentation, &backend->scene_presentation)))
    {
        bool animation_changed =
            center_changed || !backend->has_scene_presentation ||
            !reach_motion_equal(&presentation.opacity, &backend->scene_presentation.opacity) ||
            !reach_motion_equal(&presentation.x, &backend->scene_presentation.x) ||
            !reach_motion_equal(&presentation.y, &backend->scene_presentation.y) ||
            !reach_motion_equal(&presentation.scale, &backend->scene_presentation.scale) ||
            !reach_motion_equal(&presentation.scale_y, &backend->scene_presentation.scale_y);
        if (animation_changed)
        {
            if (backend->presentation_batch != nullptr)
                (void)backend->presentation_batch->remove_Completed(backend->presentation_token);
            backend->presentation_batch.Reset();
            backend->presentation_commit.Reset();
            backend->presentation_completed.reset();
            backend->submitted_presentation_generation = presentation.opacity.generation;
            backend->reconciled_presentation_generation = 0;
            backend->presentation_result = REACH_OK;
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
        if (SUCCEEDED(hr))
            hr = presentation_center(backend, presentation, center_x, center_y);
        if (SUCCEEDED(hr))
        {
            unsigned mask = animation_changed
                                ? 31
                                : property_mask(presentation, backend->scene_presentation, 0, 0,
                                                backend->has_scene_presentation != 0);
            hr = backend->root_visual != nullptr
                     ? wuc_properties(backend, backend->root_visual.Get(), presentation, 0, 0, mask)
                     : dcomp_properties(backend, backend->dcomp_visual, backend->scene_scale.Get(),
                                        presentation, 0, 0, mask);
        }
        if (SUCCEEDED(hr))
        {
            backend->scene_presentation = presentation;
            backend->scene_center_x = center_x;
            backend->scene_center_y = center_y;
            backend->has_scene_presentation = 1;
        }
        if (animation_changed && backend->presentation_batch != nullptr)
        {
            HRESULT end_hr = backend->presentation_batch->End();
            if (SUCCEEDED(hr))
                hr = end_hr;
        }
    }
    if (SUCCEEDED(hr))
        for (auto &layer : backend->retained_layers)
        {
            const auto &plan = *plan_for(layer.id);
            layer.raster_requested_bounds = plan.requested_bounds;
            layer.raster_birth_aligned = plan.raster_birth_aligned;
            layer.raster_finite_translation = plan.raster_finite_translation;
        }
    record_retained_bytes(backend);
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}
