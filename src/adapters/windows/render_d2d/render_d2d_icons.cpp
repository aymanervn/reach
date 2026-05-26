#include "render_d2d_internal.h"

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

    *out_bitmap = nullptr;

    IWICBitmap *wic_bitmap = nullptr;
    HRESULT hr = backend->wic_factory->CreateBitmapFromHICON(
        reinterpret_cast<HICON>(icon_id),
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

    wic_bitmap->Release();

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

    D2D1_RECT_F rect = D2D1::RectF(
        command->rect.x,
        command->rect.y,
        command->rect.x + command->rect.width,
        command->rect.y + command->rect.height);

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
