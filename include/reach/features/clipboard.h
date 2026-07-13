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

    /*
     * Clipboard animation capsule. Owns the clipboard's animation manager — the
     * per-item hover tracks plus the height and item-width layout tracks — that used
     * to live in reach_host's shared manager. Composition ticks it once per frame and
     * calls the semantic hover/layout operations; the surface show/hide transition
     * stays composition's policy.
     */
    typedef struct reach_clipboard_feature reach_clipboard_feature;

    reach_result reach_clipboard_feature_create(reach_clipboard_feature **out_clipboard);
    void reach_clipboard_feature_destroy(reach_clipboard_feature *clipboard);

    /*
     * Clipboard internal state (the history model, its computed layout, and the
     * scrollbar drag interaction). Owned by the reach_clipboard_feature capsule;
     * composition reads it via the const reach_clipboard_feature_state_ptr() and
     * mutates only through the semantic ops. The async refresh flag is not part
     * of this struct (it needs atomic access) — drive it through the capsule's
     * refresh functions below.
     */
    typedef struct reach_clipboard_state
    {
        reach_clipboard_model model;
        reach_clipboard_layout layout;
        reach_scrollbar_drag scrollbar_drag;
    } reach_clipboard_state;

    /* The capsule's owned internal state, read-only outside the feature
     * (never null for a live capsule). All writes go through the capsule ops. */
    const reach_clipboard_state *
    reach_clipboard_feature_state_ptr(reach_clipboard_feature *clipboard);

    /* The uniform capsule hooks, including complete pointer-stream ownership. */
    const reach_feature_capsule_ops *reach_clipboard_feature_capsule_ops(void);

    /*
     * Compute + animate + store this frame's layout from the capsule's own model
     * (moved out of composition): computes the target, runs the height/item-width
     * layout tracks, stores the animated layout. Returns 1 when the stored bounds
     * or viewport changed; *out_animating is 1 while a layout track is still
     * running (composition marks layout/surface dirty and requests an update).
     */
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

    /*
     * Const queries (Pattern R, Item 6a): composition reads capsule facts through
     * these instead of reaching into the model.
     */
    int32_t reach_clipboard_is_open(reach_clipboard_feature *clipboard);
    size_t reach_clipboard_item_count(reach_clipboard_feature *clipboard);
    const reach_clipboard_item *reach_clipboard_item_at(reach_clipboard_feature *clipboard,
                                                        size_t index);

    /*
     * Semantic ops (Pattern W, Item 6b). Composition releases item resources
     * through its ports before/after these; the capsule only mutates its model.
     */

    /* Reset the model to empty/initial (after composition released the items). */
    void reach_clipboard_reset_items(reach_clipboard_feature *clipboard);

    /* Clear the history list and settle the scrollbar (clear-all). */
    void reach_clipboard_clear_all(reach_clipboard_feature *clipboard);

    /* Remove the item only when both its index and stable id still match. */
    int32_t reach_clipboard_remove_item(reach_clipboard_feature *clipboard, size_t index,
                                        uint64_t item_id);

    /* Flip open/closed: resets hover/press/drag state. Returns 1 when it changed
     * (composition then runs the surface transition + subscription policy). */
    int32_t reach_clipboard_set_open(reach_clipboard_feature *clipboard, int32_t open);

    /* Insert a captured OS clipboard item (capacity eviction + duplicate
     * promotion + scroll-to-top). Items composition must release come back with
     * non-zero ids; accepted=1 means redraw + relayout + update. */
    typedef struct reach_clipboard_insert_outcome
    {
        int32_t accepted;
        reach_clipboard_item release_rejected;
        reach_clipboard_item release_evicted;
    } reach_clipboard_insert_outcome;

    void reach_clipboard_insert_captured(reach_clipboard_feature *clipboard,
                                         reach_clipboard_item item,
                                         reach_clipboard_insert_outcome *out);

    /* Per-frame smooth-scroll settle. Returns 1 while the offset is animating. */
    int32_t reach_clipboard_tick_scroll(reach_clipboard_feature *clipboard, double delta_seconds);

    /* The restore port call succeeded: promote the item and reset the scroll. */
    void reach_clipboard_confirm_restore(reach_clipboard_feature *clipboard, size_t index);

    /*
     * Render assembly, owned by the clipboard capsule (moved out of composition in
     * the behavior-encapsulation phase): assembles the render input from its own
     * model + layout + hover animations and appends its commands; composition owns
     * the surface frame.
     */
    reach_result reach_clipboard_append_render_commands(reach_clipboard_feature *clipboard,
                                                        const reach_theme *theme, float dpi_scale,
                                                        reach_render_command_buffer *out_commands);

    /* Activation (Level 3): the events that toggle the clipboard surface
     * (the windows key, shared with the launcher). */
    const reach_ui_event_type *reach_clipboard_activation_events(size_t *out_count);

    /* Async clipboard-history refresh flag (set on OS clipboard change, consumed in update). */
    void reach_clipboard_feature_request_refresh(reach_clipboard_feature *clipboard);
    void reach_clipboard_feature_clear_refresh(reach_clipboard_feature *clipboard);
    int32_t reach_clipboard_feature_take_refresh(reach_clipboard_feature *clipboard);

#ifdef __cplusplus
}
#endif

#endif
