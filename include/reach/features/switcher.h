#ifndef REACH_FEATURES_SWITCHER_H
#define REACH_FEATURES_SWITCHER_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/render_commands.h"
#include "reach/features/feature_capsule.h"
#include "reach/services/icon_service.h"
#include "reach/services/window_tracking.h"
#include "reach/core/theme.h"
#include "reach/core/pinned_app.h"
#include "reach/core/ui_events.h"
#include "reach/ports/window_manager.h"
#include "reach/ports/icon_provider.h"
#include "reach/support/animation.h"
#include "reach/support/util.h"

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
        uint64_t icon_id;
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
                                                          size_t visible_count, float dpi_scale);
    void reach_switcher_update_visible_start(reach_switcher_model *model);
    reach_result reach_switcher_build_render_commands(const reach_switcher_render_input *input,
                                                      reach_render_command_buffer *out_commands);

    /*
     * Switcher animation capsule. Owns the switcher's width animation (its own
     * reach_animation_manager) — the feature drives its animation, not composition.
     * Composition ticks it once per frame, resets it on open/rebuild, and asks for
     * the animated bounds; the surface show/hide transition stays composition's job.
     */
    /*
     * Switcher internal state (the open window ring the alt-tab UI shows). Owned by
     * the switcher capsule; composition reads it via the const
     * reach_switcher_state_ptr() and mutates only through the semantic ops.
     */
    typedef struct reach_switcher_state
    {
        int32_t open;
        size_t selected_index;
        size_t visible_start;
        uintptr_t windows[REACH_MAX_PINNED_APPS];
        size_t window_count;
    } reach_switcher_state;

    typedef struct reach_switcher reach_switcher;

    reach_result reach_switcher_create(reach_switcher **out_switcher);

    /* The switcher CONSUMES the icon + window-tracking services (read-only:
     * ring windows, foreground, focus history, icons). Attached at wiring;
     * activating the committed window stays a returned action. */
    void reach_switcher_attach_services(reach_switcher *switcher, reach_icon_service *icons,
                                        reach_window_tracking *windows);

    /* The capsule's owned internal state, read-only outside the feature
     * (never null for a live capsule). */
    const reach_switcher_state *reach_switcher_state_ptr(reach_switcher *switcher);

    int32_t reach_switcher_is_open(const reach_switcher *switcher);

    /* The uniform capsule hooks (tick / is_open / force_close / needs_frame). */
    const reach_feature_capsule_ops *reach_switcher_capsule_ops(void);

    /* Activation (Level 3): the alt-tab modal event stream routed whole to the
     * switcher's host handler. */
    const reach_ui_event_type *reach_switcher_routed_events(size_t *out_count);
    void reach_switcher_destroy(reach_switcher *switcher);

    /* Advance the switcher's own animation manager. */
    void reach_switcher_tick(reach_switcher *switcher, double delta_seconds);

    /* Reset the width animation (on open / rebuild / close). */
    void reach_switcher_reset_width_animation(reach_switcher *switcher);

    int32_t reach_switcher_width_animation_active(const reach_switcher *switcher);

    /*
     * Animate the surface width toward target as the visible window count changes,
     * returning the animated bounds (centered on target). transition_visible/open
     * come from composition's surface + switcher state; bounds_valid/last_bounds_width
     * come from the switcher surface runtime. Sets *out_request_redraw to 1 while the
     * animation is running so the caller marks the surface dirty.
     */
    reach_rect_f32 reach_switcher_apply_width_animation(reach_switcher *switcher,
                                                        int32_t transition_visible, int32_t open,
                                                        int32_t bounds_valid,
                                                        float last_bounds_width,
                                                        reach_rect_f32 target,
                                                        int32_t *out_request_redraw);

    /*
     * Behavior contract (composition stays wiring-only).
     *
     * The switcher owns its alt-tab interaction, ring rebuild, selection, visible-start,
     * and render-input assembly. handle_event/sync_windows mutate the capsule's own state
     * and return a neutral action; composition translates the action into surface
     * visibility + the window-control port call. The capsule never calls a port and never
     * touches another feature.
     */
    typedef enum reach_switcher_action_type
    {
        REACH_SWITCHER_ACTION_NONE = 0,
        REACH_SWITCHER_ACTION_OPENED = 1,    /* surface should show */
        REACH_SWITCHER_ACTION_CHANGED = 2,   /* selection/ring changed; redraw */
        REACH_SWITCHER_ACTION_CLOSED = 3,    /* hide; no window activation */
        REACH_SWITCHER_ACTION_COMMITTED = 4  /* hide + activate action.window */
    } reach_switcher_action_type;

    typedef struct reach_switcher_action
    {
        reach_switcher_action_type type;
        uintptr_t window; /* COMMITTED: window to activate (0 = none) */
    } reach_switcher_action;

    /*
     * Render context. Windows + icons come from the attached services;
     * icon_size_px matches the dock's so gets share the cache.
     */
    typedef struct reach_switcher_render_context
    {
        const reach_theme *theme;
        reach_rect_f32 bounds;
        float dpi_scale;
        int32_t icon_size_px;
    } reach_switcher_render_context;

    /* Force-close without committing (game mode / shutdown): closes the ring and
     * resets the selection + scroll. Composition applies its surface policy. */
    void reach_switcher_force_close(reach_switcher *switcher);

    /* Alt-tab key/gesture events (ring source: the attached window service).
     * Returns the semantic outcome for composition to apply. */
    reach_switcher_action reach_switcher_handle_event(reach_switcher *switcher,
                                                      const reach_ui_event *event);

    /* Re-sync the ring with the current windows while the switcher is open (windows
     * opened/closed underneath it). Preserves the selection where possible. */
    reach_switcher_action reach_switcher_sync_windows(reach_switcher *switcher);

    /* Assemble the alt-tab strip (icons + labels from borrowed data) and append its draw
     * commands. Switcher owns a solo surface, so the buffer is built fresh. */
    reach_result reach_switcher_append_render_commands(reach_switcher *switcher,
                                                       const reach_switcher_render_context *ctx,
                                                       reach_render_command_buffer *out_commands);

#ifdef __cplusplus
}
#endif

#endif
