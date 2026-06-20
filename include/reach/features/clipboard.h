#ifndef REACH_FEATURES_CLIPBOARD_H
#define REACH_FEATURES_CLIPBOARD_H

#include "reach/core/render_commands.h"
#include "reach/core/theme.h"
#include "reach/core/clipboard.h"
#include "reach/core/scrollbar.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_CLIPBOARD_MAX_ITEMS 20
    typedef struct reach_clipboard_model
    {
        int32_t open;
        reach_clipboard_item items[REACH_CLIPBOARD_MAX_ITEMS];
        size_t count;
        size_t hovered_index;
        size_t pressed_index;
        int32_t pressed_hit_type;
        uint64_t pressed_item_id;
        reach_scrollbar_model scrollbar;
    } reach_clipboard_model;

    typedef struct reach_clipboard_insert_result
    {
        uint64_t evicted_id;
        uint64_t rejected_id;
        int32_t inserted;
        int32_t promoted_existing;
    } reach_clipboard_insert_result;

    typedef struct reach_clipboard_layout
    {
        reach_rect_f32 bounds;
        reach_rect_f32 title;
        reach_rect_f32 viewport;
        reach_rect_f32 items[REACH_CLIPBOARD_MAX_ITEMS];
        reach_rect_f32 close_buttons[REACH_CLIPBOARD_MAX_ITEMS];
        reach_scrollbar_layout scrollbar;
        float content_height;
        float row_height;
    } reach_clipboard_layout;

    typedef enum reach_clipboard_hit_type
    {
        REACH_CLIPBOARD_HIT_NONE = 0,
        REACH_CLIPBOARD_HIT_ITEM = 1,
        REACH_CLIPBOARD_HIT_ITEM_CLOSE = 4,
        REACH_CLIPBOARD_HIT_SCROLLBAR_TRACK = 2,
        REACH_CLIPBOARD_HIT_SCROLLBAR_THUMB = 3
    } reach_clipboard_hit_type;

    typedef struct reach_clipboard_hit_result
    {
        reach_clipboard_hit_type type;
        size_t index;
    } reach_clipboard_hit_result;

    typedef struct reach_clipboard_render_input
    {
        const reach_theme *theme;
        const reach_clipboard_model *model;
        const reach_clipboard_layout *layout;
        const float *hover_values;
        float dpi_scale;
        int32_t text_alignment_leading;
    } reach_clipboard_render_input;

    void reach_clipboard_model_init(reach_clipboard_model *model);
    void reach_clipboard_model_clear_press(reach_clipboard_model *model);
    reach_clipboard_insert_result reach_clipboard_model_insert(reach_clipboard_model *model,
                                                               reach_clipboard_item item);
    int32_t reach_clipboard_model_promote(reach_clipboard_model *model, size_t index);
    void reach_clipboard_model_remove(reach_clipboard_model *model, size_t index);
    reach_clipboard_layout reach_clipboard_compute_layout(reach_clipboard_model *model,
                                                          reach_rect_f32 monitor_bounds,
                                                          reach_rect_f32 launcher_bounds,
                                                          float dpi_scale);
    reach_clipboard_hit_result reach_clipboard_hit_test(const reach_clipboard_model *model,
                                                        const reach_clipboard_layout *layout,
                                                        int32_t x, int32_t y);
    reach_result reach_clipboard_build_render_commands(const reach_clipboard_render_input *input,
                                                       reach_render_command_buffer *commands);

#ifdef __cplusplus
}
#endif

#endif
