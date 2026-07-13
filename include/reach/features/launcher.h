#ifndef REACH_FEATURES_LAUNCHER_H
#define REACH_FEATURES_LAUNCHER_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/render_commands.h"
#include "reach/core/theme.h"
#include "reach/core/ui_events.h"
#include "reach/features/common/text_edit.h"
#include "reach/features/feature_capsule.h"
#include "reach/services/icon_service.h"
#include "reach/services/search.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * The launcher capsule owns the launcher model + interaction state (both left
     * the shared core reach_ui_state in E). reach_launcher_model itself stays a core
     * data type so core layout (reach_launcher_layout_compute) can name it.
     * Composition reads the state via the const reach_launcher_state_ptr(capsule)
     * and mutates only through the capsule ops below (the former
     * reach_ui_state_*launcher* API, re-homed onto the capsule). The full struct
     * is defined below, after the hit-type it references.
     */
    typedef struct reach_launcher_state reach_launcher_state;

    typedef struct reach_launcher reach_launcher;

    reach_result reach_launcher_create(reach_launcher **out_launcher);
    void reach_launcher_destroy(reach_launcher *launcher);

    /* The capsule's owned internal state, read-only outside the feature
     * (never null for a live capsule). All writes go through the ops below. */
    const reach_launcher_state *reach_launcher_state_ptr(reach_launcher *launcher);

    /* The uniform capsule hooks, including complete pointer-stream ownership.
     * force_close stays null: closing the launcher runs composition's
     * focus-restore + worker-cancel policy. */
    const reach_feature_capsule_ops *reach_launcher_capsule_ops(void);

    /* Refresh the copied layout and borrowed pinned-app data used by the
     * uniform pointer hook after composition computes a layout pass. */
    void reach_launcher_set_pointer_context(reach_launcher *launcher,
                                            const reach_launcher_layout *layout,
                                            const reach_pinned_app_model *pinned_apps,
                                            size_t pinned_app_count);

    /* Model ops, capsule-targeted (retargeted from reach_launcher_state * in
     * the class-driven interface phase; the state versions are internal). */
    reach_result reach_launcher_close(reach_launcher *launcher);
    reach_result reach_launcher_toggle(reach_launcher *launcher);
    reach_result reach_launcher_set_query(reach_launcher *launcher, const uint16_t *query);
    reach_result reach_launcher_set_results(reach_launcher *launcher,
                                            const reach_search_candidate *results, size_t count);
    reach_result reach_launcher_clear_results(reach_launcher *launcher);
    size_t reach_launcher_model_result_scroll_offset(const reach_launcher_model *launcher);

    /*
     * Search-box caret + focus + interaction resets (moved out of composition).
     * tick_caret advances the blink while open (returns 1 = redraw) and resets
     * it while closed. set_focused notes composition's show/hide of the surface.
     */
    /*
     * Search (the launcher CONSUMES the search service — features may use
     * services; ports stay behind composition's translators). Composition
     * attaches the service at wiring time; the capsule owns the latest-wins
     * generation: submit runs the current query, cancel invalidates, take
     * returns 1 with a completed result set that is still current (matching
     * generation, launcher still open). Applying the results to the model —
     * and the result-icon slot migration around it — stays with the caller.
     */
    void reach_launcher_attach_search(reach_launcher *launcher, reach_search_service *search);
    /* Result icons pull from the icon service at render time. */
    void reach_launcher_attach_icons(reach_launcher *launcher, reach_icon_service *icons);
    void reach_launcher_cancel_search(reach_launcher *launcher);
    int32_t reach_launcher_take_search_results(reach_launcher *launcher,
                                               reach_search_candidate *out_results,
                                               size_t *out_count);

    /* Activation (Level 3): the events that toggle the launcher surface. The
     * toggle itself runs through composition's host hook (it remembers the
     * focus-restore window); WINDOWS_KEY is no longer handled inside
     * reach_launcher_handle_event. */
    const reach_ui_event_type *reach_launcher_activation_events(size_t *out_count);

    void reach_launcher_clear_pressed(reach_launcher *launcher);
    void reach_launcher_set_focused(reach_launcher *launcher, int32_t focused);
    /* Focus-restore bookkeeping: remember the foreground window at open (0
     * clears), take consumes it (returns 0 when none). Composition queries the
     * window-manager port and activates the taken window. */
    void reach_launcher_remember_restore_window(reach_launcher *launcher, uintptr_t window);
    void reach_launcher_clear_restore_window(reach_launcher *launcher);
    uintptr_t reach_launcher_take_restore_window(reach_launcher *launcher);

    /*
     * Text input, owned by the capsule: typed characters / editing keys drive
     * the internal text edit, update the query, and run the attached search
     * (empty query cancels + clears results). Composition routes the raw
     * TEXT_CHAR / TEXT_EDIT events here and applies the reported redraw /
     * relayout to its surface bookkeeping. reset_text_edit reinitializes the
     * box + caret (launcher open and close-cleanup paths).
     */
    typedef struct reach_launcher_text_event_result
    {
        int32_t redraw;   /* launcher surface needs a redraw */
        int32_t relayout; /* results changed shape; recompute layout */
    } reach_launcher_text_event_result;

    void reach_launcher_handle_text_event(reach_launcher *launcher, const reach_ui_event *event,
                                          reach_launcher_text_event_result *out_result);
    void reach_launcher_reset_text_edit(reach_launcher *launcher);

    /*
     * Const queries (Pattern R, Item 6a): composition reads capsule facts through
     * these instead of reaching into the model.
     */
    int32_t reach_launcher_is_open(reach_launcher *launcher);
    size_t reach_launcher_result_count(reach_launcher *launcher);
    const reach_search_candidate *reach_launcher_result_at(reach_launcher *launcher, size_t index);
    size_t reach_launcher_selected_result_index(reach_launcher *launcher);
    const uint16_t *reach_launcher_query_text(reach_launcher *launcher);

    /* Semantic op (Pattern W, Item 6c): drop the query text (composition cancels
     * the search worker and resets the search box around it). */
    void reach_launcher_clear_query(reach_launcher *launcher);

    /* Launcher key/intent dispatch (the former reach_ui_handle_event). */
    reach_result reach_launcher_handle_event(reach_launcher *launcher,
                                             const reach_ui_event *event,
                                             reach_ui_intent *out_intent);

    typedef struct reach_launcher_render_input
    {
        const reach_theme *theme;
        const reach_launcher_model *model;
        const reach_launcher_layout *layout;
        /* Result icon ids pulled from the icon service at append time
           (parallel to model->results; null = no icons). */
        const uint64_t *result_icon_ids;
        float dpi_scale;
        int32_t text_alignment_leading;
        /* Search-box edit state (the in-app textbox replacing the native EDIT).
         * The text shown is the model's query; these carry the caret + selection
         * the query model does not. */
        int32_t caret_index;
        int32_t caret_visible;
        int32_t selection_start;
        int32_t selection_end;
    } reach_launcher_render_input;

    typedef enum reach_launcher_pointer_action_kind
    {
        REACH_LAUNCHER_POINTER_ACTION_NONE = 0,
        REACH_LAUNCHER_POINTER_ACTION_LAUNCH_PINNED = 1,
        REACH_LAUNCHER_POINTER_ACTION_OPEN_RESULT = 2,
        REACH_LAUNCHER_POINTER_ACTION_REVEAL_RESULT = 3
    } reach_launcher_pointer_action_kind;

    /* Full launcher capsule state: the model plus the press/drag interaction that
     * used to live as flat fields on reach_host. */
    struct reach_launcher_state
    {
        reach_launcher_model model;
        int32_t pressed_launcher_hit_type;
        size_t pressed_launcher_index;
        reach_scrollbar_drag launcher_scrollbar_drag;
        /* In-app search box (replaces the native EDIT). The text-input handler +
         * search scheduling stay in composition; the data lives here. */
        reach_text_edit launcher_text_edit;
        int32_t launcher_focused;
        /* Focus-restore target while the launcher is open (the window that was
         * foreground when it opened; moved off reach_host). */
        uintptr_t restore_window;
        int32_t restore_window_valid;
        double launcher_caret_blink_seconds;
        int32_t launcher_caret_visible;
    };

    reach_result reach_launcher_build_render_commands(const reach_launcher_render_input *input,
                                                      reach_render_command_buffer *out_commands);

    /*
     * Render assembly, owned by the launcher capsule (moved out of composition in
     * the behavior-encapsulation phase): assembles the render input from its own
     * model + search-box edit state plus the borrowed layout and appends its
     * commands; composition owns the surface frame.
     */
    typedef struct reach_launcher_render_context
    {
        const reach_theme *theme;
        const reach_launcher_layout *layout; /* composition-owned, borrowed */
        float dpi_scale;
    } reach_launcher_render_context;

    reach_result reach_launcher_append_render_commands(reach_launcher *launcher,
                                                       const reach_launcher_render_context *ctx,
                                                       reach_render_command_buffer *out_commands);
#ifdef __cplusplus
}
#endif

#endif
