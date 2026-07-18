#ifndef REACH_FEATURES_COMMON_UI_CONTROLS_H
#define REACH_FEATURES_COMMON_UI_CONTROLS_H

#include "reach/core/render_commands.h"

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
        float radius;
        float text_size;
        int32_t text_weight;
    } reach_ui_button_style;

    typedef struct reach_ui_selection_item_style
    {
        reach_color background;
        reach_color accent;
        reach_color text;
        float stroke_width;
        float text_size;
        int32_t text_weight;
    } reach_ui_selection_item_style;

    typedef struct reach_ui_toggle_style
    {
        reach_color track_off;
        reach_color track_on;
        reach_color knob;
    } reach_ui_toggle_style;

    typedef struct reach_ui_textbox_state
    {
        const uint16_t *text;
        const uint16_t *placeholder;
        const uint16_t *suffix;
        float suffix_width;
        int32_t text_alignment;
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
                                int32_t enabled, float pressed);

    void reach_ui_selection_item_backdrop_render(reach_render_command_buffer *commands,
                                                 reach_rect_f32 bounds,
                                                 const reach_ui_selection_item_style *style,
                                                 float selection);

    void reach_ui_selection_item_render(reach_render_command_buffer *commands,
                                        reach_rect_f32 bounds, const uint16_t *label,
                                        const reach_ui_selection_item_style *style,
                                        float selection);

    void reach_ui_textbox_render(reach_render_command_buffer *commands, reach_rect_f32 bounds,
                                 const reach_ui_selection_item_style *style, float selection,
                                 const reach_ui_textbox_state *state);

    void reach_ui_toggle_render(reach_render_command_buffer *commands, reach_rect_f32 bounds,
                                const reach_ui_toggle_style *style, float position);

#ifdef __cplusplus
}
#endif

#endif
