#ifndef REACH_ADAPTERS_WINDOWS_RENDER_D2D_INTERNAL_H
#define REACH_ADAPTERS_WINDOWS_RENDER_D2D_INTERNAL_H

#include "reach/ports/render_backend.h"

#include <windows.h>
#include <d2d1_1.h>
#include <dcomp.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <dwrite_3.h>
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

struct reach_d2d_icon_cache_entry
{
    uint64_t icon_id;
    ID2D1Bitmap *bitmap;
};

struct reach_d2d_shadow_key
{
    float content_width;
    float content_height;
    float radius;
    float blur;
    float offset_x;
    float offset_y;
    float notch_offset_x;
    float notch_width;
    float notch_height;
    int32_t notch_side;
    int32_t has_notch;
    reach_color color;
};

struct reach_d2d_shadow_cache_entry
{
    reach_d2d_shadow_key key;
    ID2D1Bitmap1 *bitmap;
};

struct reach_render_backend
{
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

    wchar_t ui_font_family[LF_FACESIZE];
    IDWriteFontCollection *bundled_font_collection;
    IDWriteInMemoryFontFileLoader *bundled_font_loader;
    int32_t bundled_font_attempted;
    int32_t use_bundled_font;

    std::vector<reach_d2d_icon_cache_entry> icon_cache;

    ID2D1DeviceContext *shadow_bake_context;
    std::vector<reach_d2d_shadow_cache_entry> shadow_cache;

    UINT target_width;
    UINT target_height;
    reach_rect_f32 backdrop_content_rect;
};

ID2D1RenderTarget *reach_d2d_target(reach_render_backend *backend);
void reach_d2d_set_ui_font(reach_render_backend *backend, int32_t use_bundled_font);
reach_result reach_d2d_measure_text(void *context, const uint16_t *text, float text_size,
                                    int32_t text_weight, float *out_width);
void reach_d2d_release_fonts(reach_render_backend *backend);
D2D1_COLOR_F reach_d2d_color(reach_color color);
void reach_d2d_log_hresult(const wchar_t *context, HRESULT hr);

HRESULT reach_d3d11_create_device(ID3D11Device **out_device, D3D_FEATURE_LEVEL *out_level);
reach_result reach_d2d_create_target(reach_render_backend *backend);

reach_result reach_dcomp_create_swap_chain(reach_render_backend *backend, UINT width, UINT height);
reach_result reach_dcomp_create_target_bitmap(reach_render_backend *backend);
reach_result reach_dcomp_create_target(reach_render_backend *backend);
reach_result reach_dcomp_create_blur_target(reach_render_backend *backend);

HRESULT
reach_winrt_activate_compositor(ABI::Windows::UI::Composition::ICompositor **out_compositor);
HRESULT reach_visual_set_size(IInspectable *inspectable, float width, float height);
reach_result reach_wuc_create_target(reach_render_backend *backend);

reach_result reach_d2d_begin_frame(reach_render_backend *backend);
reach_result reach_d2d_end_frame(reach_render_backend *backend);

void reach_d2d_clear_icon_cache(reach_render_backend *backend);
reach_result reach_d2d_draw_icon(reach_render_backend *backend,
                                 const reach_render_command *command);
reach_result reach_d2d_draw_blurred_image(reach_render_backend *backend,
                                          const reach_render_command *command);
void reach_d2d_release_icon_cache_entry(reach_render_backend *backend, uint64_t icon_id);
reach_result reach_d2d_draw_icon_tint(reach_render_backend *backend,
                                      const reach_render_command *command);

reach_result reach_d2d_draw_vector_icon(reach_render_backend *backend,
                                        const reach_render_command *command);

reach_result reach_d2d_create_corner_geometry(ID2D1RenderTarget *target, D2D1_RECT_F rect,
                                              float radius, int32_t corner_mask,
                                              ID2D1Geometry **out_geometry);

reach_result reach_d2d_create_notched_rounded_rect_geometry(ID2D1Factory *factory,
                                                            const reach_render_command *command,
                                                            ID2D1PathGeometry **out_geometry);

reach_result reach_d2d_fill_notched_rounded_rect(ID2D1RenderTarget *target,
                                                 const reach_render_command *command);

reach_result reach_d2d_draw_shadow(reach_render_backend *backend,
                                   const reach_render_command *command);
void reach_d2d_clear_shadow_cache(reach_render_backend *backend);
void reach_d2d_release_shadow_bake_context(reach_render_backend *backend);
reach_result reach_wuc_apply_content_clip(reach_render_backend *backend,
                                          reach_rect_f32 content_rect);
reach_result reach_d2d_draw_triangle(ID2D1RenderTarget *target,
                                     const reach_render_command *command);
reach_result reach_d2d_fill_rect_or_rounded_rect(ID2D1RenderTarget *target,
                                                 const reach_render_command *command);
reach_result reach_d2d_draw_arc_stroke(ID2D1RenderTarget *target,
                                       const reach_render_command *command);
reach_result reach_d2d_draw_clipped_rounded_rect(ID2D1RenderTarget *target,
                                                 const reach_render_command *command);

reach_result reach_d2d_draw_text(reach_render_backend *backend,
                                 const reach_render_command *command);
reach_result reach_d2d_draw_textbox(reach_render_backend *backend,
                                    const reach_render_command *command);

reach_result reach_d2d_execute(reach_render_backend *backend,
                               const reach_render_command_buffer *commands);

void reach_d2d_destroy(reach_render_backend *backend);

#endif
