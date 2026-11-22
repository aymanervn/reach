#include "render_d2d_internal.h"
#include "render_d2d_wuc_effect.h"

#include "reach/core/config.h"

#include <d2d1effects.h>
#include <DispatcherQueue.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Composition.h>

namespace composition = winrt::Windows::UI::Composition;

using reach_wuc_source = winrt::Windows::Graphics::Effects::IGraphicsEffectSource;
using reach_wuc_value = winrt::Windows::Foundation::IInspectable;

// Works on premultiplied values: rgb = clamp(scale * rgb + alpha_weight * a), alpha = 0.
static reach_wuc_source reach_wuc_glass_matrix(const wchar_t *name, reach_wuc_source input,
                                               float scale, float alpha_weight)
{
    using winrt::Windows::Foundation::PropertyValue;
    float matrix[20] = {};
    matrix[0] = matrix[5] = matrix[10] = scale;
    matrix[12] = matrix[13] = matrix[14] = alpha_weight;
    return winrt::make<reach_wuc_effect>(
        CLSID_D2D1ColorMatrix, name,
        std::vector<reach_wuc_value>{
            PropertyValue::CreateSingleArray(matrix),
            PropertyValue::CreateUInt32(D2D1_COLORMATRIX_ALPHA_MODE_STRAIGHT),
            PropertyValue::CreateBoolean(true)},
        std::vector<reach_wuc_source>{input});
}

// clamp(first + second_weight * second) on every channel.
static reach_wuc_source reach_wuc_glass_sum(const wchar_t *name, reach_wuc_source first,
                                            reach_wuc_source second, float second_weight)
{
    using winrt::Windows::Foundation::PropertyValue;
    float coefficients[4] = {0, 1, second_weight, 0};
    return winrt::make<reach_wuc_effect>(
        CLSID_D2D1ArithmeticComposite, name,
        std::vector<reach_wuc_value>{PropertyValue::CreateSingleArray(coefficients),
                                     PropertyValue::CreateBoolean(true)},
        std::vector<reach_wuc_source>{first, second});
}

// max(paint, ceiling) or min(paint, floor): the theme bound, never overriding the paint itself so
// borders and artwork past it keep their colors.
static reach_wuc_source reach_wuc_glass_bound(const wchar_t *matrix_name, const wchar_t *name,
                                              float level, bool ceiling)
{
    reach_wuc_source paint = composition::CompositionEffectSourceParameter(L"Paint");
    auto offset = reach_wuc_glass_matrix(matrix_name, paint, ceiling ? -1.0f : 1.0f,
                                         ceiling ? level : -level);
    return reach_wuc_glass_sum(name, paint, offset, ceiling ? 1.0f : -1.0f);
}

// A knee: the backdrop moves the glass freely until it reaches the theme bound, then stops there.
// Composition accepts only tree-shaped graphs with unique effect names, so the glass input is used
// once and each bound is built twice.
static reach_wuc_source reach_wuc_glass_ceiling(reach_wuc_source glass, float ceiling)
{
    auto excess = reach_wuc_glass_sum(
        L"GlassCeilingExcess",
        reach_wuc_glass_bound(L"GlassCeilingRaiseA", L"GlassCeilingBoundA", ceiling, true), glass,
        -1);
    return reach_wuc_glass_sum(
        L"GlassCeiling",
        reach_wuc_glass_bound(L"GlassCeilingRaiseB", L"GlassCeilingBoundB", ceiling, true), excess,
        -1);
}

static reach_wuc_source reach_wuc_glass_floor(reach_wuc_source glass, float floor)
{
    auto rise = reach_wuc_glass_sum(
        L"GlassFloorRise", glass,
        reach_wuc_glass_bound(L"GlassFloorLowerA", L"GlassFloorBoundA", floor, false), -1);
    return reach_wuc_glass_sum(
        L"GlassFloor",
        reach_wuc_glass_bound(L"GlassFloorLowerB", L"GlassFloorBoundB", floor, false), rise, 1);
}

static composition::CompositionEffectFactory
reach_wuc_create_glass_factory(const composition::Compositor &compositor, reach_glass glass)
{
    using winrt::Windows::Foundation::PropertyValue;
    // Windows already blurs the host backdrop. Blend it with the painted theme color.
    float matrix[20] = {};
    matrix[0] = matrix[5] = matrix[10] = 1;
    constexpr float default_weight = 0.32f;
    constexpr float maximum_weight = 0.85f;
    constexpr float default_intensity = (float)REACH_CONFIG_GLASS_INTENSITY_DEFAULT;
    constexpr float intensity_span = REACH_CONFIG_GLASS_INTENSITY_MAX - default_intensity;
    const float intensity = (float)glass.intensity;
    matrix[15] = intensity <= default_intensity
                     ? default_weight * intensity / default_intensity
                     : default_weight + (maximum_weight - default_weight) *
                                            (intensity - default_intensity) / intensity_span;
    auto tinted_backdrop = winrt::make<reach_wuc_effect>(
        CLSID_D2D1ColorMatrix, L"GlassOpacity",
        std::vector<reach_wuc_value>{
            PropertyValue::CreateSingleArray(matrix),
            PropertyValue::CreateUInt32(D2D1_COLORMATRIX_ALPHA_MODE_PREMULTIPLIED),
            PropertyValue::CreateBoolean(true)},
        std::vector<reach_wuc_source>{composition::CompositionEffectSourceParameter(L"Backdrop")});
    // Source-atop keeps the painted silhouette, including antialiased corners, as the mask.
    auto composite = winrt::make<reach_wuc_effect>(
        CLSID_D2D1Composite, L"Glass",
        std::vector<reach_wuc_value>{PropertyValue::CreateUInt32(D2D1_COMPOSITE_MODE_SOURCE_ATOP)},
        std::vector<reach_wuc_source>{composition::CompositionEffectSourceParameter(L"Paint"),
                                      tinted_backdrop.as<reach_wuc_source>()});
    reach_wuc_source result = composite.as<reach_wuc_source>();
    if (glass.ceiling < 1)
        result = reach_wuc_glass_ceiling(result, glass.ceiling);
    if (glass.floor > 0)
        result = reach_wuc_glass_floor(result, glass.floor);
    return compositor.CreateEffectFactory(
        result.as<winrt::Windows::Graphics::Effects::IGraphicsEffect>());
}

HRESULT reach_wuc_sync_glass(reach_render_backend *backend, reach_d2d_retained_layer &layer,
                             reach_glass glass, bool surface_recreated)
{
    if (surface_recreated)
        layer.glass = {};
    if (reach_glass_equal(layer.glass, glass))
        return S_OK;

    ComPtr<ABI::Windows::UI::Composition::ICompositionBrush> paint;
    HRESULT hr = layer.surface_brush.As(&paint);
    if (FAILED(hr))
        return hr;
    if (layer.glass.intensity != 0)
    {
        hr = layer.visual->put_Brush(paint.Get());
        if (FAILED(hr))
            return hr;
        layer.glass = {};
    }
    if (glass.intensity == 0)
        return S_OK;

    try
    {
        if (!reach_glass_equal(backend->glass_factory_glass, glass))
        {
            backend->glass_factory_glass = glass;
            backend->glass_factory.Reset();
            composition::Compositor compositor{nullptr};
            winrt::copy_from_abi(compositor, backend->compositor.Get());
            if (backend->host_backdrop_brush == nullptr)
            {
                // Only windows with a glass paint layer may sample the desktop behind them.
                BOOL use_host_backdrop = TRUE;
                HRESULT backdrop_result =
                    DwmSetWindowAttribute(backend->hwnd, DWMWA_USE_HOSTBACKDROPBRUSH,
                                          &use_host_backdrop, sizeof(use_host_backdrop));
                reach_d2d_log_hresult(L"DWMWA_USE_HOSTBACKDROPBRUSH", backdrop_result);
                if (FAILED(backdrop_result))
                    return S_OK;

                auto backdrop =
                    compositor.CreateHostBackdropBrush().as<composition::ICompositionBrush>();
                backend->host_backdrop_brush =
                    static_cast<ABI::Windows::UI::Composition::ICompositionBrush *>(
                        winrt::get_abi(backdrop));
            }
            auto factory = reach_wuc_create_glass_factory(compositor, glass);
            winrt::check_hresult(factory.ExtendedError());
            backend->glass_factory =
                static_cast<ABI::Windows::UI::Composition::ICompositionEffectFactory *>(
                    winrt::get_abi(factory));
        }
        if (backend->glass_factory != nullptr)
        {
            composition::CompositionEffectFactory factory{nullptr};
            composition::CompositionBrush backdrop{nullptr}, surface{nullptr};
            winrt::copy_from_abi(factory, backend->glass_factory.Get());
            winrt::copy_from_abi(backdrop, backend->host_backdrop_brush.Get());
            winrt::copy_from_abi(surface, paint.Get());
            auto effect = factory.CreateBrush();
            effect.SetSourceParameter(L"Backdrop", backdrop);
            effect.SetSourceParameter(L"Paint", surface);
            composition::SpriteVisual visual{nullptr};
            winrt::copy_from_abi(visual, layer.visual.Get());
            visual.Brush(effect);
            layer.glass = glass;
        }
    }
    catch (...)
    {
        reach_d2d_log_hresult(L"glass brush", winrt::to_hresult());
    }
    return S_OK;
}

HRESULT reach_winrt_activate_compositor(reach_render_device *device)
{
    try
    {
        // Win32 composition requires a dispatcher queue on the compositor's thread.
        if (!winrt::Windows::System::DispatcherQueue::GetForCurrentThread())
        {
            DispatcherQueueOptions options = {sizeof(options), DQTYPE_THREAD_CURRENT,
                                              DQTAT_COM_STA};
            ComPtr<ABI::Windows::System::IDispatcherQueueController> controller;
            HRESULT hr = CreateDispatcherQueueController(options, &controller);
            reach_d2d_log_hresult(L"CreateDispatcherQueueController", hr);
            winrt::check_hresult(hr);
            device->dispatcher_queue_controller = controller.Get();
        }

        composition::Compositor compositor;
        device->compositor =
            static_cast<ABI::Windows::UI::Composition::ICompositor *>(winrt::get_abi(compositor));
        return S_OK;
    }
    catch (...)
    {
        return winrt::to_hresult();
    }
}

void reach_winrt_shutdown_dispatcher_queue(reach_render_device *device)
{
    if (device->dispatcher_queue_controller == nullptr)
        return;

    try
    {
        winrt::Windows::System::DispatcherQueueController controller{nullptr};
        winrt::copy_from_abi(controller, device->dispatcher_queue_controller.Get());
        auto action = controller.ShutdownQueueAsync();
        const DWORD thread_id = GetCurrentThreadId();
        action.Completed([thread_id](const auto &, auto)
                         { (void)PostThreadMessageW(thread_id, WM_NULL, 0, 0); });

        MSG message = {};
        bool repost_quit = false;
        int quit_code = 0;
        while (action.Status() == winrt::Windows::Foundation::AsyncStatus::Started)
        {
            BOOL received = GetMessageW(&message, nullptr, 0, 0);
            if (received == -1)
                break;
            if (received == 0)
            {
                repost_quit = true;
                quit_code = static_cast<int>(message.wParam);
                continue;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (repost_quit)
            PostQuitMessage(quit_code);
    }
    catch (...)
    {
        reach_d2d_log_hresult(L"ShutdownQueueAsync", winrt::to_hresult());
    }
    device->dispatcher_queue_controller.Reset();
}

HRESULT reach_visual_set_size(IInspectable *inspectable, float width, float height)
{
    if (inspectable == nullptr)
    {
        return E_INVALIDARG;
    }

    ComPtr<ABI::Windows::UI::Composition::IVisual> visual;
    HRESULT hr = inspectable->QueryInterface(IID_PPV_ARGS(&visual));

    if (SUCCEEDED(hr))
    {
        ABI::Windows::Foundation::Numerics::Vector2 size = {};
        size.X = width;
        size.Y = height;
        hr = visual->put_Size(size);
    }

    return hr;
}

reach_result reach_wuc_create_target(reach_render_backend *backend)
{
    if (backend == nullptr || backend->hwnd == nullptr || backend->device->factory == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    RECT client = {};
    GetClientRect(backend->hwnd, &client);

    UINT width = static_cast<UINT>(client.right - client.left);
    UINT height = static_cast<UINT>(client.bottom - client.top);

    if (width == 0)
    {
        width = 1;
    }
    if (height == 0)
    {
        height = 1;
    }

    HRESULT hr = backend->device->d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                                                  &backend->d2d_context);
    reach_d2d_log_hresult(L"ID2D1Device::CreateDeviceContext", hr);
    if (SUCCEEDED(hr))
    {
        hr = reach_dcomp_create_swap_chain(backend, width, height) == REACH_OK ? S_OK : E_FAIL;
        reach_d2d_log_hresult(L"CreateSwapChainForComposition", hr);
    }
    if (SUCCEEDED(hr))
    {
        hr = backend->device->compositor != nullptr &&
                     backend->device->composition_graphics_device != nullptr
                 ? S_OK
                 : E_FAIL;
    }
    if (SUCCEEDED(hr))
    {
        backend->compositor = backend->device->compositor;
        backend->composition_graphics_device = backend->device->composition_graphics_device;
    }

    ComPtr<ABI::Windows::UI::Composition::Desktop::ICompositorDesktopInterop> desktop_interop;
    if (SUCCEEDED(hr))
    {
        hr = backend->compositor.As(&desktop_interop);
        reach_d2d_log_hresult(L"ICompositorDesktopInterop", hr);
    }

    ComPtr<ABI::Windows::UI::Composition::Desktop::IDesktopWindowTarget> desktop_target;
    if (SUCCEEDED(hr))
    {
        hr = desktop_interop->CreateDesktopWindowTarget(backend->hwnd, TRUE, &desktop_target);
        reach_d2d_log_hresult(L"CreateDesktopWindowTarget", hr);
    }

    if (SUCCEEDED(hr))
    {
        hr = desktop_target.As(&backend->composition_target);
        reach_d2d_log_hresult(L"ICompositionTarget", hr);
    }
    if (SUCCEEDED(hr))
    {
        hr = backend->compositor->CreateContainerVisual(&backend->root_visual);
        reach_d2d_log_hresult(L"CreateContainerVisual", hr);
    }

    ComPtr<ABI::Windows::UI::Composition::ICompositorInterop> compositor_interop;
    if (SUCCEEDED(hr))
    {
        hr = backend->compositor.As(&compositor_interop);
    }
    if (SUCCEEDED(hr))
    {
        hr = compositor_interop->CreateCompositionSurfaceForSwapChain(
            backend->swap_chain, &backend->composition_surface);
        reach_d2d_log_hresult(L"CreateCompositionSurfaceForSwapChain", hr);
    }

    ComPtr<ABI::Windows::UI::Composition::ICompositionSurfaceBrush> surface_brush;
    if (SUCCEEDED(hr))
    {
        hr = backend->compositor->CreateSurfaceBrushWithSurface(backend->composition_surface.Get(),
                                                                &surface_brush);
    }

    if (SUCCEEDED(hr))
    {
        hr = backend->compositor->CreateSpriteVisual(&backend->swap_chain_visual);
    }

    ComPtr<ABI::Windows::UI::Composition::ICompositionBrush> surface_base_brush;
    if (SUCCEEDED(hr))
    {
        hr = surface_brush.As(&surface_base_brush);
    }
    if (SUCCEEDED(hr))
    {
        hr = backend->swap_chain_visual->put_Brush(surface_base_brush.Get());
    }

    ComPtr<ABI::Windows::UI::Composition::IVisualCollection> children;
    if (SUCCEEDED(hr))
    {
        hr = backend->root_visual->get_Children(&children);
    }

    ComPtr<ABI::Windows::UI::Composition::IVisual> swap_chain_base_visual;
    if (SUCCEEDED(hr))
    {
        hr = backend->swap_chain_visual.As(&swap_chain_base_visual);
    }
    if (SUCCEEDED(hr))
    {
        hr = children->InsertAtTop(swap_chain_base_visual.Get());
    }

    ComPtr<ABI::Windows::UI::Composition::IVisual> root_base_visual;
    if (SUCCEEDED(hr))
    {
        hr = backend->root_visual.As(&root_base_visual);
    }
    if (SUCCEEDED(hr))
    {
        hr = backend->composition_target->put_Root(root_base_visual.Get());
    }

    if (SUCCEEDED(hr))
    {
        hr = reach_visual_set_size(root_base_visual.Get(), static_cast<float>(width),
                                   static_cast<float>(height));
    }
    if (SUCCEEDED(hr))
    {
        hr = reach_visual_set_size(swap_chain_base_visual.Get(), static_cast<float>(width),
                                   static_cast<float>(height));
    }
    if (SUCCEEDED(hr))
    {
        hr = reach_dcomp_create_target_bitmap(backend) == REACH_OK ? S_OK : E_FAIL;
    }
    if (SUCCEEDED(hr))
    {
        backend->target_width = width;
        backend->target_height = height;
    }

    return reach_d2d_device_check(backend->device, hr, L"wuc create target");
}
