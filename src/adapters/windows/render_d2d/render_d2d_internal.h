#ifndef REACH_ADAPTERS_WINDOWS_RENDER_D2D_INTERNAL_H
#define REACH_ADAPTERS_WINDOWS_RENDER_D2D_INTERNAL_H

#include "reach/ports/render_backend.h"

#include <windows.h>
#include <d2d1_1.h>
#include <dcomp.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <roapi.h>
#include <wincodec.h>
#include <windows.ui.composition.h>
#include <windows.ui.composition.desktop.h>
#include <windows.ui.composition.interop.h>
#include <wrl/client.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <new>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

struct reach_d2d_icon_cache_entry {
    uintptr_t icon_id;
    ID2D1Bitmap *bitmap;
};

struct reach_render_backend {
    HWND hwnd;

    ID2D1Factory1 *factory;
    ID2D1HwndRenderTarget *target;

    ID3D11Device *d3d_device;
    IDXGIDevice *dxgi_device;
    ID2D1Device *d2d_device;
    ID2D1DeviceContext *d2d_context;
    IDXGISwapChain1 *swap_chain;
    ID2D1Bitmap1 *swap_chain_bitmap;

    IDCompositionDevice *dcomp_device;
    IDCompositionTarget *dcomp_target;
    IDCompositionVisual *dcomp_visual;

    int ro_initialized;
    ComPtr<ABI::Windows::UI::Composition::ICompositor> compositor;
    ComPtr<ABI::Windows::UI::Composition::ICompositionTarget> composition_target;
    ComPtr<ABI::Windows::UI::Composition::IContainerVisual> root_visual;
    ComPtr<ABI::Windows::UI::Composition::ISpriteVisual> backdrop_visual;
    ComPtr<ABI::Windows::UI::Composition::ISpriteVisual> swap_chain_visual;
    ComPtr<ABI::Windows::UI::Composition::ICompositionSurface> composition_surface;

    IDWriteFactory *text_factory;
    IWICImagingFactory *wic_factory;

    std::vector<reach_d2d_icon_cache_entry> icon_cache;

    UINT target_width;
    UINT target_height;
};

/* Shared helpers */
ID2D1RenderTarget *reach_d2d_target(reach_render_backend *backend);
D2D1_COLOR_F reach_d2d_color(reach_color color);
void reach_d2d_log_hresult(const wchar_t *context, HRESULT hr);

/* Device / target setup */
HRESULT reach_d3d11_create_device(ID3D11Device **out_device, D3D_FEATURE_LEVEL *out_level);
reach_result reach_d2d_create_target(reach_render_backend *backend);

/* DirectComposition target setup */
reach_result reach_dcomp_create_swap_chain(reach_render_backend *backend, UINT width, UINT height);
reach_result reach_dcomp_create_target_bitmap(reach_render_backend *backend);
reach_result reach_dcomp_create_target(reach_render_backend *backend);
reach_result reach_dcomp_create_blur_target(reach_render_backend *backend);

/* WinUI Composition target setup */
HRESULT reach_winrt_activate_compositor(ABI::Windows::UI::Composition::ICompositor **out_compositor);
HRESULT reach_visual_set_size(IInspectable *inspectable, float width, float height);
reach_result reach_wuc_create_target(reach_render_backend *backend);

/* Frame lifecycle */
reach_result reach_d2d_begin_frame(reach_render_backend *backend);
reach_result reach_d2d_end_frame(reach_render_backend *backend);

/* Raster icon drawing */
void reach_d2d_clear_icon_cache(reach_render_backend *backend);
reach_result reach_d2d_draw_icon(reach_render_backend *backend, const reach_render_command *command);

/* Vector icon / SVG resource drawing */
reach_result reach_d2d_draw_vector_icon(reach_render_backend *backend, const reach_render_command *command);

/* Shape drawing */
reach_result reach_d2d_draw_backplate_edge(ID2D1RenderTarget *target, const reach_render_command *command);
reach_result reach_d2d_draw_notched_rounded_rect(ID2D1RenderTarget *target, const reach_render_command *command);
reach_result reach_d2d_draw_triangle(ID2D1RenderTarget *target, const reach_render_command *command);
reach_result reach_d2d_draw_notch_stroke(ID2D1RenderTarget *target, const reach_render_command *command);
reach_result reach_d2d_draw_rect_or_rounded_rect(ID2D1RenderTarget *target, const reach_render_command *command);

/* Text drawing */
reach_result reach_d2d_draw_text(reach_render_backend *backend, const reach_render_command *command);

/* Command execution */
reach_result reach_d2d_execute(reach_render_backend *backend, const reach_render_command_buffer *commands);

/* Backend lifetime */
void reach_d2d_destroy(reach_render_backend *backend);

#endif
