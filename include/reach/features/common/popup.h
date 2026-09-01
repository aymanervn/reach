#ifndef REACH_FEATURES_COMMON_POPUP_H
#define REACH_FEATURES_COMMON_POPUP_H

#include "reach/core/render_commands.h"
#include "reach/core/theme.h"
#include "reach/features/common/presentation.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_popup_drop_direction
    {
        REACH_POPUP_DROP_UP = 0,
        REACH_POPUP_DROP_DOWN = 1
    } reach_popup_drop_direction;

    typedef struct reach_popup_anchor
    {
        reach_rect_f32 button;
        reach_rect_f32 monitor;
        float bar_edge_y;
        float bar_height;
        int32_t direction;
    } reach_popup_anchor;

    typedef struct reach_popup_placement
    {
        reach_rect_f32 bounds;
        float notch_anchor_x;
        int32_t notch_side;
    } reach_popup_placement;

    int32_t reach_popup_notch_side(int32_t direction);

    reach_popup_placement reach_popup_place(const reach_popup_anchor *anchor, float width,
                                            float height, float margin);

    typedef struct reach_popup_background_input
    {
        const reach_theme *theme;
        reach_rect_f32 bounds;
        float notch_center_x;
        int32_t notch_side;
        float dpi_scale;
        reach_color background_color_override;
        int32_t has_background_color_override;
    } reach_popup_background_input;

    float reach_popup_radius(const reach_theme *theme);
    float reach_popup_radius_scaled(const reach_theme *theme, float dpi_scale);
    float reach_popup_notch_width(void);
    float reach_popup_notch_width_scaled(float dpi_scale);
    float reach_popup_notch_height(void);
    float reach_popup_notch_height_scaled(float dpi_scale);
    float reach_popup_clamp_notch_center_scaled(float notch_center_x, float width, float dpi_scale);
    reach_result reach_popup_push_background(const reach_popup_background_input *input,
                                             reach_render_command_buffer *out_commands);

#ifdef __cplusplus
}
#endif

#endif
