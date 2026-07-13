#ifndef REACH_FEATURES_CLIPBOARD_H
#define REACH_FEATURES_CLIPBOARD_H

#include "reach/core/render_commands.h"
#include "reach/core/ui_events.h"
#include "reach/features/feature_capsule.h"
#include "reach/core/theme.h"
#include "reach/core/clipboard.h"
#include "reach/core/scrollbar.h"
#include "reach/support/animation.h"
#include "reach/support/util.h"

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
        reach_rect_f32 clear_button;
        reach_rect_f32 viewport;
        reach_rect_f32 items[REACH_CLIPBOARD_MAX_ITEMS];
        reach_rect_f32 close_buttons[REACH_CLIPBOARD_MAX_ITEMS];
        reach_scrollbar_layout scrollbar;
        float content_height;
        float item_large_size;
        float item_width;
    } reach_clipboard_layout;

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
    void reach_clipboard_model_clear_items(reach_clipboard_model *model);
    reach_clipboard_insert_result reach_clipboard_model_insert(reach_clipboard_model *model,
                                                               reach_clipboard_item item);
    int32_t reach_clipboard_model_promote(reach_clipboard_model *model, size_t index);
    void reach_clipboard_model_remove(reach_clipboard_model *model, size_t index);
    reach_clipboard_layout reach_clipboard_compute_layout(reach_clipboard_model *model,
                                                          reach_rect_f32 monitor_bounds,
                                                          reach_rect_f32 launcher_bounds,
                                                          float dpi_scale);
    reach_clipboard_layout reach_clipboard_compute_layout_animated(
        reach_clipboard_model *model, reach_rect_f32 monitor_bounds, reach_rect_f32 launcher_bounds,
        float dpi_scale, float animated_height, float animated_item_width);
    reach_result reach_clipboard_build_render_commands(const reach_clipboard_render_input *input,
                                                       reach_render_command_buffer *commands);

    typedef struct reach_clipboard_feature reach_clipboard_feature;

    reach_result reach_clipboard_feature_create(reach_clipboard_feature **out_clipboard);
    void reach_clipboard_feature_destroy(reach_clipboard_feature *clipboard);

    typedef struct reach_clipboard_state
    {
        reach_clipboard_model model;
        reach_clipboard_layout layout;
        reach_scrollbar_drag scrollbar_drag;
    } reach_clipboard_state;

    const reach_clipboard_state *
    reach_clipboard_feature_state_ptr(reach_clipboard_feature *clipboard);

    const reach_feature_capsule_ops *reach_clipboard_feature_capsule_ops(void);

    int32_t reach_clipboard_feature_relayout(reach_clipboard_feature *clipboard,
                                             reach_rect_f32 monitor_bounds,
                                             reach_rect_f32 launcher_bounds, float dpi_scale,
                                             int32_t *out_animating);

    typedef enum reach_clipboard_pointer_action_kind
    {
        REACH_CLIPBOARD_POINTER_ACTION_NONE = 0,
        REACH_CLIPBOARD_POINTER_ACTION_CLEAR_ALL = 1,
        REACH_CLIPBOARD_POINTER_ACTION_REMOVE_ITEM = 2,
        REACH_CLIPBOARD_POINTER_ACTION_RESTORE_ITEM = 3
    } reach_clipboard_pointer_action_kind;

    int32_t reach_clipboard_is_open(reach_clipboard_feature *clipboard);
    size_t reach_clipboard_item_count(reach_clipboard_feature *clipboard);
    const reach_clipboard_item *reach_clipboard_item_at(reach_clipboard_feature *clipboard,
                                                        size_t index);

    void reach_clipboard_reset_items(reach_clipboard_feature *clipboard);

    void reach_clipboard_clear_all(reach_clipboard_feature *clipboard);

    int32_t reach_clipboard_remove_item(reach_clipboard_feature *clipboard, size_t index,
                                        uint64_t item_id);

    int32_t reach_clipboard_set_open(reach_clipboard_feature *clipboard, int32_t open);

    typedef struct reach_clipboard_insert_outcome
    {
        int32_t accepted;
        reach_clipboard_item release_rejected;
        reach_clipboard_item release_evicted;
    } reach_clipboard_insert_outcome;

    void reach_clipboard_insert_captured(reach_clipboard_feature *clipboard,
                                         reach_clipboard_item item,
                                         reach_clipboard_insert_outcome *out);

    int32_t reach_clipboard_tick_scroll(reach_clipboard_feature *clipboard, double delta_seconds);

    void reach_clipboard_confirm_restore(reach_clipboard_feature *clipboard, size_t index);

    reach_result reach_clipboard_append_render_commands(reach_clipboard_feature *clipboard,
                                                        const reach_theme *theme, float dpi_scale,
                                                        reach_render_command_buffer *out_commands);

    const reach_ui_event_type *reach_clipboard_activation_events(size_t *out_count);

    void reach_clipboard_feature_request_refresh(reach_clipboard_feature *clipboard);
    void reach_clipboard_feature_clear_refresh(reach_clipboard_feature *clipboard);
    int32_t reach_clipboard_feature_take_refresh(reach_clipboard_feature *clipboard);

#ifdef __cplusplus
}
#endif

#endif
