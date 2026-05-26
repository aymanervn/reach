#include "render_d2d_internal.h"

ID2D1RenderTarget *reach_d2d_target(reach_render_backend *backend)
{
    if (backend == nullptr) {
        return nullptr;
    }
    if (backend->d2d_context != nullptr) {
        return backend->d2d_context;
    }
    return backend->target;
}

D2D1_COLOR_F reach_d2d_color(reach_color color)
{
    return D2D1::ColorF(color.r, color.g, color.b, color.a);
}

void reach_d2d_log_hresult(const wchar_t *context, HRESULT hr)
{
    (void)context;
    (void)hr;
}

HRESULT reach_d3d11_create_device(ID3D11Device **out_device, D3D_FEATURE_LEVEL *out_level)
{
    if (out_device == nullptr || out_level == nullptr) {
        return E_INVALIDARG;
    }

    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        levels,
        sizeof(levels) / sizeof(levels[0]),
        D3D11_SDK_VERSION,
        out_device,
        out_level,
        nullptr);

    if (hr == E_INVALIDARG) {
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels + 1,
            (sizeof(levels) / sizeof(levels[0])) - 1,
            D3D11_SDK_VERSION,
            out_device,
            out_level,
            nullptr);
    }

    return hr;
}

reach_result reach_d2d_create_target(reach_render_backend *backend)
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
