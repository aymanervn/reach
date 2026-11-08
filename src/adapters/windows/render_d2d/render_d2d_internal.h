#ifndef REACH_ADAPTERS_WINDOWS_RENDER_D2D_INTERNAL_H
#define REACH_ADAPTERS_WINDOWS_RENDER_D2D_INTERNAL_H

#include "reach/ports/render_backend.h"
#include "reach/support/render_recovery.h"

#include <windows.h>
#include <d2d1_1.h>
#include <dcomp.h>
#include <d3d11.h>
#include <d3d11_4.h>
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
#include <atomic>
#include <memory>

using Microsoft::WRL::ComPtr;

constexpr uint64_t REACH_D2D_MAX_SURFACE_BYTES = UINT64_C(128) * 1024 * 1024;
constexpr float REACH_D2D_MAX_SURFACE_DIMENSION = 16384.0f;

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

struct reach_d2d_text_format_entry
{
    float size;
    int32_t weight;
    int32_t alignment;
    int32_t paragraph;
    int32_t ellipsis;
    ComPtr<IDWriteTextFormat> format;
};

struct reach_d2d_text_layout_entry
{
    std::wstring text;
    float size;
    float width;
    float height;
    int32_t weight;
    int32_t alignment;
    int32_t paragraph;
    int32_t ellipsis;
    ComPtr<IDWriteTextLayout> layout;
};

struct reach_d2d_cover_entry
{
    uint64_t icon_id;
    UINT width;
    UINT height;
    int32_t crop;
    float blur;
    float contrast;
    ComPtr<ID2D1Bitmap1> bitmap;
};

struct reach_d2d_shadow_scene;

struct reach_d2d_text_width_layout
{
    std::vector<float> thresholds;
    std::vector<float> widths;
};

struct reach_d2d_text_width_entry
{
    std::wstring text;
    float size;
    float width;
    float height;
    int32_t weight;
    int32_t alignment;
    int32_t ellipsis;
    reach_d2d_text_width_layout layout;
};

struct reach_d2d_sampled_text
{
    uint64_t owner;
    reach_render_command command;
    reach_motion width;
};

struct reach_d2d_sampled_textbox
{
    uint64_t owner;
    reach_render_command command;
    reach_render_group color;
};

struct reach_d2d_sampled_image
{
    uint64_t owner;
    reach_render_command command;
    reach_rect_motion bounds;
    float radius;
};

struct reach_d2d_fit_owner
{
    uint64_t owner;
    uint64_t prefix;
};

struct reach_d2d_retained_layer
{
    uint64_t id = 0;
    std::shared_ptr<reach_d2d_shadow_scene> shadow_scene;
    uint64_t shadow_bytes = 0;
    reach_render_group group = {};
    reach_rect_f32 raster_requested_bounds = {};
    std::vector<reach_render_command> commands;
    ComPtr<ABI::Windows::UI::Composition::IContainerVisual> container;
    ComPtr<ABI::Windows::UI::Composition::ISpriteVisual> visual;
    ComPtr<ABI::Windows::UI::Composition::ICompositionDrawingSurface> surface;
    ComPtr<ABI::Windows::UI::Composition::ICompositionSurfaceBrush> surface_brush;
    ComPtr<ABI::Windows::UI::Composition::ICompositionMaskBrush> tint_mask;
    ComPtr<ABI::Windows::UI::Composition::ICompositionColorBrush> tint_brush;
    ComPtr<ABI::Windows::UI::Composition::ICompositionPropertySet> tint_properties;
    ComPtr<ABI::Windows::UI::Composition::IInsetClip> inset_clip;
    ComPtr<ABI::Windows::UI::Composition::ICompositionRoundedRectangleGeometry> rounded_geometry;
    ComPtr<ABI::Windows::UI::Composition::ICompositionGeometricClip> rounded_clip;
    ComPtr<IDCompositionVisual2> dcomp_container;
    ComPtr<IDCompositionVisual2> dcomp_visual;
    ComPtr<IDCompositionSurface> dcomp_surface;
    ComPtr<IDCompositionScaleTransform> dcomp_scale;
    ComPtr<IDCompositionRectangleClip> dcomp_clip;
    ComPtr<IDCompositionColorMatrixEffect> dcomp_tint;
    ComPtr<IDCompositionColorMatrixEffect> dcomp_tint_mix;
    UINT width = 0;
    UINT height = 0;
    UINT capacity_width = 0;
    UINT capacity_height = 0;
    bool initialized = false;
    bool raster_birth_aligned = false;
    bool raster_finite_translation = false;
    bool has_children = false;
    bool text_phase_valid = false;
    bool text_pixel_aligned = false;
    float text_raster_phase_y = 0.0f;
    float text_presentation_phase_y = 0.0f;
    reach_point_f32 parent_origin = {};
};

struct reach_d2d_retained_attachment
{
    uint64_t id;
    uint64_t parent_id;
    int32_t has_shadow;
};

struct reach_render_device
{
    ID2D1Factory1 *factory = nullptr;
    IDWriteFactory *text_factory = nullptr;
    IWICImagingFactory *wic_factory = nullptr;
    ID3D11Device *d3d_device = nullptr;
    IDXGIDevice *dxgi_device = nullptr;
    ID2D1Device *d2d_device = nullptr;
    ComPtr<ABI::Windows::UI::Composition::ICompositor> compositor;
    ComPtr<ABI::Windows::UI::Composition::ICompositionGraphicsDevice> composition_graphics_device;
    uint32_t active_surfaces = 0;
    int ro_initialized = 0;

    HANDLE removed_event = nullptr;
    DWORD removed_cookie = 0;
    bool lost = false;
    HRESULT lost_hr = S_OK;
    HRESULT lost_reason = S_OK;
    uint32_t losses = 0;
    uint32_t recoveries = 0;
    uint64_t generation = 0;
    bool warp_active = false;
    reach_render_recovery recovery = {};

    std::vector<reach_d2d_icon_cache_entry> icon_cache;
    std::vector<reach_d2d_cover_entry> cover_cache;
    std::vector<reach_d2d_shadow_cache_entry> shadow_cache;
    ComPtr<ID2D1DeviceContext> shadow_bake_context;
    ComPtr<ID2D1DeviceContext> shadow_fallback_context;
    ComPtr<ID2D1DeviceContext> cover_bake_context;
    ID2D1Device *shadow_fallback_parent_device = nullptr;
    IDWriteFontCollection *bundled_font_collection = nullptr;
    IDWriteInMemoryFontFileLoader *bundled_font_loader = nullptr;
    bool bundled_font_attempted = false;
};

class reach_d2d_context_scope
{
  public:
    explicit reach_d2d_context_scope(ID2D1DeviceContext *value) : context(value)
    {
        reset();
    }
    ~reach_d2d_context_scope()
    {
        reset();
    }
    reach_d2d_context_scope(const reach_d2d_context_scope &) = delete;
    reach_d2d_context_scope &operator=(const reach_d2d_context_scope &) = delete;

  private:
    void reset()
    {
        if (context == nullptr)
        {
            return;
        }
        context->SetTarget(nullptr);
        context->SetTransform(D2D1::Matrix3x2F::Identity());
        context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
        context->SetUnitMode(D2D1_UNIT_MODE_DIPS);
    }

    ID2D1DeviceContext *context;
};

struct reach_render_backend
{
    HWND hwnd;

    reach_render_device *device;
    uint64_t device_generation = 0;
    bool surface_active = false;
    ID2D1RenderTarget *command_target = nullptr;

    ID2D1DeviceContext *d2d_context;
    IDXGISwapChain1 *swap_chain;
    ID2D1Bitmap1 *swap_chain_bitmap;

    IDCompositionDesktopDevice *dcomp_device;
    IDCompositionTarget *dcomp_target;
    IDCompositionVisual2 *dcomp_visual;

    ComPtr<ABI::Windows::UI::Composition::ICompositor> compositor;
    ComPtr<ABI::Windows::UI::Composition::ICompositionTarget> composition_target;
    ComPtr<ABI::Windows::UI::Composition::IContainerVisual> root_visual;
    ComPtr<ABI::Windows::UI::Composition::ISpriteVisual> backdrop_visual;
    ComPtr<ABI::Windows::UI::Composition::ISpriteVisual> swap_chain_visual;
    ComPtr<ABI::Windows::UI::Composition::ICompositionSurface> composition_surface;
    ComPtr<ABI::Windows::UI::Composition::ICompositionGraphicsDevice> composition_graphics_device;
    std::vector<reach_d2d_retained_layer> retained_layers;
    std::vector<reach_d2d_retained_attachment> retained_topology;
    bool retained_topology_valid = false;
    reach_transform_f32 scene_content_transform = {};
    bool has_scene_content_transform = false;
    double motion_sample_seconds = 0;
    uint64_t reported_retained_bytes = 0;
    bool requires_content_sampling = false;
    std::vector<reach_d2d_sampled_text> sampled_text;
    std::vector<reach_d2d_sampled_textbox> sampled_textboxes;
    std::vector<reach_d2d_sampled_image> sampled_images;
    std::vector<reach_d2d_fit_owner> image_fit_owners;
    uint64_t next_image_fit_prefix = UINT64_C(0xd000000000010000);
    std::vector<reach_d2d_fit_owner> text_width_owners;
    uint64_t next_text_width_prefix = UINT64_C(0xc000000000010000);
    ComPtr<ABI::Windows::UI::Composition::IContainerVisual> scene_visual;
    ComPtr<IDCompositionVisual2> dcomp_scene_visual;
    ComPtr<IDCompositionScaleTransform> scene_scale;
    reach_render_group scene_presentation;
    reach_motion scene_center_x;
    reach_motion scene_center_y;
    int32_t has_scene_presentation;
    ComPtr<ABI::Windows::UI::Composition::ICompositionScopedBatch> presentation_batch;
    EventRegistrationToken presentation_token = {};
    ComPtr<ABI::Windows::Foundation::IAsyncAction> presentation_commit;
    std::shared_ptr<std::atomic<uint64_t>> presentation_completed;
    uint64_t submitted_presentation_generation = 0;
    uint64_t reconciled_presentation_generation = 0;
    reach_result presentation_result = REACH_OK;

    std::vector<reach_d2d_text_format_entry> text_formats;
    std::vector<reach_d2d_text_layout_entry> text_layouts;
    std::vector<reach_d2d_text_width_entry> text_width_layouts;
    wchar_t ui_font_family[LF_FACESIZE];
    int32_t use_bundled_font;

    UINT target_width;
    UINT target_height;
    reach_rect_f32 backdrop_content_rect;
};

ID2D1RenderTarget *reach_d2d_target(reach_render_backend *backend);
int32_t reach_d2d_retained_supported(const reach_render_backend *backend);
int32_t reach_d2d_content_sampling_required(const reach_render_backend *backend);
reach_result reach_d2d_sample_content(reach_render_backend *backend);
reach_result reach_d2d_text_fit_bounds(reach_render_backend *backend,
                                       const reach_render_command *command,
                                       reach_rect_f32 *out_bounds);
reach_result reach_d2d_text_width_layout_for(reach_render_backend *backend,
                                             const reach_render_command *command,
                                             const reach_d2d_text_width_layout **out);
reach_result reach_d2d_sync_retained(reach_render_backend *backend,
                                     const reach_render_command_buffer *commands);
void reach_d2d_release_retained(reach_render_backend *backend);
void reach_d2d_clear_retained_layers(reach_render_backend *backend);
void reach_d2d_invalidate_retained_content(reach_render_backend *backend);
void reach_d2d_release_swap_chain(reach_render_backend *backend);
reach_result reach_d2d_backend_sync_device(reach_render_backend *backend);
reach_result reach_d2d_push_group_masks(reach_render_backend *backend, ID2D1RenderTarget *target,
                                        const reach_render_command_buffer *commands, uint64_t id,
                                        double now_seconds, uint32_t *out_depth);
reach_result reach_d2d_presentation_complete(reach_render_backend *backend, uint64_t generation,
                                             int32_t *out_complete);
void reach_d2d_suspend(reach_render_backend *backend);
reach_result reach_d2d_execute_commands(reach_render_backend *backend,
                                        const reach_render_command *commands, size_t count,
                                        float caret_opacity = 1);
void reach_d2d_set_ui_font(reach_render_backend *backend, int32_t use_bundled_font);
reach_result reach_d2d_measure_text(void *context, const uint16_t *text, float text_size,
                                    int32_t text_weight, float *out_width);
void reach_d2d_release_fonts(reach_render_backend *backend);
D2D1_COLOR_F reach_d2d_color(reach_color color);
void reach_d2d_log_hresult(const wchar_t *context, HRESULT hr);

void reach_d2d_device_surface_activated(reach_render_backend *backend);
void reach_d2d_device_surface_deactivated(reach_render_backend *backend);
void reach_d2d_device_trim_if_idle(reach_render_device *device);
void reach_d2d_device_release_device_resources(reach_render_device *device);
void reach_d2d_device_release_fonts(reach_render_device *device);
void reach_d2d_device_register_removed_event(reach_render_device *device);
void reach_d2d_device_unregister_removed_event(reach_render_device *device);
reach_result reach_d2d_device_check(reach_render_device *device, HRESULT hr,
                                    const wchar_t *operation);
reach_result reach_d2d_device_result(reach_render_device *device, reach_result result,
                                     const wchar_t *operation);

HRESULT reach_d3d11_create_device(D3D_DRIVER_TYPE driver_type, ID3D11Device **out_device,
                                  D3D_FEATURE_LEVEL *out_level);

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
reach_result reach_d2d_synchronize(reach_render_backend *backend);

reach_result reach_d2d_image_size(reach_render_backend *backend, uint64_t id,
                                  D2D1_SIZE_F *out_size);
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
HRESULT reach_d2d_sync_shadow(reach_render_backend *backend, reach_d2d_retained_layer &layer,
                              const reach_render_group &group);
ABI::Windows::UI::Composition::IContainerVisual *
reach_d2d_shadow_wuc_owner(reach_d2d_retained_layer &layer);
IDCompositionVisual2 *reach_d2d_shadow_dcomp_owner(reach_d2d_retained_layer &layer);
reach_result reach_d2d_draw_group_shadow(reach_render_backend *backend,
                                         const reach_render_command_buffer *commands,
                                         const reach_render_group &group, size_t first,
                                         size_t count, float opacity, double now_seconds);
reach_result reach_d2d_execute_fallback(reach_render_backend *backend,
                                        const reach_render_command_buffer *commands,
                                        double now_seconds);
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

int32_t reach_d2d_text_raster_phase(reach_render_backend *backend,
                                    const reach_render_command *command, float *out_y);
reach_result reach_d2d_draw_text(reach_render_backend *backend,
                                 const reach_render_command *command);
reach_result reach_d2d_draw_text_to_target(reach_render_backend *backend, ID2D1RenderTarget *target,
                                           const reach_render_command *command);
reach_result reach_d2d_draw_line(ID2D1RenderTarget *target, const reach_render_command *command);
reach_result reach_d2d_textbox_caret_line(reach_render_backend *backend,
                                          const reach_render_command *command,
                                          reach_render_command *out);
reach_result reach_d2d_draw_textbox(reach_render_backend *backend,
                                    const reach_render_command *command, float caret_opacity = 1);

reach_result reach_d2d_execute(reach_render_backend *backend,
                               const reach_render_command_buffer *commands);

void reach_d2d_destroy(reach_render_backend *backend);

#endif
