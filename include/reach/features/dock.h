#ifndef REACH_FEATURES_DOCK_H
#define REACH_FEATURES_DOCK_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/menu_commands.h"
#include "reach/core/ui_state.h"
#include "reach/core/render_commands.h"
#include "reach/ports/icon_provider.h"
#include "reach/ports/window_manager.h"
#include "reach/core/theme.h"
#include "reach/features/feature_capsule.h"
#include "reach/services/icon_service.h"
#include "reach/services/now_playing.h"
#include "reach/services/window_tracking.h"
#include "reach/support/animation.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef int32_t (*reach_dock_window_matches_pinned_fn)(void *user,
                                                           const reach_pinned_app_model *pinned_app,
                                                           const reach_window_snapshot *window);

    typedef struct reach_dock_order_key
    {
        int32_t pinned;
        uint32_t pin_id;
        uintptr_t window;
    } reach_dock_order_key;

    typedef struct reach_dock_item_model
    {
        int32_t pinned;
        uint32_t pin_id;
        uintptr_t window;
        size_t pinned_index;
        size_t open_index;
    } reach_dock_item_model;

    typedef struct reach_dock_feature_model
    {
        reach_dock_item_model items[REACH_MAX_PINNED_APPS];
        size_t item_count;
        reach_dock_order_key order[REACH_MAX_PINNED_APPS];
        size_t order_count;
    } reach_dock_feature_model;

    void reach_dock_feature_model_init(reach_dock_feature_model *model);
    int32_t reach_dock_key_equal(const reach_dock_order_key *a, const reach_dock_order_key *b);
    uint32_t reach_dock_feature_model_item_pin_id(const reach_dock_feature_model *model,
                                                  size_t index);
    int32_t reach_dock_feature_model_item_matches_key(const reach_dock_feature_model *model,
                                                      size_t index, reach_dock_order_key key);
    size_t reach_dock_feature_model_find_item_key(const reach_dock_feature_model *model,
                                                  reach_dock_order_key key);
    size_t reach_dock_feature_model_find_order_key(const reach_dock_feature_model *model,
                                                   reach_dock_order_key key);
    void reach_dock_feature_model_move_order(reach_dock_feature_model *model, size_t source,
                                             size_t target);
    size_t reach_dock_feature_model_pinned_order_index(const reach_dock_feature_model *model,
                                                       uint32_t pin_id);
    size_t
    reach_dock_find_pinned_for_window(const reach_pinned_app_model *pinned_apps,
                                      size_t pinned_app_count, const reach_window_snapshot *window,
                                      reach_dock_window_matches_pinned_fn window_matches_pinned,
                                      void *match_user);
    void reach_dock_feature_model_build_items(
        reach_dock_feature_model *model, const reach_pinned_app_model *pinned_apps,
        size_t pinned_app_count, const reach_window_snapshot *open_windows,
        size_t open_window_count, reach_dock_window_matches_pinned_fn window_matches_pinned,
        void *match_user);
    typedef struct reach_dock_render_item
    {
        reach_icon_handle icon;
        uint16_t fallback_initial;
    } reach_dock_render_item;

    typedef struct reach_dock_render_input
    {
        const reach_theme *theme;
        const reach_dock_layout *layout;
        const reach_dock_feature_model *model;
        const reach_dock_render_item *render_items;
        size_t render_item_count;
        const float *item_box_x;
        size_t item_box_x_count;
        /* Per-item reveal (0..1) from the item's slot progress: 0 draws
           nothing (a growing slot holds position only), <1 scales + fades. */
        const float *item_reveal;
        size_t item_reveal_count;
        uintptr_t focused_window;
        size_t dragged_render_index;
        float dragged_box_x;
        size_t click_feedback_index;
        float click_feedback_opacity;
        size_t tray_feedback_index;
        size_t quick_settings_feedback_index;
        size_t power_feedback_index;
        const uint16_t *time_text;
        const uint16_t *date_text;
        int32_t text_alignment_center;
    } reach_dock_render_input;

    reach_result reach_dock_build_render_commands(const reach_dock_render_input *input,
                                                  reach_render_command_buffer *out_commands);

    typedef enum reach_dock_pointer_region
    {
        REACH_DOCK_POINTER_REGION_NONE = 0,
        REACH_DOCK_POINTER_REGION_ITEM = 1,
        REACH_DOCK_POINTER_REGION_TRAY_BUTTON = 2,
        REACH_DOCK_POINTER_REGION_QUICK_SETTINGS_BUTTON = 3,
        REACH_DOCK_POINTER_REGION_POWER_BUTTON = 4
    } reach_dock_pointer_region;

    typedef enum reach_dock_pointer_action_kind
    {
        REACH_DOCK_POINTER_ACTION_NONE = 0,
        REACH_DOCK_POINTER_ACTION_PRESS_NOW_PLAYING = 1,
        REACH_DOCK_POINTER_ACTION_PRESS_ITEM = 2,
        REACH_DOCK_POINTER_ACTION_PRESS_TRAY = 3,
        REACH_DOCK_POINTER_ACTION_PRESS_QUICK_SETTINGS = 4,
        REACH_DOCK_POINTER_ACTION_PRESS_POWER = 5,
        REACH_DOCK_POINTER_ACTION_LAUNCH_PINNED = 6,
        REACH_DOCK_POINTER_ACTION_FOCUS_WINDOW = 7,
        REACH_DOCK_POINTER_ACTION_LAUNCH_NEW_INSTANCE = 8,
        REACH_DOCK_POINTER_ACTION_SHOW_ITEM_CONTEXT = 9,
        REACH_DOCK_POINTER_ACTION_TOGGLE_TRAY = 10,
        REACH_DOCK_POINTER_ACTION_TOGGLE_QUICK_SETTINGS = 11,
        REACH_DOCK_POINTER_ACTION_TOGGLE_POWER = 12,
        REACH_DOCK_POINTER_ACTION_MEDIA_PREVIOUS = 13,
        REACH_DOCK_POINTER_ACTION_MEDIA_PLAY_PAUSE = 14,
        REACH_DOCK_POINTER_ACTION_MEDIA_NEXT = 15,
        REACH_DOCK_POINTER_ACTION_REBUILD_ITEMS = 16,
        REACH_DOCK_POINTER_ACTION_MOVE_PIN = 17
    } reach_dock_pointer_action_kind;

    size_t reach_dock_reorder_target(const reach_dock_feature_model *model,
                                     const reach_dock_layout *layout, size_t current_index,
                                     float dragged_box_x);

    /*
     * Dock animation capsule. Owns the dock's reach_animation_manager (Y, width,
     * drag-snap, click-feedback opacity, and the per-item X reorder tracks) — the
     * dock's animation state now lives in the feature, not in reach_host's shared
     * manager. Composition ticks it once per frame and drives it through the manager
     * returned by reach_dock_manager(), using the dock-local track ids
     * below. (Tray feedback + surface show/hide transitions stay in composition.)
     */
    /* Slot pool: one width track per slot. Slot 0 is reserved for the Now
       Playing widget; app slots follow; +1 headroom so a burst of closes can
       still animate (overflow snaps the oldest dying slot). */
#define REACH_DOCK_SLOT_CAPACITY (REACH_MAX_PINNED_APPS + 2)

    enum reach_dock_animation_id
    {
        REACH_DOCK_ANIM_Y = 0,
        REACH_DOCK_ANIM_DRAG_SNAP,
        REACH_DOCK_ANIM_FEEDBACK_OPACITY,
        REACH_DOCK_ANIM_NOW_PLAYING_CONTENT,
        REACH_DOCK_ANIM_ITEM_X_BASE,
        REACH_DOCK_ANIM_SLOT_BASE = REACH_DOCK_ANIM_ITEM_X_BASE + REACH_MAX_PINNED_APPS,
        REACH_DOCK_ANIM_COUNT = REACH_DOCK_ANIM_SLOT_BASE + REACH_DOCK_SLOT_CAPACITY
    };

    static inline size_t reach_dock_item_animation_id(size_t index)
    {
        return REACH_DOCK_ANIM_ITEM_X_BASE + index;
    }

    typedef struct reach_dock reach_dock;

    reach_result reach_dock_create(reach_dock **out_animations);
    void reach_dock_destroy(reach_dock *animations);

    /* The dock CONSUMES the icon + window-tracking services (read-only; window
     * control stays behind composition's translators). Attached at wiring. */
    void reach_dock_attach_services(reach_dock *dock, reach_icon_service *icons,
                                    reach_window_tracking *windows,
                                    reach_now_playing_service *now_playing);

    /*
     * Tick: advances the dock's animation manager AND settles the feedback
     * opacity (drained -> clear feedback slot), copies the drag-snap value into
     * drag.x while it runs, and resets the drag keys when the snap finishes —
     * the settle logic moved out of composition in the class-driven interface
     * phase. out->redraw asks for a dock surface redraw (feedback / item-x /
     * drag-snap / width activity).
     */
    reach_animation_manager *reach_dock_manager(reach_dock *animations);

    /* The uniform capsule hooks, including complete pointer-stream ownership;
     * the dock surface is persistent (is_open = always). */
    const reach_feature_capsule_ops *reach_dock_capsule_ops(void);

    /*
     * Semantic state ops (the dock keeps a mutable reach_dock_state_ptr for the
     * item-x reorder bookkeeping in composition until that algorithm moves with
     * the width-animation work; these ops cover the natural behavior writes).
     */
    /* items_changed: request a rebuild-with-animations on the next frame /
     * consume the request (returns the previous value). */
    void reach_dock_mark_items_changed(reach_dock *dock);
    int32_t reach_dock_take_items_changed(reach_dock *dock);
    /* Dock-surface pointer sequence query for composition capture policy. */
    int32_t reach_dock_pointer_sequence_active(const reach_dock *dock);
    /* Semantic region under a Dock-local point. The capsule retains row/index
     * hit details; composition uses only this cross-popup policy fact. */
    reach_dock_pointer_region reach_dock_pointer_region_at(const reach_dock *dock,
                                                           int32_t local_x, int32_t local_y);
    /* Auto-hide reveal session (edge-enter opens one; game mode drops it). */
    void reach_dock_begin_reveal_session(reach_dock *dock);

    /*
     * Item + geometry build (moved out of composition with the Phase-2 layout
     * work): rebuilds the item model from the borrowed pinned/open-window data,
     * derives the dock width (including its private Now Playing subfeature),
     * and positions every sub-rect.
     */
    typedef struct reach_dock_build_context
    {
        const reach_theme *theme;
        float dpi_scale;
        float icon_size; /* unscaled ui config */
        float gap;       /* unscaled ui config */
        const reach_pinned_app_model *pinned_apps; /* composition-owned, borrowed */
        size_t pinned_app_count;
    } reach_dock_build_context;

    void reach_dock_build_layout(reach_dock *dock, const reach_dock_build_context *ctx,
                                 reach_dock_layout *layout);

    /*
     * Dock-local <-> screen geometry (pure; moved out of composition with the
     * composition-shrink phase). The layout's sub-rects are dock-local; bounds
     * is the screen-space dock window rect.
     */
    reach_point_i32 reach_dock_local_point(const reach_dock_layout *layout, int32_t x, int32_t y);
    reach_rect_f32 reach_dock_rect_to_screen(const reach_dock_layout *layout, reach_rect_f32 rect);
    reach_dock_layout reach_dock_layout_to_screen(reach_dock_layout layout);

    /*
     * One-op animated rebuild: snapshots the item x positions (from old_layout;
     * out_layout when old_layout is null), rebuilds the item model, diffs the
     * slot pool (born slots grow from 0, dead slots collapse and linger until
     * they land, a key reappearing mid-collapse revives its slot), derives the
     * geometry from the animated slot widths, and rebinds surviving items so
     * reorders glide (offsets that decay onto the moving slot positions).
     * old_layout may alias out_layout — the snapshot is taken first.
     */
    void reach_dock_rebuild_items(reach_dock *dock, const reach_dock_build_context *ctx,
                                  const reach_dock_layout *old_layout,
                                  reach_dock_layout *out_layout);

    /*
     * Slot activity (any slot width animating). The dock's geometry derives
     * from the animated slot widths inside reach_dock_build_layout, so this is
     * the "dock geometry is in motion" fact composition's fast paths need;
     * item/now-playing hit-testing is suppressed inside the capsule while it
     * is true.
     */
    int32_t reach_dock_slots_animating(const reach_dock *dock);

    /* Reset the icon-motion offsets and snap the slot pool (game mode /
       lifecycle resets; the reorder bookkeeping itself is feature-internal). */
    void reach_dock_clear_item_x_animations(reach_dock *dock);

    /*
     * Dock internal state (the pinned/open item model the dock displays). Owned by
     * the dock capsule (the same opaque object that owns the dock's animation
     * manager); composition reads it via the const reach_dock_state_ptr() and
     * mutates only through the semantic ops. The dock's reveal/visibility drive
     * and async icon orchestration stay in composition.
     */
    /*
     * Dock click-feedback slot indices. The dock renders the pinned app slots
     * [0, REACH_MAX_PINNED_APPS) plus the tray / quick-settings / power buttons; the
     * feedback index addresses any of them, with _NONE as the empty sentinel.
     */
    enum
    {
        REACH_DOCK_FEEDBACK_TRAY_BUTTON = REACH_MAX_PINNED_APPS,
        REACH_DOCK_FEEDBACK_QUICK_SETTINGS_BUTTON,
        REACH_DOCK_FEEDBACK_POWER_BUTTON,
        REACH_DOCK_FEEDBACK_NONE
    };

    /*
     * Click-feedback state is owned by the Dock pointer handler. Composition only
     * acknowledges cross-feature popup policy below.
     */
    /* Power-button release suppression: arm when the press already dismissed
     * the power menu. Composition calls this semantic acknowledgement after
     * applying the cross-feature close policy. */
    void reach_dock_suppress_power_release(reach_dock *dock);
    /* Popup policy acknowledgements keep sticky feedback feature-owned. */
    int32_t reach_dock_feedback_stick(reach_dock *dock);
    int32_t reach_dock_feedback_clear_sticky(reach_dock *dock);

    /*
     * Refresh the dock clock text (12h time + "Mon D, Day" date) from the system
     * clock, guarded to once per minute. Owned by the dock capsule (moved out of
     * composition in the behavior-encapsulation phase). Returns 1 when the text
     * changed and the dock needs a redraw.
     */
    int32_t reach_dock_update_clock(reach_dock *dock);

    /*
     * Render assembly, owned by the dock capsule (moved out of composition in the
     * behavior-encapsulation phase). The capsule assembles its render input from
     * its own state (drag, reorder animations, feedback, clock) plus the borrowed
     * cross-cutting context below and appends its commands; composition owns the
     * surface frame (begin/execute/end). Now Playing is a private dock
     * subfeature and appends into the same buffer inside this operation.
     */
    typedef struct reach_dock_render_context
    {
        const reach_theme *theme;
        const reach_dock_layout *layout;
        uintptr_t focused_window;                  /* from the window-manager port (borrowed) */
        const reach_pinned_app_model *pinned_apps; /* composition-owned, borrowed */
        size_t pinned_app_count;
        int32_t icon_size_px; /* app-icon pixel size (composition derives from theme+dpi) */
        float dpi_scale;
        float dock_gap;
    } reach_dock_render_context;

    reach_result reach_dock_append_render_commands(reach_dock *dock,
                                                   const reach_dock_render_context *ctx,
                                                   reach_render_command_buffer *out_commands);

    /*
     * Dock drag-reorder interaction state. Owned by the dock capsule (moved out of
     * reach_host in the behavior-encapsulation phase). source_index/target_index use
     * REACH_MAX_PINNED_APPS as the "none" sentinel.
     */
    typedef struct reach_dock_drag_state
    {
        int32_t active;
        int32_t moved;
        size_t source_index;
        size_t target_index;
        int32_t pinned;
        uint32_t pin_id;
        uintptr_t window;
        int32_t start_x;
        int32_t start_y;
        float grab_offset_x;
        float x;
    } reach_dock_drag_state;

    typedef struct reach_dock_state
    {
        reach_dock_feature_model model;
        /* Auto-hide reveal state machine (moved out of reach_host in F). The
         * subscription flags + reveal-edge surface state stay in composition. */
        int32_t target_hidden;
        int32_t reveal_session_active;
        int32_t pointer_sequence_active;
        int32_t dock_animation_initialized;
        /* Drag-reorder interaction + the pressed dock slot (REACH_MAX_PINNED_APPS =
         * none). Moved out of reach_host in the behavior-encapsulation phase. */
        reach_dock_drag_state drag;
        size_t pressed_index;
        int32_t pressed_control;
        /* Right-click on the power button closes the open power menu and eats
         * the following release (moved off reach_host). */
        int32_t power_release_suppressed;
        /* Click-feedback (press/stick/release) for the dock slots + buttons.
         * feedback_index uses REACH_DOCK_FEEDBACK_NONE as the empty sentinel. The
         * feedback opacity itself lives in the dock animation manager. */
        size_t feedback_index;
        int32_t feedback_pressed;
        int32_t feedback_sticky;
        /* Per-item X reorder-animation bookkeeping (the from/to keys the dock's
         * REACH_DOCK_ANIM_ITEM_X_* tracks animate between). Moved out of reach_host
         * in the behavior-encapsulation phase; item_x_valid[] gates a live track.
         * items_changed requests a rebuild-with-animations on the next frame. */
        int32_t item_x_valid[REACH_MAX_PINNED_APPS];
        int32_t item_x_pinned[REACH_MAX_PINNED_APPS];
        uint32_t item_x_pin_ids[REACH_MAX_PINNED_APPS];
        uintptr_t item_x_windows[REACH_MAX_PINNED_APPS];
        int32_t items_changed;
        /* Dock clock text + the once-per-minute refresh guard (owned by the
         * capsule; refreshed through reach_dock_update_clock). */
        uint16_t clock_time_text[32];
        uint16_t clock_date_text[64];
        int32_t clock_initialized;
        int64_t clock_last_minute;
    } reach_dock_state;

    /* The capsule's owned internal state, read-only outside the feature
     * (never null for a live capsule). All writes go through the semantic ops. */
    const reach_dock_state *reach_dock_state_ptr(reach_dock *animations);

    /*
     * Const queries (Pattern R, Item 6a): composition reads capsule facts through
     * these instead of reaching into the model. item_at returns null when out of
     * range.
     */
    size_t reach_dock_item_count(reach_dock *dock);
    const reach_dock_item_model *reach_dock_item_at(reach_dock *dock, size_t index);

    /*
     * Context-menu command assembly for a dock item. The dock owns the item
     * capabilities (pin state, launch path from its borrowed pinned apps or the
     * window service, open window) and emits the gated command list; the
     * context_menu capsule displays it and composition executes the choice.
     * Returns the number of commands written (capped at cap).
     */
    size_t reach_dock_build_item_context_commands(reach_dock *dock, size_t item_index,
                                                  uint32_t *out_commands, size_t cap);

    /*
     * Order snapshot/restore across a pin-list reload (Pattern W, Item 6e).
     * Composition snapshots the keys, remaps pinned ids across the reload via its
     * pin paths, and hands the keys back.
     */
    size_t reach_dock_order_count(reach_dock *dock);
    reach_dock_order_key reach_dock_order_key_at(reach_dock *dock, size_t index);
    void reach_dock_restore_order(reach_dock *dock, const reach_dock_order_key *keys,
                                  size_t count);

    /*
     * Dock auto-hide visibility. The dock capsule owns the reveal state machine +
     * the Y animation; composition computes the cross-component policy inputs
     * (game mode, can-hide, transient-open, pointer) and applies the result
     * (reveal-edge surface, pointer-move subscriptions, ui.dock.visible). The raw
     * policy inputs are passed through (not collapsed) so the decision arithmetic
     * stays a verbatim relocation.
     */
    typedef enum reach_dock_reveal_edge_mode
    {
        REACH_DOCK_REVEAL_EDGE_DISABLED = 0,
        REACH_DOCK_REVEAL_EDGE_THIN = 1,
        REACH_DOCK_REVEAL_EDGE_BRIDGE = 2
    } reach_dock_reveal_edge_mode;

    typedef struct reach_dock_visibility_request
    {
        reach_rect_f32 shown_bounds;
        reach_rect_f32 monitor_bounds;
        reach_point_i32 pointer;
        int32_t pointer_valid;
        int32_t game_mode;
        int32_t can_hide;
        int32_t transient_open;
        int32_t dock_sticky_feedback;
    } reach_dock_visibility_request;

    typedef struct reach_dock_visibility_result
    {
        reach_rect_f32 animated_bounds;
        int32_t edge_mode; /* reach_dock_reveal_edge_mode */
        int32_t visible;
        int32_t clear_sticky_feedback;
    } reach_dock_visibility_result;

    reach_rect_f32 reach_dock_reveal_edge_bounds(int32_t mode, reach_rect_f32 shown_dock_bounds,
                                                 reach_rect_f32 monitor_bounds);
    reach_dock_visibility_result
    reach_dock_update_visibility(reach_dock *animations,
                                 const reach_dock_visibility_request *request);

#ifdef __cplusplus
}
#endif

#endif
