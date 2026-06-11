#ifndef REACH_FEATURES_SWITCHER_H
#define REACH_FEATURES_SWITCHER_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/render_commands.h"
#include "reach/ports/icon_provider.h"
#include "reach/core/theme.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_SWITCHER_VISIBLE_MAX 12

    typedef struct reach_switcher_model
    {
        size_t window_count;
        size_t selected_index;
        size_t visible_start;
    } reach_switcher_model;

    typedef struct reach_switcher_render_item
    {
        reach_icon_handle icon;
        uint16_t label[260];
    } reach_switcher_render_item;

    typedef struct reach_switcher_render_input
    {
        const reach_theme *theme;
        reach_rect_f32 bounds;
        const reach_switcher_model *model;
        const reach_switcher_render_item *items;
        size_t item_count;
        float dpi_scale;
        int32_t text_alignment_center;
        int32_t text_weight_demi_bold;
    } reach_switcher_render_input;

    size_t reach_switcher_visible_count(size_t window_count);
    reach_rect_f32 reach_switcher_bounds_for_count(reach_rect_f32 monitor_bounds,
                                                   size_t visible_count);
    reach_rect_f32 reach_switcher_bounds_for_count_scaled(reach_rect_f32 monitor_bounds,
                                                          size_t visible_count,
                                                          float dpi_scale);
    void reach_switcher_update_visible_start(reach_switcher_model *model);
    reach_result reach_switcher_build_render_commands(const reach_switcher_render_input *input,
                                                      reach_render_command_buffer *out_commands);

#ifdef __cplusplus
}
#endif

#endif
