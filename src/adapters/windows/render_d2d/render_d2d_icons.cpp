#include "render_d2d_internal.h"
#include "../windows_icon_handle_internal.h"

#include <math.h>

static D2D1_RECT_F reach_d2d_snap_bitmap_rect(reach_rect_f32 rect)
{
    float left = floorf(rect.x + 0.5f);
    float top = floorf(rect.y + 0.5f);

    float width = floorf(rect.width + 0.5f);
    float height = floorf(rect.height + 0.5f);

    if (width < 1.0f) {
        width = 1.0f;
    }
    if (height < 1.0f) {
        height = 1.0f;
    }

    return D2D1::RectF(left, top, left + width, top + height);
}

void reach_d2d_clear_icon_cache(reach_render_backend *backend)
{
    if (backend == nullptr) {
        return;
    }

    for (reach_d2d_icon_cache_entry &entry : backend->icon_cache) {
        if (entry.bitmap != nullptr) {
            entry.bitmap->Release();
        }
    }

    backend->icon_cache.clear();
}

void reach_d2d_release_icon_cache_entry(reach_render_backend *backend, uint64_t icon_id)
{
    if (backend == nullptr || icon_id == 0) {
        return;
    }

    for (size_t index = 0; index < backend->icon_cache.size(); ++index) {
        if (backend->icon_cache[index].icon_id == icon_id) {
            if (backend->icon_cache[index].bitmap != nullptr) {
                backend->icon_cache[index].bitmap->Release();
            }

            backend->icon_cache.erase(backend->icon_cache.begin() + index);
            return;
        }
    }
}

static reach_result reach_d2d_create_bitmap_from_hbitmap(
    reach_render_backend *backend,
    HBITMAP hbitmap,
    ID2D1Bitmap **out_bitmap)
{
    if (backend == nullptr || backend->wic_factory == nullptr || hbitmap == nullptr || out_bitmap == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_bitmap = nullptr;

    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (target == nullptr) {
        return REACH_ERROR;
    }

    IWICBitmap *wic_bitmap = nullptr;
    HRESULT hr = backend->wic_factory->CreateBitmapFromHBITMAP(
        hbitmap,
        nullptr,
        WICBitmapUseAlpha,
        &wic_bitmap);

    IWICFormatConverter *converter = nullptr;
    if (SUCCEEDED(hr)) {
        hr = backend->wic_factory->CreateFormatConverter(&converter);
    }

    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(
            wic_bitmap,
            GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeMedianCut);
    }

    ID2D1Bitmap *bitmap = nullptr;
    if (SUCCEEDED(hr)) {
        hr = target->CreateBitmapFromWicBitmap(converter, nullptr, &bitmap);
    }

    if (SUCCEEDED(hr) && bitmap != nullptr) {
        *out_bitmap = bitmap;
    }

    if (converter != nullptr) {
        converter->Release();
    }
    if (wic_bitmap != nullptr) {
        wic_bitmap->Release();
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_d2d_create_icon_bitmap(
    reach_render_backend *backend,
    uintptr_t icon_id,
    ID2D1Bitmap **out_bitmap
)
{
    REACH_ASSERT(backend != nullptr);
    REACH_ASSERT(out_bitmap != nullptr);

    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (backend == nullptr ||
        target == nullptr ||
        backend->wic_factory == nullptr ||
        icon_id == 0 ||
        out_bitmap == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_windows_icon *icon = reinterpret_cast<reach_windows_icon *>(icon_id);
    if (icon == nullptr || icon->magic != REACH_WINDOWS_ICON_MAGIC) {
        return REACH_INVALID_ARGUMENT;
    }

    if (icon->kind == REACH_WINDOWS_ICON_KIND_HBITMAP) {
        return reach_d2d_create_bitmap_from_hbitmap(backend, icon->hbitmap, out_bitmap);
    }

    if (icon->kind != REACH_WINDOWS_ICON_KIND_HICON || icon->hicon == nullptr) {
        return REACH_ERROR;
    }

    *out_bitmap = nullptr;

    IWICBitmap *wic_bitmap = nullptr;
    HRESULT hr = backend->wic_factory->CreateBitmapFromHICON(
        icon->hicon,
        &wic_bitmap);

    if (FAILED(hr)) {
        return REACH_ERROR;
    }

    IWICFormatConverter *converter = nullptr;
    hr = backend->wic_factory->CreateFormatConverter(&converter);

    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(
            wic_bitmap,
            GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeMedianCut);
    }

    ID2D1Bitmap *bitmap = nullptr;
    if (SUCCEEDED(hr)) {
        hr = target->CreateBitmapFromWicBitmap(converter, nullptr, &bitmap);
    }

    if (SUCCEEDED(hr) && bitmap != nullptr) {
        *out_bitmap = bitmap;
    }

    if (converter != nullptr) {
        converter->Release();
    }

    if (wic_bitmap != nullptr) {
        wic_bitmap->Release();
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_d2d_get_icon_bitmap(
    reach_render_backend *backend,
    uintptr_t icon_id,
    ID2D1Bitmap **out_bitmap
)
{
    REACH_ASSERT(backend != nullptr);
    REACH_ASSERT(out_bitmap != nullptr);

    if (backend == nullptr || out_bitmap == nullptr || icon_id == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    for (reach_d2d_icon_cache_entry &entry : backend->icon_cache) {
        if (entry.icon_id == icon_id && entry.bitmap != nullptr) {
            *out_bitmap = entry.bitmap;
            return REACH_OK;
        }
    }

    ID2D1Bitmap *bitmap = nullptr;
    reach_result result = reach_d2d_create_icon_bitmap(backend, icon_id, &bitmap);

    if (result != REACH_OK || bitmap == nullptr) {
        return result == REACH_OK ? REACH_ERROR : result;
    }

    try {
        backend->icon_cache.push_back({ icon_id, bitmap });
    } catch (...) {
        bitmap->Release();
        return REACH_ERROR;
    }

    *out_bitmap = bitmap;
    return REACH_OK;
}

reach_result reach_d2d_draw_icon_tint(
    reach_render_backend *backend,
    const reach_render_command *command)
{
    REACH_ASSERT(backend != nullptr);
    REACH_ASSERT(command != nullptr);

    if (backend == nullptr || command == nullptr || command->icon_id == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    ID2D1Bitmap *bitmap = nullptr;
    reach_result result = reach_d2d_get_icon_bitmap(backend, command->icon_id, &bitmap);
    if (result != REACH_OK || bitmap == nullptr) {
        return result == REACH_OK ? REACH_ERROR : result;
    }

    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (target == nullptr) {
        return REACH_ERROR;
    }

    D2D1_RECT_F rect = reach_d2d_snap_bitmap_rect(command->rect);
    D2D1_SIZE_F bitmap_size = bitmap->GetSize();
    float rect_width = rect.right - rect.left;
    float rect_height = rect.bottom - rect.top;

    if (bitmap_size.width <= 0.0f || bitmap_size.height <= 0.0f ||
        rect_width <= 0.0f || rect_height <= 0.0f) {
        return REACH_ERROR;
    }

    ID2D1Layer *layer = nullptr;
    ID2D1BitmapBrush *opacity_brush = nullptr;
    ID2D1SolidColorBrush *fill_brush = nullptr;

    HRESULT hr = target->CreateLayer(nullptr, &layer);

    if (SUCCEEDED(hr)) {
        D2D1_MATRIX_3X2_F brush_transform =
            D2D1::Matrix3x2F::Scale(
                rect_width / bitmap_size.width,
                rect_height / bitmap_size.height) *
            D2D1::Matrix3x2F::Translation(rect.left, rect.top);

        D2D1_BITMAP_BRUSH_PROPERTIES bitmap_props =
            D2D1::BitmapBrushProperties(
                D2D1_EXTEND_MODE_CLAMP,
                D2D1_EXTEND_MODE_CLAMP,
                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

        D2D1_BRUSH_PROPERTIES brush_props =
            D2D1::BrushProperties(1.0f, brush_transform);

        hr = target->CreateBitmapBrush(
            bitmap,
            bitmap_props,
            brush_props,
            &opacity_brush);
    }

    if (SUCCEEDED(hr)) {
        hr = target->CreateSolidColorBrush(
            D2D1::ColorF(
                command->color.r,
                command->color.g,
                command->color.b,
                command->color.a),
            &fill_brush);
    }

    if (SUCCEEDED(hr)) {
        D2D1_LAYER_PARAMETERS layer_params = D2D1::LayerParameters(
            rect,
            nullptr,
            D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
            D2D1::IdentityMatrix(),
            1.0f,
            opacity_brush,
            D2D1_LAYER_OPTIONS_NONE);

        target->PushLayer(layer_params, layer);
        target->FillRectangle(rect, fill_brush);
        target->PopLayer();
    }

    if (fill_brush != nullptr) {
        fill_brush->Release();
    }
    if (opacity_brush != nullptr) {
        opacity_brush->Release();
    }
    if (layer != nullptr) {
        layer->Release();
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

reach_result reach_d2d_draw_icon(
    reach_render_backend *backend,
    const reach_render_command *command
)
{
    REACH_ASSERT(backend != nullptr);
    REACH_ASSERT(command != nullptr);

    if (backend == nullptr || command == nullptr || command->icon_id == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    ID2D1Bitmap *bitmap = nullptr;
    reach_result result = reach_d2d_get_icon_bitmap(backend, command->icon_id, &bitmap);
    if (result != REACH_OK || bitmap == nullptr) {
        return result == REACH_OK ? REACH_ERROR : result;
    }

    D2D1_RECT_F rect = reach_d2d_snap_bitmap_rect(command->rect);

    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (target == nullptr) {
        return REACH_ERROR;
    }

    if (command->radius > 0.0f) {
        ID2D1Factory *factory = nullptr;
        ID2D1RoundedRectangleGeometry *clip_geometry = nullptr;
        ID2D1Layer *clip_layer = nullptr;

        target->GetFactory(&factory);

        HRESULT hr = factory != nullptr
            ? factory->CreateRoundedRectangleGeometry(
                D2D1::RoundedRect(rect, command->radius, command->radius),
                &clip_geometry)
            : E_FAIL;

        if (SUCCEEDED(hr)) {
            hr = target->CreateLayer(nullptr, &clip_layer);
        }

        if (SUCCEEDED(hr)) {
            D2D1_LAYER_PARAMETERS layer = D2D1::LayerParameters(
                rect,
                clip_geometry,
                D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                D2D1::IdentityMatrix(),
                1.0f,
                nullptr,
                D2D1_LAYER_OPTIONS_NONE);

            target->PushLayer(layer, clip_layer);
            target->DrawBitmap(
                bitmap,
                rect,
                command->color.a,
                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            target->PopLayer();
        }

        if (clip_layer != nullptr) {
            clip_layer->Release();
        }
        if (clip_geometry != nullptr) {
            clip_geometry->Release();
        }
        if (factory != nullptr) {
            factory->Release();
        }

        return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
    }

    target->DrawBitmap(
        bitmap,
        rect,
        command->color.a,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

    return REACH_OK;
}
