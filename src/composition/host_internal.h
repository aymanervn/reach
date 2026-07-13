#ifndef REACH_HOST_INTERNAL_H
#define REACH_HOST_INTERNAL_H

#include "reach/composition/host.h"

#include "reach/core/render_commands.h"
#include "reach/core/ui_events.h"
#include "reach/core/ui_layout.h"

#include "reach/features/common/text_edit.h"
#include "reach/features/context_menu.h"
#include "reach/features/feature_capsule.h"
#include "reach/features/clipboard.h"
#include "reach/features/dock.h"
#include "reach/features/launcher.h"
#include "reach/services/pin_config.h"
#include "reach/features/popup.h"
#include "reach/features/quick_settings.h"
#include "reach/features/switcher.h"
#include "reach/features/tray.h"
#include "reach/features/wallpaper.h"

#include "reach/core/runtime_policy.h"
#include "reach/composition/surface_runtime.h"
#include "reach/services/app_control.h"
#include "reach/services/config.h"
#include "reach/services/icon_service.h"
#include "reach/services/now_playing.h"
#include "reach/services/search.h"
#include "reach/services/system_status.h"
#include "reach/services/window_tracking.h"
#include "reach/support/animation.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

typedef enum reach_host_animation_id
{
    REACH_HOST_ANIMATION_LAUNCHER_TRANSITION_Y = 0,
    REACH_HOST_ANIMATION_LAUNCHER_TRANSITION_OPACITY,
    REACH_HOST_ANIMATION_TRAY_TRANSITION_Y,
    REACH_HOST_ANIMATION_TRAY_TRANSITION_OPACITY,
    REACH_HOST_ANIMATION_QUICK_SETTINGS_TRANSITION_Y,
    REACH_HOST_ANIMATION_QUICK_SETTINGS_TRANSITION_OPACITY,
    REACH_HOST_ANIMATION_SWITCHER_TRANSITION_Y,
    REACH_HOST_ANIMATION_SWITCHER_TRANSITION_OPACITY,
    REACH_HOST_ANIMATION_CONTEXT_MENU_TRANSITION_Y,
    REACH_HOST_ANIMATION_CONTEXT_MENU_TRANSITION_OPACITY,
    REACH_HOST_ANIMATION_CLIPBOARD_TRANSITION_Y,
    REACH_HOST_ANIMATION_CLIPBOARD_TRANSITION_OPACITY,
    REACH_HOST_ANIMATION_COUNT
} reach_host_animation_id;

typedef struct reach_host_surface_transition
{
    int32_t visible;
    int32_t target_open;
    size_t y_track;
    size_t opacity_track;
} reach_host_surface_transition;

typedef enum reach_surface_class
{
    REACH_SURFACE_CLASS_PERSISTENT = 0,
    REACH_SURFACE_CLASS_TRANSIENT = 1,
    REACH_SURFACE_CLASS_POPUP = 2,
    REACH_SURFACE_CLASS_OVERLAY = 3
} reach_surface_class;

typedef enum reach_surface_id
{
    REACH_SURFACE_ID_DOCK = 0,
    REACH_SURFACE_ID_LAUNCHER,
    REACH_SURFACE_ID_CLIPBOARD,
    REACH_SURFACE_ID_TRAY,
    REACH_SURFACE_ID_QUICK_SETTINGS,
    REACH_SURFACE_ID_CONTEXT_MENU,
    REACH_SURFACE_ID_SWITCHER,
    REACH_HOST_SURFACE_COUNT
} reach_surface_id;

typedef enum reach_surface_pointer_flags
{
    REACH_SURFACE_POINTER_NONE = 0,
    REACH_SURFACE_POINTER_RELAYOUT_REDRAWS = 1u << 0,
    REACH_SURFACE_POINTER_UPDATES_DOCK_VISIBILITY = 1u << 1,

    REACH_SURFACE_POINTER_SOURCE_GATED = 1u << 2,

    REACH_SURFACE_POINTER_DOWN_CLOSES_ON_UNHANDLED = 1u << 3,

    REACH_SURFACE_POINTER_DOWN_APPLIES_UNHANDLED = 1u << 4
} reach_surface_pointer_flags;

typedef struct reach_surface_desc
{
    reach_surface_id id;
    reach_surface_class cls;
    reach_surface_runtime *surface;
    reach_host_surface_transition *transition;

    void (*force_close)(reach_host *host);

    void *capsule;
    const reach_feature_capsule_ops *capsule_ops;

    uint32_t pointer_flags;

    reach_surface_role role;

    int32_t pointer_priority;

    reach_result (*apply_pointer_action)(reach_host *host, const reach_ui_event *event,
                                         const reach_capsule_pointer_result *result);

    void (*dismiss)(reach_host *host);

    reach_result (*frame)(reach_host *host, const struct reach_host_frame_context *ctx);
    int32_t frame_priority;

    const reach_ui_event_type *toggle_events;
    size_t toggle_event_count;
    void (*toggle)(reach_host *host);

    const reach_ui_event_type *routed_events;
    size_t routed_event_count;
    reach_result (*handle_routed)(reach_host *host, const reach_ui_event *event);
} reach_surface_desc;

void reach_host_init_surface_descriptors(reach_host *host);

int32_t reach_host_any_surface_open(reach_host *host, uint32_t class_mask);
int32_t reach_host_any_surface_dirty(const reach_host *host);

void reach_host_close_other_popups(reach_host *host, reach_surface_id keep);

struct reach_host_frame_context
{
    int32_t game_mode;
    reach_rect_f32 monitor_bounds;
    int32_t dock_layout_changed;
    int32_t launcher_layout_changed;
};
typedef struct reach_host_frame_context reach_host_frame_context;

reach_result reach_host_frame_launcher(reach_host *host, const reach_host_frame_context *ctx);
reach_result reach_host_frame_clipboard(reach_host *host, const reach_host_frame_context *ctx);
reach_result reach_host_frame_dock(reach_host *host, const reach_host_frame_context *ctx);
reach_result reach_host_frame_tray(reach_host *host, const reach_host_frame_context *ctx);
reach_result reach_host_frame_quick_settings(reach_host *host, const reach_host_frame_context *ctx);
reach_result reach_host_frame_switcher(reach_host *host, const reach_host_frame_context *ctx);
reach_result reach_host_frame_context_menu(reach_host *host, const reach_host_frame_context *ctx);

static inline uint32_t reach_surface_class_bit(reach_surface_class cls)
{
    return 1u << (uint32_t)cls;
}

typedef struct reach_host_deferred_launch
{
    int32_t active;
    reach_app_launch_request request;
} reach_host_deferred_launch;

typedef struct reach_host_dirty_state
{
    int32_t layout;
    int32_t render;
    int32_t update_requested;
    int32_t events_dispatched_this_cycle;
    int32_t monitors;
} reach_host_dirty_state;

typedef struct reach_host_dock_reveal_state
{
    int32_t edge_visible;
    int32_t edge_bounds_valid;
    int32_t subscriptions_initialized;
    int32_t move_enabled[7];
    reach_rect_f32 edge_bounds;
} reach_host_dock_reveal_state;

struct reach_host
{
    reach_monitor_port monitors;

    reach_dock_model dock_config;

    reach_pinned_app_model pinned_apps[REACH_MAX_PINNED_APPS];
    size_t pinned_app_count;
    reach_surface_runtime launcher;
    reach_surface_runtime dock;
    reach_dock_reveal_edge_port dock_reveal_edge;
    reach_surface_runtime tray;
    reach_surface_runtime switcher;
    reach_surface_runtime context_menu;
    reach_surface_runtime quick_settings;
    reach_surface_runtime clipboard_surface;
    reach_host_surface_transition launcher_transition;
    reach_host_surface_transition tray_transition;
    reach_host_surface_transition switcher_transition;
    reach_host_surface_transition context_menu_transition;
    reach_host_surface_transition quick_settings_transition;
    reach_host_surface_transition clipboard_transition;

    reach_surface_desc surface_descs[REACH_HOST_SURFACE_COUNT];
    reach_input_source_port input_source;
    reach_window_manager_port window_manager;
    reach_config_store_port config_store;
    reach_tray_provider_port tray_provider;
    reach_search_provider_port search_provider;
    reach_app_launcher_port app_launcher;
    reach_settings_launcher_port settings_launcher;
    reach_icon_service *icon_service;
    reach_explorer_service_port explorer_service;
    reach_wallpaper_service_port wallpaper_service;
    reach_wallpaper_surface_port wallpaper_surface;
    reach_power_session_port power_session;
    const reach_theme *theme;

    reach_window_tracking *window_tracking;
    float layout_dpi_scale;
    reach_animation_manager animations;
    reach_animation_track animation_tracks[REACH_HOST_ANIMATION_COUNT];
    reach_ui_layout layout;
    int32_t has_layout;
    reach_host_dirty_state dirty;
    reach_config_service *config_service;
    reach_wallpaper *wallpaper;
    reach_dock *dock_capsule;
    reach_host_dock_reveal_state dock_reveal;
    reach_clipboard_feature *clipboard_capsule;
    reach_clipboard_port clipboard;
    reach_search_service *search_service;
    reach_app_control *app_control;
    reach_host_deferred_launch deferred_launch;
    reach_tray *tray_capsule;
    reach_switcher *switcher_capsule;
    reach_context_menu *context_menu_capsule;
    reach_launcher *launcher_capsule;
    int32_t running;
    reach_runtime_policy_state runtime_policy;
    reach_audio_volume_port audio_volume;
    reach_system_controls_port system_controls;
    reach_media_controls_port media_controls;
    reach_now_playing_service *now_playing_service;

    double popup_hook_reassert_seconds;
    std::atomic<uint32_t> quick_settings_system_change_flags;
    reach_system_status *system_status;
    reach_quick_settings *quick_settings_capsule;
    reach_popup_capture_port popup_capture;
};

static inline const reach_window_snapshot *reach_host_open_windows(const reach_host *host)
{
    return reach_window_tracking_windows(host->window_tracking);
}

static inline size_t reach_host_open_window_count(const reach_host *host)
{
    return reach_window_tracking_window_count(host->window_tracking);
}

static inline uintptr_t reach_host_foreground_window(const reach_host *host)
{
    return reach_window_tracking_foreground(host->window_tracking);
}

static inline float reach_host_layout_dpi_scale(const reach_host *host)
{
    return host != nullptr && host->layout_dpi_scale > 0.0f ? host->layout_dpi_scale : 1.0f;
}

static inline float reach_host_monitor_dpi_scale(const reach_monitor_info *monitor)
{
    if (monitor == nullptr)
    {
        return 1.0f;
    }

    int32_t dpi = monitor->dpi_y > 0 ? monitor->dpi_y : monitor->dpi_x;
    return dpi > 0 ? (float)dpi / 96.0f : 1.0f;
}

int32_t reach_host_rect_equal(reach_rect_f32 a, reach_rect_f32 b);
int32_t reach_host_opacity_equal(float a, float b);

reach_result reach_host_apply_window_state(reach_platform_window_port *window,
                                           reach_rect_f32 bounds, float opacity,
                                           reach_rect_f32 *last_bounds, float *last_opacity,
                                           int32_t *bounds_valid, int32_t *opacity_valid,
                                           int32_t *out_changed);
void reach_host_surface_transition_init(reach_host *host, reach_host_surface_transition *transition,
                                        size_t y_track, size_t opacity_track);
void reach_host_surface_transitions_init(reach_host *host);
void reach_host_surface_transition_set(reach_host *host, reach_host_surface_transition *transition,
                                       int32_t open);
reach_rect_f32 reach_host_surface_transition_bounds(const reach_host *host,
                                                    const reach_host_surface_transition *transition,
                                                    reach_rect_f32 target_bounds);
float reach_host_surface_transition_opacity(const reach_host *host,
                                            const reach_host_surface_transition *transition);
int32_t reach_host_surface_transition_visible(const reach_host_surface_transition *transition);
int32_t reach_host_surface_transition_active(const reach_host *host,
                                             const reach_host_surface_transition *transition);
void reach_host_surface_transition_finish(reach_host *host,
                                          reach_host_surface_transition *transition);

void reach_host_request_update(reach_host *host);
void reach_host_on_launcher_window_event(void *user, const reach_ui_event *event);
void reach_host_on_dock_window_event(void *user, const reach_ui_event *event);
void reach_host_on_tray_window_event(void *user, const reach_ui_event *event);
void reach_host_on_switcher_window_event(void *user, const reach_ui_event *event);
void reach_host_on_context_menu_window_event(void *user, const reach_ui_event *event);
void reach_host_on_quick_settings_window_event(void *user, const reach_ui_event *event);
void reach_host_on_clipboard_window_event(void *user, const reach_ui_event *event);

reach_result reach_host_render_popup_surface(reach_host *host, reach_surface_runtime *surface,
                                             reach_rect_f32 bounds, float notch_anchor_x,
                                             const reach_render_command_buffer *content_commands);

void reach_host_sync_popup_mouse_hook(reach_host *host);
void reach_host_close_transient_surfaces(reach_host *host, int32_t restore_launcher_focus);

void reach_host_notify_launcher_search_ready(reach_host *host);
void reach_host_cleanup_closed_launcher(reach_host *host);

void reach_host_close_launcher(reach_host *host);
void reach_host_close_launcher_without_focus_restore(reach_host *host);
void reach_host_remember_launcher_restore_window(reach_host *host);

void reach_host_toggle_launcher(reach_host *host);
void reach_host_clear_launcher_restore_window(reach_host *host);
void reach_host_restore_launcher_focus(reach_host *host);
reach_result reach_host_open_launcher_result(reach_host *host);
reach_result reach_host_reveal_launcher_result(reach_host *host, size_t result_index);
reach_result reach_host_schedule_app_launch(reach_host *host,
                                            const reach_app_launch_request *request);
void reach_host_stop_app_control(reach_host *host);
void reach_host_process_deferred_launcher_app_launch(reach_host *host);
reach_result
reach_host_defer_app_launch_until_launcher_closed(reach_host *host,
                                                  const reach_app_launch_request *request);

reach_result reach_host_focus_window(reach_host *host, uintptr_t window_id,
                                     int32_t minimize_if_foreground);
reach_result reach_host_launch_app(reach_host *host, const uint16_t *path,
                                   const uint16_t *arguments, int32_t force_new_instance,
                                   int32_t run_as_admin, int32_t defer_until_launcher_closed);
reach_result reach_host_open_app(reach_host *host, const uint16_t *path, const uint16_t *arguments,
                                 const uint16_t *app_user_model_id, int32_t force_new_instance,
                                 int32_t defer_until_launcher_closed);
reach_result reach_host_set_pinned_apps(reach_host *host, const reach_pinned_app_model *apps,
                                        size_t count);
reach_result reach_host_open_pinned_app(reach_host *host, size_t pinned_index,
                                        int32_t force_new_instance,
                                        int32_t defer_until_launcher_closed);
reach_result reach_host_open_pinned_app_id(reach_host *host, uint32_t pin_id,
                                           int32_t force_new_instance,
                                           int32_t defer_until_launcher_closed);
void reach_host_apply_launcher_search_results(reach_host *host);
void reach_host_stop_launcher_search_worker(reach_host *host);

reach_result reach_host_apply_clipboard_pointer_action(reach_host *host,
                                                       const reach_ui_event *event,
                                                       const reach_capsule_pointer_result *result);
void reach_host_release_clipboard_item(reach_host *host, const reach_clipboard_item *item);
void reach_host_clear_clipboard(reach_host *host);
void reach_host_set_clipboard_open(reach_host *host, int32_t open);
void reach_host_toggle_clipboard(reach_host *host);
void reach_host_process_clipboard_refresh(reach_host *host);
void reach_host_release_clipboard_items(reach_host *host);
reach_result reach_host_render_clipboard_surface(reach_host *host);

void reach_host_set_tray_popup_open(reach_host *host, int32_t open);
void reach_host_toggle_tray_popup(reach_host *host);
reach_result reach_host_refresh_tray_items(reach_host *host);

void reach_host_compute_tray_popup_layout(reach_host *host, const reach_dock_layout *dock_layout,
                                          reach_rect_f32 *out_bounds);

reach_result reach_host_apply_tray_pointer_action(reach_host *host, const reach_ui_event *event,
                                                  const reach_capsule_pointer_result *result);

void reach_host_close_context_menu(reach_host *host);
void reach_host_reanchor_context_menu(reach_host *host);
reach_result reach_host_execute_context_command(reach_host *host, uint32_t command);
reach_result reach_host_show_power_context_menu(reach_host *host);

reach_result reach_host_show_dock_app_context_menu(reach_host *host, size_t item_index, int32_t x,
                                                   int32_t y);

int32_t reach_host_dock_icon_size_px(const reach_host *host);

void reach_host_release_render_icon(reach_host *host, uint64_t icon_id);

void reach_host_drain_icon_evictions(reach_host *host);
void reach_host_release_tray_render_icons(reach_host *host);
void reach_host_release_quick_settings_audio_render_icons(reach_host *host);
void reach_host_drain_now_playing_retired_covers(reach_host *host);
reach_result reach_host_refresh_open_windows(reach_host *host, int32_t *out_changed);
void reach_host_note_foreground_window(reach_host *host, uintptr_t foreground_window);

int32_t reach_host_window_is_minimized(const reach_host *host, uintptr_t window_id);
reach_result reach_host_schedule_window_control(reach_host *host,
                                                reach_window_control_action action,
                                                uintptr_t window_id);
reach_result reach_host_schedule_minimize_open_windows(reach_host *host);
reach_result reach_host_schedule_open_terminal(reach_host *host);
reach_result reach_host_schedule_minimize_windows(reach_host *host, const uintptr_t *window_ids,
                                                  size_t window_count);
void reach_host_apply_window_control_result(reach_host *host);

reach_dock_build_context reach_host_dock_build_context(reach_host *host);

int32_t reach_host_dock_can_hide(const reach_host *host);
void reach_host_sync_pointer_move_subscriptions(reach_host *host);
void reach_host_request_dock_visibility_update(reach_host *host);

reach_rect_f32 reach_host_reconcile_dock_visibility(reach_host *host, reach_rect_f32 shown_bounds,
                                                    reach_rect_f32 monitor_bounds);

reach_result reach_host_refresh_monitor_layout(reach_host *host);
int32_t reach_host_can_move_dock_without_redraw(const reach_host *host);
reach_result reach_host_move_dock_animation_frame(reach_host *host);

const uint16_t *reach_host_dock_item_path(const reach_host *host, size_t item_index);

reach_result reach_host_launch_dock_item(reach_host *host, size_t item_index,
                                         int32_t force_new_instance);

void reach_host_clear_sticky_dock_feedback(reach_host *host);

void reach_host_set_quick_settings_open(reach_host *host, int32_t open);
void reach_host_toggle_quick_settings(reach_host *host);

void reach_host_process_quick_settings_changes(reach_host *host, double delta_seconds);
void reach_host_refresh_quick_settings_layout(reach_host *host);

reach_result reach_host_execute_media_action(reach_host *host, reach_now_playing_action action);
reach_result reach_host_step_main_volume(reach_host *host, float delta);
reach_result reach_host_toggle_main_volume_mute(reach_host *host);
reach_result reach_host_step_brightness(reach_host *host, float delta);
reach_result reach_host_snap_foreground_window(reach_host *host, reach_split_mode mode);
void reach_host_relayout_quick_settings(reach_host *host, int32_t animate_height);

void reach_host_update_quick_settings_animation(reach_host *host);

reach_result
reach_host_apply_quick_settings_pointer_action(reach_host *host, const reach_ui_event *event,
                                               const reach_capsule_pointer_result *result);

reach_result reach_host_apply_dock_pointer_action(reach_host *host, const reach_ui_event *event,
                                                  const reach_capsule_pointer_result *result);
reach_result reach_host_apply_launcher_pointer_action(reach_host *host, const reach_ui_event *event,
                                                      const reach_capsule_pointer_result *result);
reach_result
reach_host_apply_context_menu_pointer_action(reach_host *host, const reach_ui_event *event,
                                             const reach_capsule_pointer_result *result);

void reach_host_on_system_controls_changed(void *user, uint32_t change_flags);

size_t reach_host_switcher_visible_count(const reach_host *host);
void reach_host_refresh_switcher_windows(reach_host *host);

reach_result reach_host_handle_switcher_event(reach_host *host, const reach_ui_event *event);

reach_result reach_host_schedule_config_reload(reach_host *host);
reach_result reach_host_schedule_pin_app(reach_host *host, const reach_pinned_app_model *app);
reach_result reach_host_schedule_unpin_id(reach_host *host, uint32_t pin_id);
reach_result reach_host_schedule_move_pin(reach_host *host, uint32_t pin_id, size_t target_index);
int32_t reach_host_apply_config_reload_result(reach_host *host);
int32_t reach_host_config_reload_work_pending(const reach_host *host);
void reach_host_stop_config_reload_worker(reach_host *host);
reach_result reach_host_apply_config_snapshot(reach_host *host,
                                              const reach_config_snapshot *snapshot,
                                              int32_t apply_pins, int32_t apply_wallpaper);

void reach_host_seed_or_apply_wallpaper(reach_host *host, reach_config_snapshot *snapshot);

void reach_host_reload_wallpaper(reach_host *host, int32_t force);

reach_result reach_host_render_dock_surface(reach_host *host, const reach_dock_layout *layout);

reach_result reach_host_render_tray_surface(reach_host *host, reach_rect_f32 bounds);

reach_result reach_host_render_quick_settings_surface(reach_host *host);

reach_result reach_host_render_switcher_surface(reach_host *host, reach_rect_f32 bounds);

reach_result reach_host_render_launcher_surface(reach_host *host,
                                                const reach_launcher_layout *layout);

reach_result reach_host_render_context_menu_surface(reach_host *host);
int32_t reach_host_game_mode_enabled(const reach_host *host);
reach_result reach_host_update_game_mode(reach_host *host);
#endif
