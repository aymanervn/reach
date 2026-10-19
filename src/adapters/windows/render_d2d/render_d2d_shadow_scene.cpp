#include "render_d2d_internal.h"
#include "reach/support/workload.h"

#include <algorithm>
#include <cmath>
#include <d2d1effects.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <winrt/Windows.UI.Composition.h>
#include <windows.graphics.effects.interop.h>

namespace composition = winrt::Windows::UI::Composition;
using effect_source = winrt::Windows::Graphics::Effects::IGraphicsEffectSource;
using effect_value = winrt::Windows::Foundation::IInspectable;
using ABI::Windows::Graphics::Effects::GRAPHICS_EFFECT_PROPERTY_MAPPING;
using ABI::Windows::Graphics::Effects::IGraphicsEffectD2D1Interop;

struct reach_d2d_shadow_scene
{
    composition::ContainerVisual caster{nullptr};
    composition::SpriteVisual output{nullptr};
    composition::CompositionVisualSurface surface{nullptr};
    composition::CompositionEffectFactory factory{nullptr};
    composition::CompositionEffectBrush brush{nullptr};
    ComPtr<IDCompositionVisual2> dcomp_caster;
    ComPtr<IDCompositionVisual2> dcomp_output;
    ComPtr<IDCompositionRectangleClip> dcomp_clip;
    ComPtr<IDCompositionShadowEffect> dcomp_shadow;
    ComPtr<IDCompositionAffineTransform2DEffect> dcomp_shift;
    ComPtr<IDCompositionAffineTransform2DEffect> dcomp_identity;
    ComPtr<IDCompositionCompositeEffect> dcomp_cutout;
};

namespace
{
struct shadow_effect
    : winrt::implements<shadow_effect, winrt::Windows::Graphics::Effects::IGraphicsEffect,
                        effect_source, IGraphicsEffectD2D1Interop>
{
    GUID id;
    winrt::hstring name;
    std::vector<effect_value> properties;
    std::vector<effect_source> sources;

    shadow_effect(GUID effect_id, winrt::hstring label, std::vector<effect_value> values,
                  std::vector<effect_source> inputs)
        : id(effect_id), name(std::move(label)), properties(std::move(values)),
          sources(std::move(inputs))
    {
    }
    winrt::hstring Name() const
    {
        return name;
    }
    void Name(winrt::hstring const &value)
    {
        name = value;
    }
    HRESULT __stdcall GetEffectId(GUID *out) noexcept override
    {
        if (!out)
            return E_POINTER;
        *out = id;
        return S_OK;
    }
    HRESULT __stdcall GetNamedPropertyMapping(LPCWSTR, UINT *,
                                              GRAPHICS_EFFECT_PROPERTY_MAPPING *) noexcept override
    {
        return E_INVALIDARG;
    }
    HRESULT __stdcall GetPropertyCount(UINT *out) noexcept override
    {
        if (!out)
            return E_POINTER;
        *out = (UINT)properties.size();
        return S_OK;
    }
    HRESULT __stdcall GetProperty(UINT index,
                                  ABI::Windows::Foundation::IPropertyValue **out) noexcept override
    {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (index >= properties.size())
            return E_INVALIDARG;
        try
        {
            auto value = properties[index].as<winrt::Windows::Foundation::IPropertyValue>();
            *out =
                reinterpret_cast<ABI::Windows::Foundation::IPropertyValue *>(winrt::get_abi(value));
            (*out)->AddRef();
            return S_OK;
        }
        catch (...)
        {
            return winrt::to_hresult();
        }
    }
    HRESULT __stdcall GetSourceCount(UINT *out) noexcept override
    {
        if (!out)
            return E_POINTER;
        *out = (UINT)sources.size();
        return S_OK;
    }
    HRESULT __stdcall
    GetSource(UINT index,
              ABI::Windows::Graphics::Effects::IGraphicsEffectSource **out) noexcept override
    {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (index >= sources.size())
            return E_INVALIDARG;
        *out = reinterpret_cast<ABI::Windows::Graphics::Effects::IGraphicsEffectSource *>(
            winrt::get_abi(sources[index]));
        (*out)->AddRef();
        return S_OK;
    }
};

void configure_wuc(composition::Compositor const &compositor, reach_d2d_shadow_scene &scene,
                   reach_render_group const &group)
{
    using winrt::Windows::Foundation::PropertyValue;
    const reach_shadow &shadow = group.shadow;
    auto caster = compositor.CreateSurfaceBrush(scene.surface);
    caster.Stretch(composition::CompositionStretch::None);
    auto shifted = compositor.CreateSurfaceBrush(scene.surface);
    shifted.Stretch(composition::CompositionStretch::None);
    shifted.Offset({shadow.offset_x, shadow.offset_y});
    auto blur = winrt::make<shadow_effect>(
        CLSID_D2D1GaussianBlur, L"Blur",
        std::vector<effect_value>{
            PropertyValue::CreateSingle(shadow.blur * .5f),
            PropertyValue::CreateUInt32(D2D1_GAUSSIANBLUR_OPTIMIZATION_BALANCED),
            PropertyValue::CreateUInt32(D2D1_BORDER_MODE_SOFT)},
        std::vector<effect_source>{composition::CompositionEffectSourceParameter(L"Shifted")});
    float matrix[20] = {};
    matrix[15] = shadow.color.a;
    matrix[16] = shadow.color.r;
    matrix[17] = shadow.color.g;
    matrix[18] = shadow.color.b;
    auto color = winrt::make<shadow_effect>(
        CLSID_D2D1ColorMatrix, L"Color",
        std::vector<effect_value>{
            PropertyValue::CreateSingleArray(matrix),
            PropertyValue::CreateUInt32(D2D1_COLORMATRIX_ALPHA_MODE_PREMULTIPLIED),
            PropertyValue::CreateBoolean(true)},
        std::vector<effect_source>{blur.as<effect_source>()});
    auto cutout = winrt::make<shadow_effect>(
        CLSID_D2D1Composite, L"Cutout",
        std::vector<effect_value>{PropertyValue::CreateUInt32(D2D1_COMPOSITE_MODE_DESTINATION_OUT)},
        std::vector<effect_source>{color.as<effect_source>(),
                                   composition::CompositionEffectSourceParameter(L"Caster")});
    auto factory = compositor.CreateEffectFactory(cutout);
    auto brush = factory.CreateBrush();
    brush.SetSourceParameter(L"Caster", caster);
    brush.SetSourceParameter(L"Shifted", shifted);
    scene.output.Brush(brush);
    scene.factory = factory;
    scene.brush = brush;
    scene.caster.Size({group.bounds.width, group.bounds.height});
    scene.surface.SourceSize({group.bounds.width, group.bounds.height});
    scene.output.Size({group.bounds.width, group.bounds.height});
}

HRESULT configure_dcomp(reach_d2d_shadow_scene &scene, reach_render_group const &group)
{
    const reach_shadow &shadow = group.shadow;
    HRESULT hr = scene.dcomp_shadow->SetStandardDeviation(shadow.blur * .5f);
    if (SUCCEEDED(hr))
        hr = scene.dcomp_shadow->SetColor(
            D2D1::Vector4F(shadow.color.r, shadow.color.g, shadow.color.b, shadow.color.a));
    if (SUCCEEDED(hr))
        hr = scene.dcomp_shift->SetTransformMatrix({1, 0, 0, 1, shadow.offset_x, shadow.offset_y});
    if (SUCCEEDED(hr))
        hr = scene.dcomp_clip->SetLeft(0.0f);
    if (SUCCEEDED(hr))
        hr = scene.dcomp_clip->SetTop(0.0f);
    if (SUCCEEDED(hr))
        hr = scene.dcomp_clip->SetRight(group.bounds.width);
    if (SUCCEEDED(hr))
        hr = scene.dcomp_clip->SetBottom(group.bounds.height);
    return hr;
}

bool shadow_changed(reach_render_group const &left, reach_render_group const &right)
{
    const reach_shadow &a = left.shadow, &b = right.shadow;
    return left.bounds.width != right.bounds.width || left.bounds.height != right.bounds.height ||
           a.offset_x != b.offset_x || a.offset_y != b.offset_y || a.blur != b.blur ||
           a.color.r != b.color.r || a.color.g != b.color.g || a.color.b != b.color.b ||
           a.color.a != b.color.a;
}
} // namespace

ABI::Windows::UI::Composition::IContainerVisual *
reach_d2d_shadow_wuc_owner(reach_d2d_retained_layer &layer)
{
    return layer.shadow_scene ? reinterpret_cast<ABI::Windows::UI::Composition::IContainerVisual *>(
                                    winrt::get_abi(layer.shadow_scene->caster))
                              : nullptr;
}

IDCompositionVisual2 *reach_d2d_shadow_dcomp_owner(reach_d2d_retained_layer &layer)
{
    return layer.shadow_scene ? layer.shadow_scene->dcomp_caster.Get() : nullptr;
}

HRESULT reach_d2d_sync_shadow(reach_render_backend *backend, reach_d2d_retained_layer &layer,
                              const reach_render_group &group)
{
    try
    {
        if (!group.has_shadow)
        {
            if (!layer.shadow_scene)
                return S_OK;
            if (backend->compositor)
            {
                composition::SpriteVisual parent{nullptr};
                winrt::copy_from_abi(parent, layer.visual.Get());
                parent.Children().Remove(layer.shadow_scene->output);
            }
            else
                winrt::check_hresult(
                    layer.dcomp_visual->RemoveVisual(layer.shadow_scene->dcomp_output.Get()));
            reach_workload_add(REACH_WORKLOAD_VISUAL_TREE_MUTATIONS, 1);
            layer.shadow_scene.reset();
            layer.shadow_bytes = 0;
            return S_OK;
        }
        if (layer.shadow_scene && !shadow_changed(group, layer.group))
            return S_OK;
        auto scene = layer.shadow_scene;
        bool created = !scene;
        if (created)
            scene = std::make_shared<reach_d2d_shadow_scene>();
        if (backend->compositor)
        {
            composition::Compositor compositor{nullptr};
            winrt::copy_from_abi(compositor, backend->compositor.Get());
            if (created)
            {
                scene->caster = compositor.CreateContainerVisual();
                scene->surface = compositor.CreateVisualSurface();
                scene->surface.SourceVisual(scene->caster);
                scene->output = compositor.CreateSpriteVisual();
            }
            configure_wuc(compositor, *scene, group);
            if (created)
            {
                composition::SpriteVisual parent{nullptr};
                winrt::copy_from_abi(parent, layer.visual.Get());
                parent.Children().InsertAtTop(scene->output);
            }
        }
        else
        {
            if (created)
            {
                ComPtr<IDCompositionDevice3> device;
                winrt::check_hresult(backend->dcomp_device->QueryInterface(IID_PPV_ARGS(&device)));
                winrt::check_hresult(device->CreateVisual(&scene->dcomp_caster));
                winrt::check_hresult(device->CreateVisual(&scene->dcomp_output));
                winrt::check_hresult(device->CreateRectangleClip(&scene->dcomp_clip));
                winrt::check_hresult(device->CreateShadowEffect(&scene->dcomp_shadow));
                winrt::check_hresult(device->CreateAffineTransform2DEffect(&scene->dcomp_shift));
                winrt::check_hresult(device->CreateAffineTransform2DEffect(&scene->dcomp_identity));
                winrt::check_hresult(scene->dcomp_identity->SetTransformMatrix({1, 0, 0, 1, 0, 0}));
                winrt::check_hresult(device->CreateCompositeEffect(&scene->dcomp_cutout));
                winrt::check_hresult(scene->dcomp_shadow->SetInput(0, scene->dcomp_shift.Get(), 0));
                winrt::check_hresult(
                    scene->dcomp_cutout->SetMode(D2D1_COMPOSITE_MODE_DESTINATION_OUT));
                winrt::check_hresult(
                    scene->dcomp_cutout->SetInput(0, scene->dcomp_shadow.Get(), 0));
                winrt::check_hresult(
                    scene->dcomp_cutout->SetInput(1, scene->dcomp_identity.Get(), 0));
                winrt::check_hresult(scene->dcomp_caster->SetClip(scene->dcomp_clip.Get()));
                winrt::check_hresult(scene->dcomp_output->SetClip(scene->dcomp_clip.Get()));
                winrt::check_hresult(
                    scene->dcomp_output->AddVisual(scene->dcomp_caster.Get(), FALSE, nullptr));
                winrt::check_hresult(scene->dcomp_output->SetEffect(scene->dcomp_cutout.Get()));
            }
            winrt::check_hresult(configure_dcomp(*scene, group));
            if (created)
                winrt::check_hresult(
                    layer.dcomp_visual->AddVisual(scene->dcomp_output.Get(), FALSE, nullptr));
        }
        reach_workload_add(created ? REACH_WORKLOAD_VISUAL_TREE_MUTATIONS
                                   : REACH_WORKLOAD_PROPERTY_SUBMISSIONS,
                           1);
        layer.shadow_scene = std::move(scene);
        layer.shadow_bytes =
            (uint64_t)ceilf(group.bounds.width) * (uint64_t)ceilf(group.bounds.height) * 4;
        return S_OK;
    }
    catch (...)
    {
        return winrt::to_hresult();
    }
}

namespace
{
HRESULT shadow_filter_context(reach_render_backend *backend)
{
    if (backend->shadow_fallback_context &&
        backend->shadow_fallback_parent_device == backend->d2d_device)
        return S_OK;
    backend->shadow_fallback_context.Reset();
    backend->shadow_fallback_parent_device = nullptr;
    if (backend->d2d_device)
    {
        HRESULT hr = backend->d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                                              &backend->shadow_fallback_context);
        if (SUCCEEDED(hr))
        {
            backend->shadow_fallback_parent_device = backend->d2d_device;
            backend->shadow_fallback_context->SetDpi(96, 96);
        }
        return hr;
    }
    ComPtr<ID3D11Device> device;
    HRESULT hr =
        D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                          nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, nullptr);
    ComPtr<IDXGIDevice> dxgi;
    ComPtr<ID2D1Factory1> factory;
    ComPtr<ID2D1Device> drawing;
    if (SUCCEEDED(hr))
        hr = device.As(&dxgi);
    if (SUCCEEDED(hr))
        hr = backend->factory->QueryInterface(IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr))
        hr = factory->CreateDevice(dxgi.Get(), &drawing);
    if (SUCCEEDED(hr))
        hr = drawing->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                          &backend->shadow_fallback_context);
    if (SUCCEEDED(hr))
        backend->shadow_fallback_context->SetDpi(96, 96);
    return hr;
}

struct shadow_target_scope
{
    reach_render_backend *backend;
    ID2D1RenderTarget *target;
    ID2D1DeviceContext *context;
    shadow_target_scope(reach_render_backend *value, ID2D1DeviceContext *source)
        : backend(value), target(value->command_target), context(value->d2d_context)
    {
        backend->command_target = source;
        backend->d2d_context = source;
    }
    ~shadow_target_scope()
    {
        backend->command_target = target;
        backend->d2d_context = context;
    }
};
} // namespace

reach_result reach_d2d_draw_group_shadow(reach_render_backend *backend,
                                         const reach_render_command_buffer *commands,
                                         const reach_render_group &group, size_t first,
                                         size_t count, float opacity, double now_seconds)
{
    if (group.bounds.width > REACH_D2D_MAX_SURFACE_DIMENSION ||
        group.bounds.height > REACH_D2D_MAX_SURFACE_DIMENSION)
        return REACH_INVALID_ARGUMENT;
    UINT width = (UINT)ceilf(group.bounds.width), height = (UINT)ceilf(group.bounds.height);
    if ((uint64_t)width * height * 16 > REACH_D2D_MAX_SURFACE_BYTES)
        return REACH_ERROR;
    if (opacity <= 0 || group.shadow.color.a <= 0)
        return REACH_OK;
    if (FAILED(shadow_filter_context(backend)))
        return REACH_ERROR;
    try
    {
        auto source_commands = std::make_unique<reach_render_command_buffer>(*commands);
        source_commands->has_presentation = 0;
        source_commands->presentation_center_x = source_commands->presentation_center_y = {};
        source_commands->content_transform = {1, 1, -group.bounds.x, -group.bounds.y};
        source_commands->count = (uint32_t)count;
        for (size_t index = 0; index < count; ++index)
            source_commands->commands[index] = commands->commands[first + index];
        auto &source_group = source_commands->groups[&group - commands->groups];
        source_group.parent_id = 0;
        source_group.has_shadow = 0;
        source_group.has_clip = 0;
        source_group.clip_radius = 0;
        source_group.clip_radius_motion = {};
        source_group.clip_uniform_radius = 0;
        source_group.x = source_group.y = source_group.opacity = source_group.scale =
            source_group.scale_y = {};
        auto *context = backend->shadow_fallback_context.Get();
        auto format = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
        ComPtr<ID2D1Bitmap1> source, output, readback;
        auto target_properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, format);
        winrt::check_hresult(
            context->CreateBitmap({width, height}, nullptr, 0, target_properties, &source));
        winrt::check_hresult(
            context->CreateBitmap({width, height}, nullptr, 0, target_properties, &output));
        if (!backend->shadow_fallback_parent_device)
            winrt::check_hresult(context->CreateBitmap(
                {width, height}, nullptr, 0,
                D2D1::BitmapProperties1(
                    D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, format),
                &readback));
        context->SetTarget(source.Get());
        context->BeginDraw();
        context->Clear({0, 0, 0, 0});
        reach_result result;
        {
            shadow_target_scope scope(backend, context);
            result = reach_d2d_execute_fallback(backend, source_commands.get(), now_seconds);
        }
        HRESULT hr = context->EndDraw();
        if (result != REACH_OK || FAILED(hr))
        {
            context->SetTarget(nullptr);
            return REACH_ERROR;
        }
        ComPtr<ID2D1Effect> shift, blur, cutout;
        winrt::check_hresult(context->CreateEffect(CLSID_D2D12DAffineTransform, &shift));
        winrt::check_hresult(context->CreateEffect(CLSID_D2D1Shadow, &blur));
        winrt::check_hresult(context->CreateEffect(CLSID_D2D1Composite, &cutout));
        shift->SetInput(0, source.Get());
        winrt::check_hresult(shift->SetValue(
            D2D1_2DAFFINETRANSFORM_PROP_TRANSFORM_MATRIX,
            D2D1::Matrix3x2F::Translation(group.shadow.offset_x, group.shadow.offset_y)));
        blur->SetInputEffect(0, shift.Get());
        winrt::check_hresult(
            blur->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, group.shadow.blur * .5f));
        winrt::check_hresult(blur->SetValue(
            D2D1_SHADOW_PROP_COLOR, D2D1::Vector4F(group.shadow.color.r, group.shadow.color.g,
                                                   group.shadow.color.b, group.shadow.color.a)));
        cutout->SetInputEffect(0, blur.Get());
        cutout->SetInput(1, source.Get());
        winrt::check_hresult(
            cutout->SetValue(D2D1_COMPOSITE_PROP_MODE, D2D1_COMPOSITE_MODE_DESTINATION_OUT));
        context->SetTarget(output.Get());
        context->BeginDraw();
        context->SetTransform(D2D1::Matrix3x2F::Identity());
        context->Clear({0, 0, 0, 0});
        context->DrawImage(cutout.Get());
        hr = context->EndDraw();
        context->SetTarget(nullptr);
        winrt::check_hresult(hr);
        if (backend->shadow_fallback_parent_device)
        {
            reach_workload_add(REACH_WORKLOAD_RASTER_PASSES, 1);
            reach_d2d_target(backend)->DrawBitmap(output.Get(),
                                                  D2D1::RectF(group.bounds.x, group.bounds.y,
                                                              group.bounds.x + width,
                                                              group.bounds.y + height),
                                                  opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            return REACH_OK;
        }
        winrt::check_hresult(readback->CopyFromBitmap(nullptr, output.Get(), nullptr));
        D2D1_MAPPED_RECT mapped = {};
        winrt::check_hresult(readback->Map(D2D1_MAP_OPTIONS_READ, &mapped));
        ComPtr<ID2D1Bitmap> bitmap;
        hr = reach_d2d_target(backend)->CreateBitmap({width, height}, mapped.bits, mapped.pitch,
                                                     D2D1::BitmapProperties(format), &bitmap);
        (void)readback->Unmap();
        winrt::check_hresult(hr);
        reach_workload_add(REACH_WORKLOAD_RASTER_PASSES, 1);
        reach_d2d_target(backend)->DrawBitmap(bitmap.Get(),
                                              D2D1::RectF(group.bounds.x, group.bounds.y,
                                                          group.bounds.x + width,
                                                          group.bounds.y + height),
                                              opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        return REACH_OK;
    }
    catch (...)
    {
        backend->shadow_fallback_context.Reset();
        backend->shadow_fallback_parent_device = nullptr;
        return REACH_ERROR;
    }
}
