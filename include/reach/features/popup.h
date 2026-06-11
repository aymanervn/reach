#ifndef REACH_FEATURES_POPUP_H
#define REACH_FEATURES_POPUP_H

#include "reach/core/render_commands.h"
#include "reach/core/theme.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_popup_background_input
    {
        const reach_theme *theme;
        reach_rect_f32 bounds;
        float notch_center_x;
        float dpi_scale;
    } reach_popup_background_input;

    float reach_popup_radius(void);
    float reach_popup_radius_scaled(float dpi_scale);
    float reach_popup_notch_width(void);
    float reach_popup_notch_width_scaled(float dpi_scale);
    float reach_popup_notch_height(void);
    float reach_popup_notch_height_scaled(float dpi_scale);
    float reach_popup_clamp_notch_center(float notch_center_x, float width);
    float reach_popup_clamp_notch_center_scaled(float notch_center_x, float width,
                                                float dpi_scale);
    reach_result reach_popup_push_background(const reach_popup_background_input *input,
                                             reach_render_command_buffer *out_commands);

#ifdef __cplusplus
}
#endif

#endif
