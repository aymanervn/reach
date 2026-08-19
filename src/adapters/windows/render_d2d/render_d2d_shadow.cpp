#include "render_d2d_internal.h"

#include <d2d1effects.h>
#include <math.h>

static const float REACH_D2D_SHADOW_STRETCH_BAND = 2.0f;
static const size_t REACH_D2D_SHADOW_MAX_SIZED_ENTRIES = 8;

static reach_shadow reach_d2d_shadow_from_command(const reach_render_command *command)
{
    reach_shadow shadow = {};
    shadow.offset_x = command->shadow_offset_x;
    shadow.offset_y = command->shadow_offset_y;
    shadow.blur = command->blur_radius;
    shadow.color = command->color;
    return shadow;
}

static reach_shadow reach_d2d_shadow_from_key(const reach_d2d_shadow_key *key)
{
    reach_shadow shadow = {};
    shadow.offset_x = key->offset_x;
    shadow.offset_y = key->offset_y;
    shadow.blur = key->blur;
    shadow.color = key->color;
    return shadow;
}

static float reach_d2d_shadow_cap(const reach_d2d_shadow_key *key)
{
    reach_shadow shadow = reach_d2d_shadow_from_key(key);
    return key->radius + reach_theme_shadow_extent(&shadow, 1.0f);
}

static int32_t reach_d2d_shadow_stretches_x(const reach_d2d_shadow_key *key)
{
    return key->content_width <= 0.0f;
}

static int32_t reach_d2d_shadow_stretches_y(const reach_d2d_shadow_key *key)
{
    return key->content_height <= 0.0f;
}

static float reach_d2d_shadow_notch_reserve_top(const reach_d2d_shadow_key *key)
{
    return key->has_notch && key->notch_side == REACH_NOTCH_SIDE_TOP ? key->notch_height : 0.0f;
}

static float reach_d2d_shadow_notch_reserve_bottom(const reach_d2d_shadow_key *key)
{
    return key->has_notch && key->notch_side == REACH_NOTCH_SIDE_BOTTOM ? key->notch_height : 0.0f;
}

static float reach_d2d_shadow_bake_width(const reach_d2d_shadow_key *key)
{
    if (!reach_d2d_shadow_stretches_x(key))
    {
        return key->content_width;
    }
    return 2.0f * reach_d2d_shadow_cap(key) + REACH_D2D_SHADOW_STRETCH_BAND;
}

static float reach_d2d_shadow_bake_height(const reach_d2d_shadow_key *key)
{
    if (!reach_d2d_shadow_stretches_y(key))
    {
        return key->content_height;
    }
    return 2.0f * reach_d2d_shadow_cap(key) + REACH_D2D_SHADOW_STRETCH_BAND +
           reach_d2d_shadow_notch_reserve_top(key) + reach_d2d_shadow_notch_reserve_bottom(key);
}

static reach_d2d_shadow_key reach_d2d_shadow_key_from_command(const reach_render_command *command)
{
    reach_d2d_shadow_key key = {};
    key.radius = command->radius;
    key.blur = command->blur_radius;
    key.offset_x = command->shadow_offset_x;
    key.offset_y = command->shadow_offset_y;
    key.color = command->color;
    key.has_notch = command->notch_width > 0.0f && command->notch_height > 0.0f;

    if (key.has_notch)
    {
        key.notch_width = command->notch_width;
        key.notch_height = command->notch_height;
        key.notch_side = command->notch_side;
        key.notch_offset_x = command->notch_center_x - command->rect.x;
    }

    float cap = reach_d2d_shadow_cap(&key);
    float min_width = 2.0f * cap + REACH_D2D_SHADOW_STRETCH_BAND;
    float min_height = min_width + reach_d2d_shadow_notch_reserve_top(&key) +
                       reach_d2d_shadow_notch_reserve_bottom(&key);

    if (key.has_notch || command->rect.width < min_width)
    {
        key.content_width = command->rect.width;
    }
    if (command->rect.height < min_height)
    {
        key.content_height = command->rect.height;
    }

    return key;
}

static int32_t reach_d2d_shadow_key_equal(const reach_d2d_shadow_key *a,
                                          const reach_d2d_shadow_key *b)
{
    return a->content_width == b->content_width && a->content_height == b->content_height &&
           a->radius == b->radius && a->blur == b->blur && a->offset_x == b->offset_x &&
           a->offset_y == b->offset_y && a->notch_offset_x == b->notch_offset_x &&
           a->notch_width == b->notch_width && a->notch_height == b->notch_height &&
           a->notch_side == b->notch_side && a->has_notch == b->has_notch &&
           a->color.r == b->color.r && a->color.g == b->color.g && a->color.b == b->color.b &&
           a->color.a == b->color.a;
}

static ID2D1DeviceContext *reach_d2d_shadow_bake_context(reach_render_backend *backend)
{
    if (backend->shadow_bake_context != nullptr)
    {
        return backend->shadow_bake_context;
    }
    if (backend->d2d_device == nullptr)
    {
        return nullptr;
    }

    HRESULT hr = backend->d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                                          &backend->shadow_bake_context);
    if (FAILED(hr))
    {
        reach_d2d_log_hresult(L"CreateDeviceContext(shadow bake)", hr);
        backend->shadow_bake_context = nullptr;
        return nullptr;
    }

    backend->shadow_bake_context->SetDpi(96.0f, 96.0f);
    return backend->shadow_bake_context;
}

static HRESULT reach_d2d_shadow_fill_list(ID2D1DeviceContext *context, ID2D1Geometry *geometry,
                                          ID2D1Brush *brush, D2D1_MATRIX_3X2_F transform,
                                          ID2D1CommandList **out_list)
{
    ComPtr<ID2D1CommandList> list;
    HRESULT hr = context->CreateCommandList(&list);
    if (FAILED(hr))
    {
        return hr;
    }

    context->SetTarget(list.Get());
    context->BeginDraw();
    context->SetTransform(transform);
    context->FillGeometry(geometry, brush);
    context->SetTransform(D2D1::Matrix3x2F::Identity());
    hr = context->EndDraw();
    context->SetTarget(nullptr);

    if (SUCCEEDED(hr))
    {
        hr = list->Close();
    }
    if (FAILED(hr))
    {
        return hr;
    }

    *out_list = list.Detach();
    return S_OK;
}

static reach_result reach_d2d_shadow_bake(reach_render_backend *backend,
                                          const reach_d2d_shadow_key *key,
                                          ID2D1Bitmap1 **out_bitmap)
{
    ID2D1DeviceContext *context = reach_d2d_shadow_bake_context(backend);
    if (context == nullptr || backend->factory == nullptr)
    {
        return REACH_ERROR;
    }

    reach_shadow shadow = reach_d2d_shadow_from_key(key);
    reach_shadow_pad pad = reach_theme_shadow_pad(&shadow, 1.0f);
    float content_width = reach_d2d_shadow_bake_width(key);
    float content_height = reach_d2d_shadow_bake_height(key);

    reach_render_command shape = {};
    shape.rect.x = pad.left;
    shape.rect.y = pad.top;
    shape.rect.width = content_width;
    shape.rect.height = content_height;
    shape.radius = key->radius;
    shape.notch_width = key->has_notch ? key->notch_width : 0.0f;
    shape.notch_height = key->has_notch ? key->notch_height : 0.0f;
    shape.notch_side = key->notch_side;
    shape.notch_center_x =
        key->has_notch ? pad.left + key->notch_offset_x : pad.left + content_width * 0.5f;

    ID2D1PathGeometry *geometry = nullptr;
    if (reach_d2d_create_notched_rounded_rect_geometry(backend->factory, &shape, &geometry) !=
        REACH_OK)
    {
        return REACH_ERROR;
    }

    UINT bitmap_width = (UINT)ceilf(content_width + pad.left + pad.right);
    UINT bitmap_height = (UINT)ceilf(content_height + pad.top + pad.bottom);
    if (bitmap_width == 0 || bitmap_height == 0)
    {
        geometry->Release();
        return REACH_ERROR;
    }

    ComPtr<ID2D1SolidColorBrush> mask;
    HRESULT hr = context->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), &mask);

    ComPtr<ID2D1CommandList> caster;
    if (SUCCEEDED(hr))
    {
        hr = reach_d2d_shadow_fill_list(
            context, geometry, mask.Get(),
            D2D1::Matrix3x2F::Translation(key->offset_x, key->offset_y), &caster);
    }

    ComPtr<ID2D1CommandList> cutter;
    if (SUCCEEDED(hr))
    {
        hr = reach_d2d_shadow_fill_list(context, geometry, mask.Get(),
                                        D2D1::Matrix3x2F::Identity(), &cutter);
    }

    geometry->Release();

    ComPtr<ID2D1Effect> blur;
    if (SUCCEEDED(hr))
    {
        hr = context->CreateEffect(CLSID_D2D1Shadow, &blur);
    }
    if (SUCCEEDED(hr))
    {
        blur->SetInput(0, caster.Get());
        hr = blur->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, key->blur * 0.5f);
    }
    if (SUCCEEDED(hr))
    {
        D2D1_COLOR_F color = reach_d2d_color(key->color);
        hr = blur->SetValue(D2D1_SHADOW_PROP_COLOR,
                            D2D1::Vector4F(color.r, color.g, color.b, color.a));
    }

    ComPtr<ID2D1Effect> cutout;
    if (SUCCEEDED(hr))
    {
        hr = context->CreateEffect(CLSID_D2D1Composite, &cutout);
    }
    if (SUCCEEDED(hr))
    {
        hr = cutout->SetValue(D2D1_COMPOSITE_PROP_MODE, D2D1_COMPOSITE_MODE_DESTINATION_OUT);
    }
    if (SUCCEEDED(hr))
    {
        cutout->SetInputEffect(0, blur.Get());
        cutout->SetInput(1, cutter.Get());
    }

    ComPtr<ID2D1Bitmap1> bitmap;
    if (SUCCEEDED(hr))
    {
        D2D1_BITMAP_PROPERTIES1 properties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        hr = context->CreateBitmap(D2D1::SizeU(bitmap_width, bitmap_height), nullptr, 0,
                                   &properties, &bitmap);
    }

    if (SUCCEEDED(hr))
    {
        context->SetTarget(bitmap.Get());
        context->BeginDraw();
        context->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
        context->DrawImage(cutout.Get());
        hr = context->EndDraw();
        context->SetTarget(nullptr);
    }

    if (FAILED(hr))
    {
        reach_d2d_log_hresult(L"shadow bake", hr);
        return REACH_ERROR;
    }

    *out_bitmap = bitmap.Detach();
    return REACH_OK;
}

static int32_t reach_d2d_shadow_key_is_sized(const reach_d2d_shadow_key *key)
{
    return !reach_d2d_shadow_stretches_x(key) || !reach_d2d_shadow_stretches_y(key);
}

static void reach_d2d_shadow_retire_oldest_sized(reach_render_backend *backend)
{
    size_t sized = 0;
    for (const reach_d2d_shadow_cache_entry &entry : backend->shadow_cache)
    {
        if (reach_d2d_shadow_key_is_sized(&entry.key))
        {
            ++sized;
        }
    }
    if (sized < REACH_D2D_SHADOW_MAX_SIZED_ENTRIES)
    {
        return;
    }

    for (size_t index = 0; index < backend->shadow_cache.size(); ++index)
    {
        if (!reach_d2d_shadow_key_is_sized(&backend->shadow_cache[index].key))
        {
            continue;
        }
        if (backend->shadow_cache[index].bitmap != nullptr)
        {
            backend->shadow_cache[index].bitmap->Release();
        }
        backend->shadow_cache.erase(backend->shadow_cache.begin() + (ptrdiff_t)index);
        return;
    }
}

static ID2D1Bitmap1 *reach_d2d_shadow_acquire(reach_render_backend *backend,
                                              const reach_d2d_shadow_key *key)
{
    for (const reach_d2d_shadow_cache_entry &entry : backend->shadow_cache)
    {
        if (reach_d2d_shadow_key_equal(&entry.key, key))
        {
            return entry.bitmap;
        }
    }

    if (reach_d2d_shadow_key_is_sized(key))
    {
        reach_d2d_shadow_retire_oldest_sized(backend);
    }

    ID2D1Bitmap1 *bitmap = nullptr;
    if (reach_d2d_shadow_bake(backend, key, &bitmap) != REACH_OK)
    {
        return nullptr;
    }

    reach_d2d_shadow_cache_entry entry = {};
    entry.key = *key;
    entry.bitmap = bitmap;
    backend->shadow_cache.push_back(entry);
    return bitmap;
}

static void reach_d2d_shadow_axis(float source_size, float dest_origin, float dest_size,
                                  float near_inset, float far_inset, int32_t stretch,
                                  float *source, float *dest)
{
    source[0] = 0.0f;
    dest[0] = dest_origin;

    if (stretch)
    {
        source[1] = near_inset;
        source[2] = source_size - far_inset;
        dest[1] = dest_origin + near_inset;
        dest[2] = dest_origin + dest_size - far_inset;
    }
    else
    {
        source[1] = source_size;
        source[2] = source_size;
        dest[1] = dest_origin + dest_size;
        dest[2] = dest_origin + dest_size;
    }

    source[3] = source_size;
    dest[3] = dest_origin + dest_size;
}

reach_result reach_d2d_draw_shadow(reach_render_backend *backend,
                                   const reach_render_command *command)
{
    REACH_ASSERT(backend != nullptr);
    REACH_ASSERT(command != nullptr);
    if (backend == nullptr || command == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (backend->d2d_context == nullptr || command->color.a <= 0.0f ||
        command->rect.width <= 0.0f || command->rect.height <= 0.0f)
    {
        return REACH_OK;
    }

    reach_d2d_shadow_key key = reach_d2d_shadow_key_from_command(command);
    ID2D1Bitmap1 *bitmap = reach_d2d_shadow_acquire(backend, &key);
    if (bitmap == nullptr)
    {
        return REACH_ERROR;
    }

    reach_shadow shadow = reach_d2d_shadow_from_command(command);
    reach_shadow_pad pad = reach_theme_shadow_pad(&shadow, 1.0f);
    float cap = reach_d2d_shadow_cap(&key);
    float source_width = reach_d2d_shadow_bake_width(&key) + pad.left + pad.right;
    float source_height = reach_d2d_shadow_bake_height(&key) + pad.top + pad.bottom;

    float source_x[4] = {};
    float source_y[4] = {};
    float dest_x[4] = {};
    float dest_y[4] = {};

    reach_d2d_shadow_axis(source_width, command->rect.x - pad.left,
                          command->rect.width + pad.left + pad.right, pad.left + cap,
                          pad.right + cap, reach_d2d_shadow_stretches_x(&key), source_x, dest_x);
    reach_d2d_shadow_axis(source_height, command->rect.y - pad.top,
                          command->rect.height + pad.top + pad.bottom,
                          pad.top + cap + reach_d2d_shadow_notch_reserve_top(&key),
                          pad.bottom + cap + reach_d2d_shadow_notch_reserve_bottom(&key),
                          reach_d2d_shadow_stretches_y(&key), source_y, dest_y);

    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            if (row == 1 && column == 1)
            {
                continue;
            }
            if (source_x[column + 1] <= source_x[column] ||
                source_y[row + 1] <= source_y[row] || dest_x[column + 1] <= dest_x[column] ||
                dest_y[row + 1] <= dest_y[row])
            {
                continue;
            }

            D2D1_RECT_F source = D2D1::RectF(source_x[column], source_y[row],
                                             source_x[column + 1], source_y[row + 1]);
            D2D1_RECT_F destination =
                D2D1::RectF(dest_x[column], dest_y[row], dest_x[column + 1], dest_y[row + 1]);
            backend->d2d_context->DrawBitmap(bitmap, &destination, 1.0f,
                                             D2D1_INTERPOLATION_MODE_LINEAR, &source, nullptr);
        }
    }

    return REACH_OK;
}

void reach_d2d_clear_shadow_cache(reach_render_backend *backend)
{
    if (backend == nullptr)
    {
        return;
    }

    for (reach_d2d_shadow_cache_entry &entry : backend->shadow_cache)
    {
        if (entry.bitmap != nullptr)
        {
            entry.bitmap->Release();
        }
    }

    backend->shadow_cache.clear();
}

void reach_d2d_release_shadow_bake_context(reach_render_backend *backend)
{
    if (backend == nullptr || backend->shadow_bake_context == nullptr)
    {
        return;
    }

    backend->shadow_bake_context->SetTarget(nullptr);
    backend->shadow_bake_context->Release();
    backend->shadow_bake_context = nullptr;
}
