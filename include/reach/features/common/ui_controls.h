#ifndef REACH_FEATURES_COMMON_UI_CONTROLS_H
#define REACH_FEATURES_COMMON_UI_CONTROLS_H

#include "reach/core/render_commands.h"
#include "reach/core/blink.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_ui_button_style
    {
        reach_color background;
        reach_color disabled_background;
        reach_color text;
        reach_color disabled_text;
        reach_color pressed_background;
        reach_color pressed_text;
        int32_t has_pressed_colors;
        float radius;
        float text_size;
        float pressed_darken;
        int32_t text_weight;
    } reach_ui_button_style;

    typedef struct reach_ui_selection_item_style
    {
        reach_color background;
        reach_color accent;
        reach_color text;
        float border_width;
        float text_size;
        int32_t text_weight;
    } reach_ui_selection_item_style;

    typedef struct reach_ui_toggle_style
    {
        reach_color track_off;
        reach_color track_on;
        reach_color knob;
    } reach_ui_toggle_style;

    typedef struct reach_ui_segmented_control_style
    {
        reach_color track;
        reach_color selection;
        reach_color text;
        reach_color selected_text;
        float selection_inset;
        float text_size;
        int32_t text_weight;
    } reach_ui_segmented_control_style;

    typedef struct reach_ui_textbox_state
    {
        const uint16_t *text;
        const uint16_t *placeholder;
        const uint16_t *suffix;
        float suffix_width;
        int32_t text_alignment;
        const reach_blink *caret;
        uint64_t caret_id;
        reach_rect_f32 caret_viewport;
        int32_t caret_index;
        int32_t caret_visible;
        int32_t selection_start;
        int32_t selection_end;
        reach_color text_color;
        reach_color placeholder_color;
        reach_color selection_color;
        reach_color suffix_color;
    } reach_ui_textbox_state;

    void reach_ui_button_render(reach_render_command_buffer *commands, reach_rect_f32 bounds,
                                const uint16_t *label, const reach_ui_button_style *style,
                                int32_t enabled, uint64_t feedback_id, const reach_motion *pressed);

    void reach_ui_selection_item_backdrop_render(reach_render_command_buffer *commands,
                                                 reach_rect_f32 bounds,
                                                 const reach_ui_selection_item_style *style,
                                                 float selection);

    void reach_ui_selection_item_render(reach_render_command_buffer *commands,
                                        reach_rect_f32 bounds, const uint16_t *label,
                                        const reach_ui_selection_item_style *style,
                                        float selection);

    reach_rect_f32 reach_ui_segmented_control_item_bounds(reach_rect_f32 bounds, size_t item_count,
                                                          size_t index);

    int32_t reach_ui_segmented_control_index_at(reach_rect_f32 bounds, size_t item_count, float x,
                                                float y);

    reach_result reach_ui_segmented_control_motion_render(
        reach_render_command_buffer *commands, reach_rect_f32 bounds, const uint16_t *const *labels,
        size_t item_count, const reach_ui_segmented_control_style *style, uint64_t id,
        const reach_motion *position);

    void reach_ui_textbox_render(reach_render_command_buffer *commands, reach_rect_f32 bounds,
                                 const reach_ui_selection_item_style *style, float selection,
                                 const reach_ui_textbox_state *state);

    void reach_ui_toggle_render(reach_render_command_buffer *commands, reach_rect_f32 bounds,
                                const reach_ui_toggle_style *style, float position);
    reach_result reach_ui_toggle_motion_render(reach_render_command_buffer *commands,
                                               reach_rect_f32 bounds,
                                               const reach_ui_toggle_style *style, uint64_t id,
                                               const reach_motion *position);

    reach_result reach_ui_feedback_begin(reach_render_command_buffer *commands, uint64_t id,
                                         reach_rect_f32 bounds, const reach_motion *opacity,
                                         const reach_render_group *placement);
    int32_t reach_ui_color_feedback_begin(reach_render_command_buffer *commands, uint64_t id,
                                          reach_rect_f32 bounds, reach_color from, reach_color to,
                                          const reach_motion *progress);
    reach_result reach_ui_bordered_color_feedback_render(reach_render_command_buffer *commands,
                                                         uint64_t first_id, reach_rect_f32 bounds,
                                                         float radius, float border,
                                                         reach_color from, reach_color to,
                                                         reach_color outline,
                                                         const reach_motion *progress);
    reach_result reach_ui_feedback_render(reach_render_command_buffer *commands, uint64_t id,
                                          reach_rect_f32 bounds, float radius, reach_color color,
                                          const reach_motion *opacity,
                                          const reach_render_group *placement);

#ifdef __cplusplus
}
#endif

#endif
