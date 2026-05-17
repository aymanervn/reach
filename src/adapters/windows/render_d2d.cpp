#include "reach/platform/windows_adapters.h"

#include "reach/ports/render_backend.h"

#include <windows.h>
#include <d2d1_1.h>
#include <dcomp.h>
#include <dwrite.h>

#include <new>

struct reach_render_backend {
    HWND hwnd;
    ID2D1Factory1 *factory;
    ID2D1HwndRenderTarget *target;
    IDWriteFactory *text_factory;
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
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_d2d_begin_frame(reach_render_backend *backend)
{
    REACH_ASSERT(backend != nullptr);
    if (backend == nullptr || backend->target == nullptr) {
        return REACH_INVALID_ARGUMENT;
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
        backend->target->Release();
        backend->target = nullptr;
        return reach_d2d_create_target(backend);
    }
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
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

    if (backend->target != nullptr) {
        backend->target->Release();
    }
    if (backend->text_factory != nullptr) {
        backend->text_factory->Release();
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
