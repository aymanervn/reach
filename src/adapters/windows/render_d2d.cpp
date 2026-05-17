#include "reach/platform/windows_adapters.h"

#include "reach/ports/render_backend.h"

#include <windows.h>
#include <d2d1_1.h>
#include <dcomp.h>
#include <dwrite.h>
#include <wincodec.h>

#include <stdint.h>
#include <new>
#include <vector>

struct reach_d2d_icon_cache_entry {
    uintptr_t icon_id;
    ID2D1Bitmap *bitmap;
};

struct reach_render_backend {
    HWND hwnd;
    ID2D1Factory1 *factory;
    ID2D1HwndRenderTarget *target;
    IDWriteFactory *text_factory;
    IWICImagingFactory *wic_factory;
    std::vector<reach_d2d_icon_cache_entry> icon_cache;
    UINT target_width;
    UINT target_height;
};

static D2D1_COLOR_F reach_d2d_color(reach_color color)
{
    return D2D1::ColorF(color.r, color.g, color.b, color.a);
}

static reach_result reach_d2d_create_target(reach_render_backend *backend)
{
    REACH_ASSERT(backend != nullptr);
    if (backend == nullptr || backend->hwnd == nullptr || backend->factory == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    RECT client = {};
    GetClientRect(backend->hwnd, &client);
    UINT width = static_cast<UINT>(client.right - client.left);
    UINT height = static_cast<UINT>(client.bottom - client.top);
    if (width == 0) {
        width = 1;
    }
    if (height == 0) {
        height = 1;
    }

    HRESULT hr = backend->factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(backend->hwnd, D2D1::SizeU(width, height)),
        &backend->target);
    if (SUCCEEDED(hr)) {
        backend->target_width = width;
        backend->target_height = height;
    }
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static void reach_d2d_clear_icon_cache(reach_render_backend *backend)
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

static reach_result reach_d2d_begin_frame(reach_render_backend *backend)
{
    REACH_ASSERT(backend != nullptr);
    if (backend == nullptr || backend->target == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    RECT client = {};
    GetClientRect(backend->hwnd, &client);
    UINT width = static_cast<UINT>(client.right - client.left);
    UINT height = static_cast<UINT>(client.bottom - client.top);
    if (width == 0) {
        width = 1;
    }
    if (height == 0) {
        height = 1;
    }
    if (width != backend->target_width || height != backend->target_height) {
        reach_d2d_clear_icon_cache(backend);
        HRESULT resize_hr = backend->target->Resize(D2D1::SizeU(width, height));
        if (FAILED(resize_hr)) {
            return REACH_ERROR;
        }
        backend->target_width = width;
        backend->target_height = height;
    }

    backend->target->BeginDraw();
    backend->target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
    return REACH_OK;
}

static reach_result reach_d2d_end_frame(reach_render_backend *backend)
{
    REACH_ASSERT(backend != nullptr);
    if (backend == nullptr || backend->target == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    HRESULT hr = backend->target->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        reach_d2d_clear_icon_cache(backend);
        backend->target->Release();
        backend->target = nullptr;
        return reach_d2d_create_target(backend);
    }
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_d2d_create_icon_bitmap(reach_render_backend *backend, uintptr_t icon_id, ID2D1Bitmap **out_bitmap)
{
    REACH_ASSERT(backend != nullptr);
    REACH_ASSERT(out_bitmap != nullptr);
    if (backend == nullptr || backend->target == nullptr || backend->wic_factory == nullptr || icon_id == 0 || out_bitmap == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_bitmap = nullptr;
    IWICBitmap *wic_bitmap = nullptr;
    HRESULT hr = backend->wic_factory->CreateBitmapFromHICON(reinterpret_cast<HICON>(icon_id), &wic_bitmap);
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
        hr = backend->target->CreateBitmapFromWicBitmap(converter, nullptr, &bitmap);
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

static reach_result reach_d2d_get_icon_bitmap(reach_render_backend *backend, uintptr_t icon_id, ID2D1Bitmap **out_bitmap)
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

static reach_result reach_d2d_draw_icon(reach_render_backend *backend, const reach_render_command *command)
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

    D2D1_RECT_F rect = D2D1::RectF(command->rect.x, command->rect.y, command->rect.x + command->rect.width, command->rect.y + command->rect.height);
    backend->target->DrawBitmap(bitmap, rect, command->color.a, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    return REACH_OK;
}

static reach_result reach_d2d_execute(reach_render_backend *backend, const reach_render_command_buffer *commands)
{
    REACH_ASSERT(backend != nullptr);
    REACH_ASSERT(commands != nullptr);
    if (backend == nullptr || backend->target == nullptr || commands == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < commands->count; ++index) {
        const reach_render_command *command = &commands->commands[index];
        if (command->type == REACH_RENDER_COMMAND_ICON && command->icon_id != 0) {
            reach_result result = reach_d2d_draw_icon(backend, command);
            if (result == REACH_OK) {
                continue;
            }
        }

        if (command->type == REACH_RENDER_COMMAND_RECT || command->type == REACH_RENDER_COMMAND_ICON) {
            ID2D1SolidColorBrush *brush = nullptr;
            HRESULT hr = backend->target->CreateSolidColorBrush(reach_d2d_color(command->color), &brush);
            if (FAILED(hr)) {
                return REACH_ERROR;
            }

            D2D1_ROUNDED_RECT rect = D2D1::RoundedRect(
                D2D1::RectF(command->rect.x, command->rect.y, command->rect.x + command->rect.width, command->rect.y + command->rect.height),
                command->radius,
                command->radius);
            backend->target->FillRoundedRectangle(rect, brush);
            brush->Release();
        } else if (command->type == REACH_RENDER_COMMAND_TEXT) {
            IDWriteTextFormat *format = nullptr;
            ID2D1SolidColorBrush *brush = nullptr;
            HRESULT hr = backend->text_factory->CreateTextFormat(
                L"Segoe UI",
                nullptr,
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                16.0f,
                L"",
                &format);
            if (SUCCEEDED(hr)) {
                (void)format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                (void)format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }
            if (SUCCEEDED(hr)) {
                hr = backend->target->CreateSolidColorBrush(reach_d2d_color(command->color), &brush);
            }
            if (FAILED(hr)) {
                if (format != nullptr) {
                    format->Release();
                }
                return REACH_ERROR;
            }

            D2D1_RECT_F rect = D2D1::RectF(command->rect.x, command->rect.y, command->rect.x + command->rect.width, command->rect.y + command->rect.height);
            backend->target->DrawTextW(reinterpret_cast<const wchar_t *>(command->text), static_cast<UINT32>(wcslen(reinterpret_cast<const wchar_t *>(command->text))), format, rect, brush);
            brush->Release();
            format->Release();
        } else if (command->type == REACH_RENDER_COMMAND_BLUR_BACKGROUND) {
            // Implement blur through the platform surface/composition path, not core rendering logic.
        }
    }

    return REACH_OK;
}

static void reach_d2d_destroy(reach_render_backend *backend)
{
    if (backend == nullptr) {
        return;
    }

    reach_d2d_clear_icon_cache(backend);
    if (backend->target != nullptr) {
        backend->target->Release();
    }
    if (backend->text_factory != nullptr) {
        backend->text_factory->Release();
    }
    if (backend->wic_factory != nullptr) {
        backend->wic_factory->Release();
    }
    if (backend->factory != nullptr) {
        backend->factory->Release();
    }
    delete backend;
}

reach_result reach_windows_create_d2d_render_backend(void *native_window, reach_render_backend_port *out_port)
{
    REACH_ASSERT(native_window != nullptr);
    REACH_ASSERT(out_port != nullptr);
    if (native_window == nullptr || out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_render_backend *backend = new (std::nothrow) reach_render_backend();
    if (backend == nullptr) {
        return REACH_ERROR;
    }

    backend->hwnd = static_cast<HWND>(native_window);
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &backend->factory);
    if (SUCCEEDED(hr)) {
        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown **>(&backend->text_factory));
    }
    if (SUCCEEDED(hr)) {
        hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&backend->wic_factory));
    }
    if (SUCCEEDED(hr)) {
        hr = reach_d2d_create_target(backend) == REACH_OK ? S_OK : E_FAIL;
    }
    if (FAILED(hr)) {
        reach_d2d_destroy(backend);
        return REACH_ERROR;
    }

    out_port->backend = backend;
    out_port->ops.begin_frame = reach_d2d_begin_frame;
    out_port->ops.end_frame = reach_d2d_end_frame;
    out_port->ops.execute = reach_d2d_execute;
    out_port->ops.destroy = reach_d2d_destroy;
    return REACH_OK;
}
