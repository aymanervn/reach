#ifndef REACH_FEATURES_QUICK_SETTINGS_H
#define REACH_FEATURES_QUICK_SETTINGS_H

#include "reach/core/render_commands.h"
#include "reach/core/theme.h"
#include "reach/features/feature_capsule.h"
#include "reach/ports/audio_volume.h"
#include "reach/ports/system_controls.h"
#include "reach/services/system_status.h"
#include "reach/support/util.h"
#include "reach/support/animation.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_quick_settings_model
    {
        float main_volume_level;
        int32_t main_muted;
        int32_t expanded;
        int32_t output_devices_expanded;
        reach_audio_volume_session_list sessions;
        reach_audio_output_device_list output_devices;
        reach_network_state network;
        reach_bluetooth_state bluetooth;
        int32_t bluetooth_pending;
        int32_t bluetooth_pending_enabled;
        reach_power_state power;
        reach_brightness_state brightness;
    } reach_quick_settings_model;

    typedef struct reach_quick_settings_volume_pill_model
    {
        float volume_level;
        int32_t muted;
        uint32_t icon_id;
        uint16_t label[REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY];
        uint16_t session_instance_id[REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY];
    } reach_quick_settings_volume_pill_model;

    typedef struct reach_quick_settings_volume_pill_layout
    {
        reach_rect_f32 bounds;
        reach_rect_f32 header_icon;
        reach_rect_f32 header_label;
        reach_rect_f32 slider_track;
        reach_rect_f32 slider_fill;
    } reach_quick_settings_volume_pill_layout;

    typedef struct reach_quick_settings_app_volume_row_layout
    {
        reach_rect_f32 bounds;
        reach_rect_f32 app_icon;
        reach_rect_f32 app_label;
        reach_rect_f32 slider_full_range_line;
        reach_rect_f32 slider_level_line;
        reach_rect_f32 slider_thumb;
        reach_rect_f32 app_volume_percent;
        reach_rect_f32 separator;
    } reach_quick_settings_app_volume_row_layout;

    typedef struct reach_quick_settings_output_device_row_layout
    {
        reach_rect_f32 bounds;
        reach_rect_f32 icon;
        reach_rect_f32 label;
        reach_rect_f32 checkmark;
        reach_rect_f32 separator;
    } reach_quick_settings_output_device_row_layout;

    typedef struct reach_quick_settings_tile_layout
    {
        reach_rect_f32 bounds;
        reach_rect_f32 icon;
        reach_rect_f32 label;
    } reach_quick_settings_tile_layout;

    typedef struct reach_quick_settings_layout
    {
        reach_rect_f32 content_bounds;

        reach_rect_f32 system_grid_bounds;
        reach_quick_settings_tile_layout network_tile;
        reach_quick_settings_tile_layout bluetooth_tile;
        reach_quick_settings_tile_layout battery_saver_tile;
        reach_quick_settings_tile_layout project_tile;
        size_t system_tile_count;

        reach_quick_settings_volume_pill_layout brightness_pill;
        reach_rect_f32 brightness_slider_track;
        reach_rect_f32 brightness_slider_fill;

        reach_quick_settings_volume_pill_layout main_volume_pill;
        reach_rect_f32 main_slider_track;
        reach_rect_f32 main_slider_fill;

        reach_rect_f32 output_device_button;
        reach_rect_f32 output_device_button_icon;
        reach_rect_f32 output_device_button_label;
        reach_rect_f32 output_device_button_chevron;
        reach_rect_f32 output_devices_title;
        reach_rect_f32 output_devices_title_chevron;
        reach_rect_f32 output_devices_panel;
        reach_quick_settings_output_device_row_layout
            output_device_rows[REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES];
        size_t output_device_row_count;

        reach_rect_f32 app_volumes_title;
        reach_rect_f32 app_volumes_panel;
        reach_quick_settings_app_volume_row_layout app_volume_rows[REACH_AUDIO_VOLUME_MAX_SESSIONS];
        size_t app_volume_row_count;

        reach_rect_f32 expand_button;
        reach_rect_f32 expand_button_label;
        reach_rect_f32 expand_button_icon;
    } reach_quick_settings_layout;

    typedef enum reach_quick_settings_pointer_action_kind
    {
        REACH_QUICK_SETTINGS_POINTER_ACTION_NONE = 0,
        REACH_QUICK_SETTINGS_POINTER_ACTION_SET_MAIN_VOLUME,
        REACH_QUICK_SETTINGS_POINTER_ACTION_SET_SESSION_VOLUME,
        REACH_QUICK_SETTINGS_POINTER_ACTION_SET_BRIGHTNESS,
        REACH_QUICK_SETTINGS_POINTER_ACTION_NETWORK_TILE,
        REACH_QUICK_SETTINGS_POINTER_ACTION_TOGGLE_BLUETOOTH,
        REACH_QUICK_SETTINGS_POINTER_ACTION_TOGGLE_BATTERY_SAVER,
        REACH_QUICK_SETTINGS_POINTER_ACTION_OPEN_PROJECT,
        REACH_QUICK_SETTINGS_POINTER_ACTION_TOGGLE_OUTPUT_DEVICES,
        REACH_QUICK_SETTINGS_POINTER_ACTION_SET_OUTPUT_DEVICE,
        REACH_QUICK_SETTINGS_POINTER_ACTION_EXPAND
    } reach_quick_settings_pointer_action_kind;

    typedef struct reach_quick_settings_render_input
    {
        reach_quick_settings_model model;
        reach_quick_settings_layout layout;
        reach_theme theme;
        float dpi_scale;
    } reach_quick_settings_render_input;

    void reach_quick_settings_model_init(reach_quick_settings_model *model);

    void reach_quick_settings_model_set_main_volume(reach_quick_settings_model *model,
                                                    float volume_level, int32_t muted);

    void reach_quick_settings_model_set_sessions(reach_quick_settings_model *model,
                                                 const reach_audio_volume_session_list *sessions);

    void
    reach_quick_settings_model_set_output_devices(reach_quick_settings_model *model,
                                                  const reach_audio_output_device_list *devices);

    void reach_quick_settings_model_set_system_states(reach_quick_settings_model *model,
                                                      const reach_network_state *network,
                                                      const reach_bluetooth_state *bluetooth,
                                                      const reach_power_state *power,
                                                      const reach_brightness_state *brightness);

    uint32_t reach_quick_settings_volume_icon_id(float volume_level, int32_t muted);

    void reach_quick_settings_volume_pill_model_init(reach_quick_settings_volume_pill_model *model,
                                                     float volume_level, int32_t muted,
                                                     const uint16_t *label);

    reach_quick_settings_volume_pill_layout
    reach_quick_settings_volume_pill_layout_for_bounds_scaled(reach_rect_f32 bounds,
                                                              const reach_theme *theme,
                                                              float dpi_scale);

    float reach_quick_settings_volume_pill_level_for_x(
        const reach_quick_settings_volume_pill_layout *layout, float x);

    reach_quick_settings_layout
    reach_quick_settings_layout_for_content_bounds(reach_rect_f32 content_bounds,
                                                   const reach_theme *theme,
                                                   const reach_quick_settings_model *model);
    reach_quick_settings_layout reach_quick_settings_layout_for_content_bounds_scaled(
        reach_rect_f32 content_bounds, const reach_theme *theme,
        const reach_quick_settings_model *model, float dpi_scale);

    float reach_quick_settings_content_height_for_model(const reach_quick_settings_model *model);
    float
    reach_quick_settings_content_height_for_model_scaled(const reach_quick_settings_model *model,
                                                         float dpi_scale);

    reach_result
    reach_quick_settings_build_render_commands(const reach_quick_settings_render_input *input,
                                               reach_render_command_buffer *commands);

    /*
     * Quick-settings animation capsule. Owns the expand/collapse height animation
     * (its own reach_animation_manager). Composition ticks it once per frame, starts/
     * resets it as the panel opens and its content height changes, and reads the
     * animated height; the surface show/hide transition stays composition's policy.
     */
    typedef struct reach_quick_settings reach_quick_settings;

    reach_result reach_quick_settings_create(reach_quick_settings **out_quick_settings);
    void reach_quick_settings_destroy(reach_quick_settings *quick_settings);
    void reach_quick_settings_reset_height_animation(reach_quick_settings *quick_settings);
    int32_t reach_quick_settings_height_animation_active(const reach_quick_settings *quick_settings);

    /*
     * Quick-settings internal state (the open flag, the displayed model + its
     * computed layout, and the animated panel geometry). Owned by the
     * reach_quick_settings capsule (the same object that owns the panel's height
     * animation); composition reads it via the const reach_quick_settings_state_ptr()
     * and mutates only through the semantic ops. The OS audio/network/bluetooth
     * data and its refresh/watching state machines stay in composition (they feed
     * the model through the apply_* ops).
     */
    /*
     * Slider drag interaction (brightness / per-app volume). Owned by the QS capsule
     * (moved out of reach_host in the behavior-encapsulation phase). A zero type
     * is the inactive sentinel, so a zero-initialized capsule starts inactive.
     */
    typedef struct reach_quick_settings_drag_state
    {
        int32_t active;
        int32_t type;
        float last_level;
        int32_t level_valid;
        size_t session_index;
        uint16_t session_instance_id[REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY];
    } reach_quick_settings_drag_state;

    typedef struct reach_quick_settings_state
    {
        reach_quick_settings_model model;
        reach_quick_settings_layout layout;
        int32_t open;
        reach_rect_f32 bounds;
        reach_rect_f32 target_bounds;
        reach_rect_f32 content_bounds;
        float notch_anchor_x;
        reach_quick_settings_drag_state drag;
    } reach_quick_settings_state;

    /* The capsule's owned internal state, read-only outside the feature
     * (never null for a live capsule). All writes go through the semantic ops. */
    const reach_quick_settings_state *
    reach_quick_settings_state_ptr(reach_quick_settings *quick_settings);

    /* The uniform capsule hooks, including complete pointer-stream ownership. */
    const reach_feature_capsule_ops *reach_quick_settings_capsule_ops(void);

    /*
     * Semantic state ops. set_open flips only the capsule's open flag and
     * resets the slider drag (returns 1 when it changed) — surface transition,
     * refresh kicks, and popup policy stay in composition. force_close is the
     * shutdown/display-change close (open + drag drop, verbatim). reset
     * re-initializes model + panel geometry (host init/restart).
     */
    int32_t reach_quick_settings_is_open(const reach_quick_settings *quick_settings);
    int32_t reach_quick_settings_set_open(reach_quick_settings *quick_settings, int32_t open);
    void reach_quick_settings_force_close(reach_quick_settings *quick_settings);

    /* Capsule-level model feeds (the refresh workers/action translator deliver
     * through these instead of reaching into the model). */
    void reach_quick_settings_apply_main_volume(reach_quick_settings *quick_settings, float level,
                                                int32_t muted);
    void reach_quick_settings_apply_sessions(reach_quick_settings *quick_settings,
                                             const reach_audio_volume_session_list *sessions);
    void reach_quick_settings_apply_output_devices(
        reach_quick_settings *quick_settings, const reach_audio_output_device_list *devices);

    /*
     * Render assembly, owned by the quick-settings capsule (moved out of
     * composition in the behavior-encapsulation phase): assembles the render input
     * from its own model + layout and appends its commands; composition owns the
     * popup surface frame.
     */
    reach_result reach_quick_settings_append_render_commands(
        reach_quick_settings *quick_settings, const reach_theme *theme, float dpi_scale,
        reach_render_command_buffer *out_commands);

    /*
     * Layout + open/close height animation, owned by the capsule (moved out of
     * composition in the behavior-encapsulation phase). The popup anchors to the
     * dock's quick-settings button; composition computes the anchor / dock top from
     * its layout and borrows them in. Composition still owns the popup surface
     * (bounds application, transition, visibility policy).
     */
    typedef struct reach_quick_settings_layout_context
    {
        const reach_theme *theme;
        float dpi_scale;
        float anchor_x; /* dock quick-settings button center, screen space */
        float dock_top; /* dock bounds top, screen space */
    } reach_quick_settings_layout_context;

    /* Recompute target bounds, (unless height-animating) bounds, and the content
     * layout from the model. */
    void reach_quick_settings_refresh_layout(reach_quick_settings *quick_settings,
                                             const reach_quick_settings_layout_context *ctx);

    /* Retarget the popup size; optionally height-animate toward the new target. */
    void reach_quick_settings_relayout(reach_quick_settings *quick_settings,
                                       const reach_quick_settings_layout_context *ctx,
                                       int32_t animate_height);

    /* Per-frame height animation while open. Returns 1 when the bounds advanced
     * (composition marks the surface dirty and re-renders). */
    int32_t
    reach_quick_settings_update_open_animation(reach_quick_settings *quick_settings,
                                               const reach_quick_settings_layout_context *ctx);

    /*
     * Const queries + semantic ops (Patterns R/W, Item 6d): the action translator
     * and the refresh-worker delivery stop reaching into the model.
     */
    int32_t reach_quick_settings_bluetooth_pending(reach_quick_settings *quick_settings);
    int32_t reach_quick_settings_bluetooth_available(reach_quick_settings *quick_settings);
    int32_t reach_quick_settings_bluetooth_enabled(reach_quick_settings *quick_settings);
    void reach_quick_settings_set_bluetooth_pending(reach_quick_settings *quick_settings,
                                                    int32_t pending, int32_t pending_enabled);

    /* Toggle the expanded sections; each collapses the other. Returns the new value. */
    int32_t reach_quick_settings_toggle_expanded(reach_quick_settings *quick_settings);
    int32_t reach_quick_settings_toggle_output_devices(reach_quick_settings *quick_settings);
    void reach_quick_settings_collapse_output_devices(reach_quick_settings *quick_settings);

    /* Set a session slider level locally. Returns the current session instance
     * id for the audio port, or null when the semantic action index is stale. */
    const uint16_t *reach_quick_settings_set_session_level(reach_quick_settings *quick_settings,
                                                           size_t session_index, float level);
    const uint16_t *reach_quick_settings_output_device_id(
        const reach_quick_settings *quick_settings, size_t output_device_index);

    /* Apply a system-refresh worker result: system states + the bluetooth-pending
     * settle check + the relayout-needed decision. */
    typedef struct reach_quick_settings_system_apply_result
    {
        int32_t bluetooth_pending_cleared; /* composition clears its pending tracker */
        int32_t relayout;                  /* battery/brightness presence changed while open */
    } reach_quick_settings_system_apply_result;

    void reach_quick_settings_apply_system_states(
        reach_quick_settings *quick_settings, const reach_network_state *network,
        const reach_bluetooth_state *bluetooth, const reach_power_state *power,
        const reach_brightness_state *brightness, int32_t bluetooth_valid,
        reach_quick_settings_system_apply_result *out);

    /*
     * The quick settings CONSUME the system-status service directly (refresh
     * requests + snapshot take; the launcher→search precedent). Attached at
     * wiring; composition owns the service lifecycle and every OS mutation.
     */
    void reach_quick_settings_attach_status(reach_quick_settings *quick_settings,
                                            reach_system_status *status);

    /* Kick the async refreshes on the attached service (no-op unattached). */
    void reach_quick_settings_refresh_audio(reach_quick_settings *quick_settings);
    void reach_quick_settings_refresh_system(reach_quick_settings *quick_settings,
                                             uint32_t change_flags);

    /*
     * Per-tick service pump: starts a system refresh for the drained watcher
     * change flags (the watcher callback fires on a port thread, so composition
     * keeps the atomic accumulator and passes the drained flags in), runs the
     * bluetooth-pending grace timers, and applies completed audio/system
     * snapshots to the model. out reports redraw / relayout (composition runs
     * its anchored relayout with animate_height=1) / request_update.
     * Audio applies retire the replaced session/device render icons; GPU
     * lifetime stays composition-owned, so composition drains them with
     * reach_quick_settings_take_retired_render_icons after each call.
     */
    void reach_quick_settings_process_changes(reach_quick_settings *quick_settings,
                                              uint32_t change_flags, double delta_seconds,
                                              reach_feature_tick_result *out);
    size_t reach_quick_settings_take_retired_render_icons(reach_quick_settings *quick_settings,
                                                          uint64_t *out_ids, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
