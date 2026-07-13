#include "render_d2d_internal.h"
#include "../windows_icon_handle_internal.h"

#include <dcommon.h>
#include <d2d1effects.h>
#include <math.h>

static D2D1_RECT_F reach_d2d_snap_bitmap_rect(reach_rect_f32 rect)
{
    float left = floorf(rect.x + 0.5f);
    float top = floorf(rect.y + 0.5f);

    float width = floorf(rect.width + 0.5f);
    float height = floorf(rect.height + 0.5f);

    if (width < 1.0f)
    {
        width = 1.0f;
    }
    if (height < 1.0f)
    {
        height = 1.0f;
    }

    return D2D1::RectF(left, top, left + width, top + height);
}

static int32_t reach_d2d_center_crop_source_rect(ID2D1Bitmap *bitmap, D2D1_RECT_F dest,
                                                 D2D1_RECT_F *out_source)
{
    if (bitmap == nullptr || out_source == nullptr)
    {
        return 0;
    }

    D2D1_SIZE_F size = bitmap->GetSize();
    float dest_width = dest.right - dest.left;
    float dest_height = dest.bottom - dest.top;
    if (size.width <= 0.0f || size.height <= 0.0f || dest_width <= 0.0f || dest_height <= 0.0f)
    {
        return 0;
    }

    float source_width = size.width;
    float source_height = size.height;
    float source_ratio = source_width / source_height;
    float dest_ratio = dest_width / dest_height;
    if (source_ratio > dest_ratio)
    {
        source_width = source_height * dest_ratio;
    }
    else
    {
        source_height = source_width / dest_ratio;
    }

    out_source->left = (size.width - source_width) * 0.5f;
    out_source->top = (size.height - source_height) * 0.5f;
    out_source->right = out_source->left + source_width;
    out_source->bottom = out_source->top + source_height;
    return 1;
}

static int32_t reach_d2d_corner_mask(const reach_render_command *command)
{
    if (command == nullptr || command->corner_mask == 0)
    {
        return REACH_RENDER_CORNER_ALL;
    }
    return command->corner_mask;
}

static float reach_d2d_clamp_radius(D2D1_RECT_F rect, float radius)
{
    float width = rect.right - rect.left;
    float height = rect.bottom - rect.top;
    float max_radius = fminf(width, height) * 0.5f;
    if (radius < 0.0f)
    {
        return 0.0f;
    }
    return radius > max_radius ? max_radius : radius;
}

static reach_result reach_d2d_create_corner_geometry(ID2D1RenderTarget *target, D2D1_RECT_F rect,
                                                     float radius, int32_t corner_mask,
                                                     ID2D1Geometry **out_geometry)
{
    if (target == nullptr || out_geometry == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_geometry = nullptr;
    radius = reach_d2d_clamp_radius(rect, radius);
    if (radius <= 0.0f)
    {
        return REACH_OK;
    }

    ID2D1Factory *factory = nullptr;
    target->GetFactory(&factory);
    if (factory == nullptr)
    {
        return REACH_ERROR;
    }

    if (corner_mask == REACH_RENDER_CORNER_ALL)
    {
        ID2D1RoundedRectangleGeometry *rounded = nullptr;
        HRESULT hr = factory->CreateRoundedRectangleGeometry(
            D2D1::RoundedRect(rect, radius, radius), &rounded);
        factory->Release();
        if (SUCCEEDED(hr))
        {
            *out_geometry = rounded;
        }
        return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
    }

    ID2D1PathGeometry *path = nullptr;
    ID2D1GeometrySink *sink = nullptr;
    HRESULT hr = factory->CreatePathGeometry(&path);
    factory->Release();
    if (SUCCEEDED(hr))
    {
        hr = path->Open(&sink);
    }
    if (SUCCEEDED(hr))
    {
        float rtl = (corner_mask & REACH_RENDER_CORNER_TOP_LEFT) ? radius : 0.0f;
        float rtr = (corner_mask & REACH_RENDER_CORNER_TOP_RIGHT) ? radius : 0.0f;
        float rbr = (corner_mask & REACH_RENDER_CORNER_BOTTOM_RIGHT) ? radius : 0.0f;
        float rbl = (corner_mask & REACH_RENDER_CORNER_BOTTOM_LEFT) ? radius : 0.0f;

        sink->BeginFigure(D2D1::Point2F(rect.left + rtl, rect.top), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(rect.right - rtr, rect.top));
        if (rtr > 0.0f)
        {
            sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(rect.right, rect.top + rtr),
                                          D2D1::SizeF(rtr, rtr), 0.0f,
                                          D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
        }
        sink->AddLine(D2D1::Point2F(rect.right, rect.bottom - rbr));
        if (rbr > 0.0f)
        {
            sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(rect.right - rbr, rect.bottom),
                                          D2D1::SizeF(rbr, rbr), 0.0f,
                                          D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
        }
        sink->AddLine(D2D1::Point2F(rect.left + rbl, rect.bottom));
        if (rbl > 0.0f)
        {
            sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(rect.left, rect.bottom - rbl),
                                          D2D1::SizeF(rbl, rbl), 0.0f,
                                          D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
        }
        sink->AddLine(D2D1::Point2F(rect.left, rect.top + rtl));
        if (rtl > 0.0f)
        {
            sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(rect.left + rtl, rect.top),
                                          D2D1::SizeF(rtl, rtl), 0.0f,
                                          D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
        }
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        hr = sink->Close();
    }

    if (sink != nullptr)
    {
        sink->Release();
    }
    if (SUCCEEDED(hr))
    {
        *out_geometry = path;
    }
    else if (path != nullptr)
    {
        path->Release();
    }
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

void reach_d2d_clear_icon_cache(reach_render_backend *backend)
{
    if (backend == nullptr)
    {
        return;
    }

    for (reach_d2d_icon_cache_entry &entry : backend->icon_cache)
    {
        if (entry.bitmap != nullptr)
        {
            entry.bitmap->Release();
        }
    }

    backend->icon_cache.clear();
}

void reach_d2d_release_icon_cache_entry(reach_render_backend *backend, uint64_t icon_id)
{
    if (backend == nullptr || icon_id == 0)
    {
        return;
    }

    for (size_t index = 0; index < backend->icon_cache.size(); ++index)
    {
        if (backend->icon_cache[index].icon_id == icon_id)
        {
            if (backend->icon_cache[index].bitmap != nullptr)
            {
                backend->icon_cache[index].bitmap->Release();
            }

            backend->icon_cache.erase(backend->icon_cache.begin() + index);
            return;
        }
    }
}

static reach_result reach_d2d_create_bitmap_from_hbitmap(reach_render_backend *backend,
                                                         HBITMAP hbitmap, ID2D1Bitmap **out_bitmap)
{
    if (backend == nullptr || backend->wic_factory == nullptr || hbitmap == nullptr ||
        out_bitmap == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_bitmap = nullptr;

    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (target == nullptr)
    {
        return REACH_ERROR;
    }

    IWICBitmap *wic_bitmap = nullptr;
    HRESULT hr = backend->wic_factory->CreateBitmapFromHBITMAP(hbitmap, nullptr, WICBitmapUseAlpha,
                                                               &wic_bitmap);

    IWICFormatConverter *converter = nullptr;
    if (SUCCEEDED(hr))
    {
        hr = backend->wic_factory->CreateFormatConverter(&converter);
    }

    if (SUCCEEDED(hr))
    {
        hr = converter->Initialize(wic_bitmap, GUID_WICPixelFormat32bppPBGRA,
                                   WICBitmapDitherTypeNone, nullptr, 0.0,
                                   WICBitmapPaletteTypeMedianCut);
    }

    ID2D1Bitmap *bitmap = nullptr;
    if (SUCCEEDED(hr))
    {
        hr = target->CreateBitmapFromWicBitmap(converter, nullptr, &bitmap);
    }

    if (SUCCEEDED(hr) && bitmap != nullptr)
    {
        *out_bitmap = bitmap;
    }

    if (converter != nullptr)
    {
        converter->Release();
    }
    if (wic_bitmap != nullptr)
    {
        wic_bitmap->Release();
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_d2d_create_icon_bitmap(reach_render_backend *backend, uintptr_t icon_id,
                                                 ID2D1Bitmap **out_bitmap)
{
    REACH_ASSERT(backend != nullptr);
    REACH_ASSERT(out_bitmap != nullptr);

    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (backend == nullptr || target == nullptr || backend->wic_factory == nullptr ||
        icon_id == 0 || out_bitmap == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_windows_icon *icon = reinterpret_cast<reach_windows_icon *>(icon_id);
    if (icon == nullptr || icon->magic != REACH_WINDOWS_ICON_MAGIC)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (icon->kind == REACH_WINDOWS_ICON_KIND_HBITMAP)
    {
        return reach_d2d_create_bitmap_from_hbitmap(backend, icon->hbitmap, out_bitmap);
    }

    if (icon->kind != REACH_WINDOWS_ICON_KIND_HICON || icon->hicon == nullptr)
    {
        return REACH_ERROR;
    }

    *out_bitmap = nullptr;

    IWICBitmap *wic_bitmap = nullptr;
    HRESULT hr = backend->wic_factory->CreateBitmapFromHICON(icon->hicon, &wic_bitmap);

    if (FAILED(hr))
    {
        return REACH_ERROR;
    }

    IWICFormatConverter *converter = nullptr;
    hr = backend->wic_factory->CreateFormatConverter(&converter);

    if (SUCCEEDED(hr))
    {
        hr = converter->Initialize(wic_bitmap, GUID_WICPixelFormat32bppPBGRA,
                                   WICBitmapDitherTypeNone, nullptr, 0.0,
                                   WICBitmapPaletteTypeMedianCut);
    }

    ID2D1Bitmap *bitmap = nullptr;
    if (SUCCEEDED(hr))
    {
        hr = target->CreateBitmapFromWicBitmap(converter, nullptr, &bitmap);
    }

    if (SUCCEEDED(hr) && bitmap != nullptr)
    {
        *out_bitmap = bitmap;
    }

    if (converter != nullptr)
    {
        converter->Release();
    }

    if (wic_bitmap != nullptr)
    {
        wic_bitmap->Release();
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_d2d_get_icon_bitmap(reach_render_backend *backend, uintptr_t icon_id,
                                              ID2D1Bitmap **out_bitmap)
{
    REACH_ASSERT(backend != nullptr);
    REACH_ASSERT(out_bitmap != nullptr);

    if (backend == nullptr || out_bitmap == nullptr || icon_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    for (reach_d2d_icon_cache_entry &entry : backend->icon_cache)
    {
        if (entry.icon_id == icon_id && entry.bitmap != nullptr)
        {
            *out_bitmap = entry.bitmap;
            return REACH_OK;
        }
    }

    ID2D1Bitmap *bitmap = nullptr;
    reach_result result = reach_d2d_create_icon_bitmap(backend, icon_id, &bitmap);

    if (result != REACH_OK || bitmap == nullptr)
    {
        return result == REACH_OK ? REACH_ERROR : result;
    }

    try
    {
        backend->icon_cache.push_back({icon_id, bitmap});
    }
    catch (...)
    {
        bitmap->Release();
        return REACH_ERROR;
    }

    *out_bitmap = bitmap;
    return REACH_OK;
}

reach_result reach_d2d_draw_icon_tint(reach_render_backend *backend,
                                      const reach_render_command *command)
{
    REACH_ASSERT(backend != nullptr);
    REACH_ASSERT(command != nullptr);

    if (backend == nullptr || command == nullptr || command->icon_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    ID2D1Bitmap *bitmap = nullptr;
    reach_result result = reach_d2d_get_icon_bitmap(backend, command->icon_id, &bitmap);
    if (result != REACH_OK || bitmap == nullptr)
    {
        return result == REACH_OK ? REACH_ERROR : result;
    }

    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (target == nullptr)
    {
        return REACH_ERROR;
    }

    D2D1_RECT_F rect = reach_d2d_snap_bitmap_rect(command->rect);
    D2D1_SIZE_F bitmap_size = bitmap->GetSize();
    float rect_width = rect.right - rect.left;
    float rect_height = rect.bottom - rect.top;

    if (bitmap_size.width <= 0.0f || bitmap_size.height <= 0.0f || rect_width <= 0.0f ||
        rect_height <= 0.0f)
    {
        return REACH_ERROR;
    }

    ID2D1Layer *layer = nullptr;
    ID2D1BitmapBrush *opacity_brush = nullptr;
    ID2D1SolidColorBrush *fill_brush = nullptr;

    HRESULT hr = target->CreateLayer(nullptr, &layer);

    if (SUCCEEDED(hr))
    {
        D2D1_MATRIX_3X2_F brush_transform =
            D2D1::Matrix3x2F::Scale(rect_width / bitmap_size.width,
                                    rect_height / bitmap_size.height) *
            D2D1::Matrix3x2F::Translation(rect.left, rect.top);

        D2D1_BITMAP_BRUSH_PROPERTIES bitmap_props = D2D1::BitmapBrushProperties(
            D2D1_EXTEND_MODE_CLAMP, D2D1_EXTEND_MODE_CLAMP, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

        D2D1_BRUSH_PROPERTIES brush_props = D2D1::BrushProperties(1.0f, brush_transform);

        hr = target->CreateBitmapBrush(bitmap, bitmap_props, brush_props, &opacity_brush);
    }

    if (SUCCEEDED(hr))
    {
        hr = target->CreateSolidColorBrush(
            D2D1::ColorF(command->color.r, command->color.g, command->color.b, command->color.a),
            &fill_brush);
    }

    if (SUCCEEDED(hr))
    {
        D2D1_LAYER_PARAMETERS layer_params = D2D1::LayerParameters(
            rect, nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(), 1.0f,
            opacity_brush, D2D1_LAYER_OPTIONS_NONE);

        target->PushLayer(layer_params, layer);
        target->FillRectangle(rect, fill_brush);
        target->PopLayer();
    }

    if (fill_brush != nullptr)
    {
        fill_brush->Release();
    }
    if (opacity_brush != nullptr)
    {
        opacity_brush->Release();
    }
    if (layer != nullptr)
    {
        layer->Release();
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static HRESULT reach_d2d_push_fade_layer(ID2D1RenderTarget *target, D2D1_RECT_F rect,
                                         float fade_start, ID2D1Layer **out_layer,
                                         ID2D1LinearGradientBrush **out_brush)
{
    D2D1_GRADIENT_STOP stops[2] = {
        {fade_start, D2D1::ColorF(D2D1::ColorF::White, 1.0f)},
        {1.0f, D2D1::ColorF(D2D1::ColorF::White, 0.0f)},
    };
    ID2D1GradientStopCollection *collection = nullptr;
    HRESULT hr = target->CreateGradientStopCollection(stops, 2, &collection);
    if (SUCCEEDED(hr))
    {
        hr = target->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(D2D1::Point2F(rect.left, rect.top),
                                                D2D1::Point2F(rect.right, rect.top)),
            collection, out_brush);
    }
    if (collection != nullptr)
    {
        collection->Release();
    }
    if (SUCCEEDED(hr))
    {
        hr = target->CreateLayer(nullptr, out_layer);
    }
    if (SUCCEEDED(hr))
    {
        D2D1_LAYER_PARAMETERS params =
            D2D1::LayerParameters(rect, nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                  D2D1::IdentityMatrix(), 1.0f, *out_brush,
                                  D2D1_LAYER_OPTIONS_NONE);
        target->PushLayer(params, *out_layer);
    }
    return hr;
}

static void reach_d2d_draw_bitmap_with_fade(reach_render_backend *backend,
                                            ID2D1RenderTarget *target, ID2D1Bitmap *bitmap,
                                            D2D1_RECT_F rect, float opacity,
                                            const D2D1_RECT_F *source, float fade_start)
{
    ID2D1Layer *fade_layer = nullptr;
    ID2D1LinearGradientBrush *fade_brush = nullptr;
    int32_t fade = fade_start > 0.0f && fade_start < 1.0f;
    if (fade &&
        FAILED(reach_d2d_push_fade_layer(target, rect, fade_start, &fade_layer, &fade_brush)))
    {
        fade = 0;
    }
    if (backend->d2d_context != nullptr)
    {
        backend->d2d_context->DrawBitmap(bitmap, &rect, opacity,
                                         D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC, source,
                                         nullptr);
    }
    else
    {
        target->DrawBitmap(bitmap, rect, opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, source);
    }
    if (fade)
    {
        target->PopLayer();
    }
    if (fade_brush != nullptr)
    {
        fade_brush->Release();
    }
    if (fade_layer != nullptr)
    {
        fade_layer->Release();
    }
}

reach_result reach_d2d_draw_icon(reach_render_backend *backend, const reach_render_command *command)
{
    REACH_ASSERT(backend != nullptr);
    REACH_ASSERT(command != nullptr);

    if (backend == nullptr || command == nullptr || command->icon_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    ID2D1Bitmap *bitmap = nullptr;
    reach_result result = reach_d2d_get_icon_bitmap(backend, command->icon_id, &bitmap);
    if (result != REACH_OK || bitmap == nullptr)
    {
        return result == REACH_OK ? REACH_ERROR : result;
    }

    D2D1_RECT_F rect = reach_d2d_snap_bitmap_rect(command->rect);

    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (target == nullptr)
    {
        return REACH_ERROR;
    }

    if (command->radius > 0.0f)
    {
        ID2D1Geometry *clip_geometry = nullptr;
        ID2D1Layer *clip_layer = nullptr;

        reach_result geometry_result = reach_d2d_create_corner_geometry(
            target, rect, command->radius, reach_d2d_corner_mask(command), &clip_geometry);
        HRESULT hr = geometry_result == REACH_OK ? S_OK : E_FAIL;

        if (SUCCEEDED(hr))
        {
            hr = target->CreateLayer(nullptr, &clip_layer);
        }

        if (SUCCEEDED(hr))
        {
            D2D1_RECT_F source_rect = {};
            D2D1_RECT_F *source = nullptr;
            if (command->icon_crop_to_fill &&
                reach_d2d_center_crop_source_rect(bitmap, rect, &source_rect))
            {
                source = &source_rect;
            }
            D2D1_LAYER_PARAMETERS layer = D2D1::LayerParameters(
                rect, clip_geometry, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(),
                1.0f, nullptr, D2D1_LAYER_OPTIONS_NONE);

            target->PushLayer(layer, clip_layer);
            reach_d2d_draw_bitmap_with_fade(backend, target, bitmap, rect, command->color.a,
                                            source, command->icon_fade_start);
            target->PopLayer();
        }

        if (clip_layer != nullptr)
        {
            clip_layer->Release();
        }
        if (clip_geometry != nullptr)
        {
            clip_geometry->Release();
        }

        return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
    }

    D2D1_RECT_F source_rect = {};
    D2D1_RECT_F *source = nullptr;
    if (command->icon_crop_to_fill && reach_d2d_center_crop_source_rect(bitmap, rect, &source_rect))
    {
        source = &source_rect;
    }

    reach_d2d_draw_bitmap_with_fade(backend, target, bitmap, rect, command->color.a, source,
                                    command->icon_fade_start);

    return REACH_OK;
}

reach_result reach_d2d_draw_blurred_image(reach_render_backend *backend,
                                          const reach_render_command *command)
{
    REACH_ASSERT(backend != nullptr);
    REACH_ASSERT(command != nullptr);

    if (backend == nullptr || command == nullptr || command->icon_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    ID2D1Bitmap *bitmap = nullptr;
    reach_result result = reach_d2d_get_icon_bitmap(backend, command->icon_id, &bitmap);
    if (result != REACH_OK || bitmap == nullptr)
    {
        return result == REACH_OK ? REACH_ERROR : result;
    }

    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (target == nullptr)
    {
        return REACH_ERROR;
    }

    D2D1_RECT_F rect = reach_d2d_snap_bitmap_rect(command->rect);
    D2D1_RECT_F source_rect = {};
    D2D1_RECT_F *source = nullptr;
    if (command->icon_crop_to_fill && reach_d2d_center_crop_source_rect(bitmap, rect, &source_rect))
    {
        source = &source_rect;
    }

    if (backend->d2d_context == nullptr || command->blur_radius <= 0.0f)
    {
        if (command->has_clip_rect && command->clip_radius > 0.0f &&
            command->clip_rect.width > 0.0f && command->clip_rect.height > 0.0f)
        {
            ID2D1Geometry *clip_geometry = nullptr;
            ID2D1Layer *clip_layer = nullptr;
            D2D1_RECT_F clip_snap = reach_d2d_snap_bitmap_rect(command->clip_rect);
            HRESULT hr = reach_d2d_create_corner_geometry(target, clip_snap, command->clip_radius,
                                                          REACH_RENDER_CORNER_ALL,
                                                          &clip_geometry) == REACH_OK
                             ? S_OK
                             : E_FAIL;
            if (SUCCEEDED(hr))
            {
                hr = target->CreateLayer(nullptr, &clip_layer);
            }
            if (SUCCEEDED(hr))
            {
                D2D1_LAYER_PARAMETERS layer = D2D1::LayerParameters(
                    clip_snap, clip_geometry, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                    D2D1::IdentityMatrix(), 1.0f, nullptr, D2D1_LAYER_OPTIONS_NONE);
                target->PushLayer(layer, clip_layer);
                target->DrawBitmap(bitmap, rect, command->color.a,
                                   D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, source);
                target->PopLayer();
            }
            if (clip_layer != nullptr)
            {
                clip_layer->Release();
            }
            if (clip_geometry != nullptr)
            {
                clip_geometry->Release();
            }
            return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
        }
        else if (command->has_clip_rect)
        {
            ID2D1RenderTarget *clip_target = target;
            clip_target->PushAxisAlignedClip(reach_d2d_snap_bitmap_rect(command->clip_rect),
                                             D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            target->DrawBitmap(bitmap, rect, command->color.a,
                               D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, source);
            clip_target->PopAxisAlignedClip();
            return REACH_OK;
        }
        else if (command->radius > 0.0f)
        {
            ID2D1Geometry *clip_geometry = nullptr;
            ID2D1Layer *clip_layer = nullptr;
            HRESULT hr = reach_d2d_create_corner_geometry(target, rect, command->radius,
                                                          reach_d2d_corner_mask(command),
                                                          &clip_geometry) == REACH_OK
                             ? S_OK
                             : E_FAIL;
            if (SUCCEEDED(hr))
            {
                hr = target->CreateLayer(nullptr, &clip_layer);
            }
            if (SUCCEEDED(hr))
            {
                D2D1_LAYER_PARAMETERS layer = D2D1::LayerParameters(
                    rect, clip_geometry, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(),
                    1.0f, nullptr, D2D1_LAYER_OPTIONS_NONE);
                target->PushLayer(layer, clip_layer);
                target->DrawBitmap(bitmap, rect, command->color.a,
                                   D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, source);
                target->PopLayer();
            }
            if (clip_layer != nullptr)
            {
                clip_layer->Release();
            }
            if (clip_geometry != nullptr)
            {
                clip_geometry->Release();
            }
            return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
        }
        target->DrawBitmap(bitmap, rect, command->color.a, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                           source);
        return REACH_OK;
    }

    ID2D1DeviceContext *context = backend->d2d_context;

    float actual_contrast = command->image_contrast > 0.0f ? command->image_contrast : 1.0f;

    D2D1_RECT_F mapped_source = source != nullptr ? *source
                                                  : D2D1::RectF(0.0f, 0.0f, bitmap->GetSize().width,
                                                                bitmap->GetSize().height);
    float source_width = mapped_source.right - mapped_source.left;
    float source_height = mapped_source.bottom - mapped_source.top;

    D2D1_RECT_F image_rect = reach_d2d_snap_bitmap_rect(command->rect);
    float image_width = image_rect.right - image_rect.left;
    float image_height = image_rect.bottom - image_rect.top;

    if (source_width <= 0.0f || source_height <= 0.0f || image_width <= 0.0f ||
        image_height <= 0.0f)
    {
        return REACH_ERROR;
    }

    HRESULT hr = S_OK;
    ID2D1Effect *color_mat = nullptr;
    ID2D1Effect *transform = nullptr;
    ID2D1Effect *blur = nullptr;

    if (actual_contrast != 1.0f)
    {
        hr = context->CreateEffect(CLSID_D2D1ColorMatrix, &color_mat);
        if (SUCCEEDED(hr))
        {
            color_mat->SetInput(0, bitmap);
            float scale = actual_contrast;
            float offset = 0.5f - 0.5f * scale;
            D2D1_MATRIX_5X4_F matrix =
                D2D1::Matrix5x4F(scale, 0.0f, 0.0f, 0.0f, 0.0f, scale, 0.0f, 0.0f, 0.0f, 0.0f,
                                 scale, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, offset, offset, offset, 0.0f);
            color_mat->SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, matrix);
        }
    }

    if (SUCCEEDED(hr))
    {
        hr = context->CreateEffect(CLSID_D2D12DAffineTransform, &transform);
    }
    if (SUCCEEDED(hr))
    {
        D2D1_MATRIX_3X2_F matrix =
            D2D1::Matrix3x2F::Translation(-mapped_source.left, -mapped_source.top) *
            D2D1::Matrix3x2F::Scale(image_width / source_width, image_height / source_height) *
            D2D1::Matrix3x2F::Translation(image_rect.left, image_rect.top);
        if (color_mat != nullptr)
        {
            transform->SetInputEffect(0, color_mat);
        }
        else
        {
            transform->SetInput(0, bitmap);
        }
        hr = transform->SetValue(D2D1_2DAFFINETRANSFORM_PROP_TRANSFORM_MATRIX, matrix);
    }

    if (SUCCEEDED(hr))
    {
        hr = context->CreateEffect(CLSID_D2D1GaussianBlur, &blur);
    }
    if (SUCCEEDED(hr))
    {
        ID2D1Effect *blur_input = nullptr;
        if (transform != nullptr)
        {
            blur_input = transform;
        }
        else if (color_mat != nullptr)
        {
            blur_input = color_mat;
        }
        else
        {
            blur->SetInput(0, bitmap);
        }

        if (blur_input != nullptr)
        {
            blur->SetInputEffect(0, blur_input);
        }
        hr = blur->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, command->blur_radius);
    }
    if (SUCCEEDED(hr))
    {
        hr = blur->SetValue(D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION,
                            D2D1_GAUSSIANBLUR_OPTIMIZATION_SPEED);
    }

    if (SUCCEEDED(hr))
    {
        ID2D1Geometry *clip_geometry = nullptr;
        ID2D1Layer *clip_layer = nullptr;
        int32_t pushed_layer = 0;
        int32_t pushed_axis_clip = 0;

        if (command->has_clip_rect && command->clip_radius > 0.0f &&
            command->clip_rect.width > 0.0f && command->clip_rect.height > 0.0f)
        {
            D2D1_RECT_F clip_snap = reach_d2d_snap_bitmap_rect(command->clip_rect);
            HRESULT clip_hr = reach_d2d_create_corner_geometry(
                                  target, clip_snap, command->clip_radius, REACH_RENDER_CORNER_ALL,
                                  &clip_geometry) == REACH_OK
                                  ? S_OK
                                  : E_FAIL;
            if (SUCCEEDED(clip_hr))
            {
                clip_hr = target->CreateLayer(nullptr, &clip_layer);
            }
            if (SUCCEEDED(clip_hr))
            {
                D2D1_LAYER_PARAMETERS layer = D2D1::LayerParameters(
                    clip_snap, clip_geometry, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                    D2D1::IdentityMatrix(), 1.0f, nullptr, D2D1_LAYER_OPTIONS_NONE);
                target->PushLayer(layer, clip_layer);
                pushed_layer = 1;
            }
            else
            {
                if (clip_geometry != nullptr)
                {
                    clip_geometry->Release();
                }
                hr = E_FAIL;
            }
        }
        else if (command->has_clip_rect)
        {
            context->PushAxisAlignedClip(reach_d2d_snap_bitmap_rect(command->clip_rect),
                                         D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            pushed_axis_clip = 1;
        }
        else if (command->radius > 0.0f)
        {
            HRESULT clip_hr = reach_d2d_create_corner_geometry(target, rect, command->radius,
                                                               reach_d2d_corner_mask(command),
                                                               &clip_geometry) == REACH_OK
                                  ? S_OK
                                  : E_FAIL;
            if (SUCCEEDED(clip_hr))
            {
                clip_hr = target->CreateLayer(nullptr, &clip_layer);
            }
            if (SUCCEEDED(clip_hr))
            {
                D2D1_LAYER_PARAMETERS layer = D2D1::LayerParameters(
                    rect, clip_geometry, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(),
                    1.0f, nullptr, D2D1_LAYER_OPTIONS_NONE);
                target->PushLayer(layer, clip_layer);
                pushed_layer = 1;
            }
            else
            {
                if (clip_geometry != nullptr)
                {
                    clip_geometry->Release();
                }
                hr = E_FAIL;
            }
        }

        if (SUCCEEDED(hr))
        {
            ID2D1Image *blur_output = nullptr;
            blur->GetOutput(&blur_output);
            if (blur_output != nullptr)
            {
                context->DrawImage(blur_output, nullptr, nullptr, D2D1_INTERPOLATION_MODE_LINEAR,
                                   D2D1_COMPOSITE_MODE_SOURCE_OVER);
                blur_output->Release();
            }
        }

        if (pushed_layer)
        {
            target->PopLayer();
        }
        if (pushed_axis_clip)
        {
            context->PopAxisAlignedClip();
        }
        if (clip_layer != nullptr)
        {
            clip_layer->Release();
        }
        if (clip_geometry != nullptr)
        {
            clip_geometry->Release();
        }
    }
    else
    {
        target->DrawBitmap(bitmap, rect, command->color.a, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                           source);
    }

    if (blur != nullptr)
    {
        blur->Release();
    }
    if (transform != nullptr)
    {
        transform->Release();
    }
    if (color_mat != nullptr)
    {
        color_mat->Release();
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}
