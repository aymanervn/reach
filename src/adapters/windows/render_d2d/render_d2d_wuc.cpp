#include "render_d2d_internal.h"

HRESULT reach_winrt_activate_compositor(
    ABI::Windows::UI::Composition::ICompositor **out_compositor
)
{
    if (out_compositor == nullptr) {
        return E_INVALIDARG;
    }

    *out_compositor = nullptr;

    HSTRING class_name = nullptr;
    HRESULT hr = WindowsCreateString(
        L"Windows.UI.Composition.Compositor",
        static_cast<UINT32>(wcslen(L"Windows.UI.Composition.Compositor")),
        &class_name);

    if (SUCCEEDED(hr)) {
        IInspectable *inspectable = nullptr;
        hr = RoActivateInstance(class_name, &inspectable);
        if (SUCCEEDED(hr) && inspectable != nullptr) {
            hr = inspectable->QueryInterface(IID_PPV_ARGS(out_compositor));
            inspectable->Release();
        }
        WindowsDeleteString(class_name);
    }

    return hr;
}

HRESULT reach_visual_set_size(IInspectable *inspectable, float width, float height)
{
    if (inspectable == nullptr) {
        return E_INVALIDARG;
    }

    ComPtr<ABI::Windows::UI::Composition::IVisual> visual;
    HRESULT hr = inspectable->QueryInterface(IID_PPV_ARGS(&visual));

    if (SUCCEEDED(hr)) {
        ABI::Windows::Foundation::Numerics::Vector2 size = {};
        size.X = width;
        size.Y = height;
        hr = visual->put_Size(size);
    }

    return hr;
}

reach_result reach_wuc_create_target(reach_render_backend *backend)
{
    if (backend == nullptr || backend->hwnd == nullptr || backend->factory == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    BOOL use_host_backdrop = TRUE;
    (void)DwmSetWindowAttribute(
        backend->hwnd,
        DWMWA_USE_HOSTBACKDROPBRUSH,
        &use_host_backdrop,
        sizeof(use_host_backdrop));

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

    HRESULT hr = RoInitialize(RO_INIT_SINGLETHREADED);
    reach_d2d_log_hresult(L"RoInitialize", hr);

    if (SUCCEEDED(hr)) {
        backend->ro_initialized = 1;
    } else if (hr == RPC_E_CHANGED_MODE) {
        hr = S_OK;
    }

    D3D_FEATURE_LEVEL actual_level = D3D_FEATURE_LEVEL_11_0;

    if (SUCCEEDED(hr)) {
        hr = reach_d3d11_create_device(&backend->d3d_device, &actual_level);
        reach_d2d_log_hresult(L"D3D11CreateDevice", hr);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->d3d_device->QueryInterface(IID_PPV_ARGS(&backend->dxgi_device));
        reach_d2d_log_hresult(L"ID3D11Device::QueryInterface IDXGIDevice", hr);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->factory->CreateDevice(backend->dxgi_device, &backend->d2d_device);
        reach_d2d_log_hresult(L"ID2D1Factory1::CreateDevice", hr);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->d2d_device->CreateDeviceContext(
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
            &backend->d2d_context);
        reach_d2d_log_hresult(L"ID2D1Device::CreateDeviceContext", hr);
    }
    if (SUCCEEDED(hr)) {
        hr = reach_dcomp_create_swap_chain(backend, width, height) == REACH_OK ? S_OK : E_FAIL;
        reach_d2d_log_hresult(L"CreateSwapChainForComposition", hr);
    }
    if (SUCCEEDED(hr)) {
        hr = reach_winrt_activate_compositor(&backend->compositor);
        static int32_t logged_compositor_activation_failure = 0;
        if (FAILED(hr) && !logged_compositor_activation_failure) {
            logged_compositor_activation_failure = 1;
            reach_d2d_log_hresult(L"RoActivateInstance(Compositor)", hr);
        }
    }

    ComPtr<ABI::Windows::UI::Composition::Desktop::ICompositorDesktopInterop> desktop_interop;
    if (SUCCEEDED(hr)) {
        hr = backend->compositor.As(&desktop_interop);
        reach_d2d_log_hresult(L"ICompositorDesktopInterop", hr);
    }

    ComPtr<ABI::Windows::UI::Composition::Desktop::IDesktopWindowTarget> desktop_target;
    if (SUCCEEDED(hr)) {
        hr = desktop_interop->CreateDesktopWindowTarget(
            backend->hwnd,
            TRUE,
            &desktop_target);
        reach_d2d_log_hresult(L"CreateDesktopWindowTarget", hr);
    }

    if (SUCCEEDED(hr)) {
        hr = desktop_target.As(&backend->composition_target);
        reach_d2d_log_hresult(L"ICompositionTarget", hr);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->compositor->CreateContainerVisual(&backend->root_visual);
        reach_d2d_log_hresult(L"CreateContainerVisual", hr);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->compositor->CreateSpriteVisual(&backend->backdrop_visual);
    }

    ComPtr<ABI::Windows::UI::Composition::ICompositor3> compositor3;
    if (SUCCEEDED(hr)) {
        hr = backend->compositor.As(&compositor3);
    }

    ComPtr<ABI::Windows::UI::Composition::ICompositionBackdropBrush> backdrop_brush;
    if (SUCCEEDED(hr)) {
        hr = compositor3->CreateHostBackdropBrush(&backdrop_brush);
        reach_d2d_log_hresult(L"CreateHostBackdropBrush", hr);
    }

    ComPtr<ABI::Windows::UI::Composition::ICompositionBrush> backdrop_base_brush;
    if (SUCCEEDED(hr)) {
        hr = backdrop_brush.As(&backdrop_base_brush);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->backdrop_visual->put_Brush(backdrop_base_brush.Get());
    }

    ComPtr<ABI::Windows::UI::Composition::ICompositorInterop> compositor_interop;
    if (SUCCEEDED(hr)) {
        hr = backend->compositor.As(&compositor_interop);
    }
    if (SUCCEEDED(hr)) {
        hr = compositor_interop->CreateCompositionSurfaceForSwapChain(
            backend->swap_chain,
            &backend->composition_surface);
        reach_d2d_log_hresult(L"CreateCompositionSurfaceForSwapChain", hr);
    }

    ComPtr<ABI::Windows::UI::Composition::ICompositionSurfaceBrush> surface_brush;
    if (SUCCEEDED(hr)) {
        hr = backend->compositor->CreateSurfaceBrushWithSurface(
            backend->composition_surface.Get(),
            &surface_brush);
    }

    if (SUCCEEDED(hr)) {
        hr = backend->compositor->CreateSpriteVisual(&backend->swap_chain_visual);
    }

    ComPtr<ABI::Windows::UI::Composition::ICompositionBrush> surface_base_brush;
    if (SUCCEEDED(hr)) {
        hr = surface_brush.As(&surface_base_brush);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->swap_chain_visual->put_Brush(surface_base_brush.Get());
    }

    ComPtr<ABI::Windows::UI::Composition::IVisualCollection> children;
    if (SUCCEEDED(hr)) {
        hr = backend->root_visual->get_Children(&children);
    }

    ComPtr<ABI::Windows::UI::Composition::IVisual> backdrop_base_visual;
    if (SUCCEEDED(hr)) {
        hr = backend->backdrop_visual.As(&backdrop_base_visual);
    }
    if (SUCCEEDED(hr)) {
        hr = children->InsertAtTop(backdrop_base_visual.Get());
    }

    ComPtr<ABI::Windows::UI::Composition::IVisual> swap_chain_base_visual;
    if (SUCCEEDED(hr)) {
        hr = backend->swap_chain_visual.As(&swap_chain_base_visual);
    }
    if (SUCCEEDED(hr)) {
        hr = children->InsertAtTop(swap_chain_base_visual.Get());
    }

    ComPtr<ABI::Windows::UI::Composition::IVisual> root_base_visual;
    if (SUCCEEDED(hr)) {
        hr = backend->root_visual.As(&root_base_visual);
    }
    if (SUCCEEDED(hr)) {
        hr = backend->composition_target->put_Root(root_base_visual.Get());
    }

    if (SUCCEEDED(hr)) {
        hr = reach_visual_set_size(root_base_visual.Get(), static_cast<float>(width), static_cast<float>(height));
    }
    if (SUCCEEDED(hr)) {
        hr = reach_visual_set_size(backdrop_base_visual.Get(), static_cast<float>(width), static_cast<float>(height));
    }
    if (SUCCEEDED(hr)) {
        hr = reach_visual_set_size(swap_chain_base_visual.Get(), static_cast<float>(width), static_cast<float>(height));
    }
    if (SUCCEEDED(hr)) {
        hr = reach_dcomp_create_target_bitmap(backend) == REACH_OK ? S_OK : E_FAIL;
    }
    if (SUCCEEDED(hr)) {
        backend->target_width = width;
        backend->target_height = height;
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}
