#ifndef REACH_SHELL_INTERNAL_H
#define REACH_SHELL_INTERNAL_H

#include "reach/shell/shell.h"

#include "reach/core/render_commands.h"
#include "reach/core/theme.h"
#include "reach/core/ui_events.h"
#include "reach/core/ui_layout.h"

#include "reach/features/context_menu.h"
#include "reach/features/dock.h"
#include "reach/features/launcher.h"
#include "reach/features/pin_config.h"
#include "reach/features/popup.h"
#include "reach/features/quick_settings.h"
#include "reach/features/switcher.h"
#include "reach/features/tray.h"
#include "reach/features/wallpaper.h"

#include "reach/core/runtime_policy.h"
#include "reach/shell/surface_runtime.h"
#include "reach/support/animation.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

typedef struct reach_shell_popup_bounds_animation {
  reach_float_animation width;
  reach_float_animation height;
  int32_t animate_width;
  int32_t animate_height;
  int32_t active;
} reach_shell_popup_bounds_animation;

typedef struct reach_shell_dock_drag_state {
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
  int32_t snapping;
  int32_t reload_pins_after_snap;
  reach_float_animation snap_animation;
} reach_shell_dock_drag_state;

typedef struct reach_shell_feedback_state {
  size_t dock_index;
  int32_t dock_pressed;
  int32_t dock_sticky;
  int32_t dock_animating;
  reach_float_animation dock_opacity;

  size_t tray_index;
  int32_t tray_pressed;
  int32_t tray_animating;
  reach_float_animation tray_opacity;
} reach_shell_feedback_state;

typedef struct reach_shell_quick_settings_drag_state {
  int32_t active;
  reach_quick_settings_hit_type type;
  float last_level;
  int32_t level_valid;
  size_t session_index;
  uint16_t session_instance_id[REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY];
} reach_shell_quick_settings_drag_state;

typedef struct reach_shell_launcher_search_state {
  reach_icon_handle result_icons[REACH_SEARCH_MAX_RESULTS];

  std::thread thread;
  std::mutex mutex;
  std::condition_variable cv;

  int32_t thread_started;
  int32_t stop;
  int32_t pending;
  int32_t in_flight;

  uint32_t generation;
  uint32_t pending_generation;
  uint16_t pending_query[REACH_MAX_SEARCH_CHARS + 1];

  int32_t completed;
  uint32_t completed_generation;
  reach_search_candidate completed_results[REACH_SEARCH_MAX_RESULTS];
  size_t completed_count;

  void (*notify)(reach_shell *shell);
} reach_shell_launcher_search_state;

typedef struct reach_shell_context_menu_state {
  int32_t open;
  int32_t power_open;
  size_t target_index;
  reach_rect_f32 bounds;
  reach_rect_f32 item_slots[REACH_CONTEXT_MENU_MAX_ITEMS];
  uint32_t item_commands[REACH_CONTEXT_MENU_MAX_ITEMS];
  uint32_t item_icon_ids[REACH_CONTEXT_MENU_MAX_ITEMS];
  size_t item_count;
  size_t hovered_index;
} reach_shell_context_menu_state;

typedef struct reach_shell_tray_state {
  int32_t popup_open;
  reach_tray_model model;
} reach_shell_tray_state;

typedef struct reach_shell_switcher_state {
  int32_t open;
  size_t selected_index;
  size_t visible_start;
} reach_shell_switcher_state;

typedef struct reach_shell_wallpaper_state {
  int32_t bounds_valid;
  reach_rect_f32 bounds;
  uint16_t path[260];
} reach_shell_wallpaper_state;

typedef struct reach_shell_dirty_state {
  int32_t layout;
  int32_t render;
  int32_t update_requested;
  int32_t events_dispatched_this_cycle;
  int32_t monitors;
} reach_shell_dirty_state;

typedef struct reach_shell_dock_animation_state {
  int32_t initialized;
  int32_t animating;
  reach_float_animation y;
} reach_shell_dock_animation_state;
typedef struct reach_shell_dock_reveal_state {
  int32_t target_hidden;
  int32_t active;
  int32_t requested;
  int32_t check_dirty;
  int32_t edge_visible;
  int32_t edge_bounds_valid;
  reach_rect_f32 edge_bounds;
} reach_shell_dock_reveal_state;

typedef struct reach_shell_dock_width_state {
  int32_t initialized;
  int32_t animating;
  size_t item_count;
  reach_float_animation animation;
} reach_shell_dock_width_state;

struct reach_shell {
  reach_hotkeys_port hotkeys;
  reach_monitor_port monitors;
  reach_ui_state ui;
  reach_surface_runtime launcher;
  reach_surface_runtime dock;
  reach_dock_reveal_edge_port dock_reveal_edge;
  reach_surface_runtime tray;
  reach_surface_runtime switcher;
  reach_surface_runtime context_menu;
  reach_surface_runtime quick_settings;
  reach_input_source_port input_source;
  reach_window_manager_port window_manager;
  reach_config_store_port config_store;
  reach_tray_provider_port tray_provider;
  reach_search_provider_port search_provider;
  reach_app_launcher_port app_launcher;
  reach_icon_provider_port icon_provider;
  reach_explorer_service_port explorer_service;
  reach_wallpaper_service_port wallpaper_service;
  reach_wallpaper_surface_port wallpaper_surface;
  reach_power_session_port power_session;
  const reach_theme *theme;
  reach_window_snapshot open_windows[REACH_MAX_PINNED_APPS];
  size_t open_window_count;
  uintptr_t foreground_window;
  reach_dock_feature_model dock_model;
  reach_dock_icon_cache dock_icons;
  reach_float_animation dock_item_x_animations[REACH_MAX_PINNED_APPS];
  int32_t dock_item_x_animating[REACH_MAX_PINNED_APPS];
  int32_t dock_item_x_valid[REACH_MAX_PINNED_APPS];
  int32_t dock_item_x_pinned[REACH_MAX_PINNED_APPS];
  uint32_t dock_item_x_pin_ids[REACH_MAX_PINNED_APPS];
  uintptr_t dock_item_x_windows[REACH_MAX_PINNED_APPS];
  int32_t dock_items_changed;
  reach_ui_layout layout;
  int32_t has_layout;
  reach_shell_dirty_state dirty;
  reach_shell_wallpaper_state wallpaper_state;
  reach_shell_dock_animation_state dock_animation;
  reach_shell_dock_reveal_state dock_reveal;
  reach_shell_dock_width_state dock_width;
  reach_shell_dock_drag_state dock_drag;
  reach_shell_feedback_state feedback;
  size_t pressed_dock_index;
  reach_launcher_hit_type pressed_launcher_hit_type;
  size_t pressed_launcher_index;
  reach_shell_launcher_search_state launcher_search;
  int32_t suppress_power_button_release;
  reach_shell_tray_state tray_state;
  reach_shell_switcher_state switcher_state;
  reach_shell_context_menu_state context_menu_state;
  uint16_t dock_time_text[32];
  uint16_t dock_date_text[64];
  int32_t dock_clock_initialized;
  int64_t dock_clock_last_minute;
  int32_t running;
  reach_runtime_policy_state runtime_policy;
  reach_audio_volume_port audio_volume;
  reach_system_controls_port system_controls;
  std::atomic<uint32_t> quick_settings_system_change_flags;
  int32_t quick_settings_open;
  reach_shell_quick_settings_drag_state quick_settings_drag;
  reach_audio_volume_state quick_settings_audio_state;
  reach_audio_volume_session_list quick_settings_audio_sessions;
  reach_audio_output_device_list quick_settings_output_devices;
  reach_quick_settings_model quick_settings_model;
  reach_quick_settings_layout quick_settings_layout;
  reach_rect_f32 quick_settings_bounds;
  reach_rect_f32 quick_settings_target_bounds;
  reach_rect_f32 quick_settings_content_bounds;
  float quick_settings_notch_anchor_x;
  reach_shell_popup_bounds_animation quick_settings_bounds_animation;
  reach_popup_capture_port popup_capture;
};

static const size_t REACH_SHELL_DOCK_FEEDBACK_TRAY_BUTTON =
    REACH_MAX_PINNED_APPS;
static const size_t REACH_SHELL_DOCK_FEEDBACK_QUICK_SETTINGS_BUTTON =
    REACH_MAX_PINNED_APPS + 1;
static const size_t REACH_SHELL_DOCK_FEEDBACK_POWER_BUTTON =
    REACH_MAX_PINNED_APPS + 2;
static const size_t REACH_SHELL_DOCK_FEEDBACK_NONE = REACH_MAX_PINNED_APPS + 3;

/* Generic shell/window helpers */

int32_t reach_shell_rect_equal(reach_rect_f32 a, reach_rect_f32 b);
int32_t reach_shell_opacity_equal(float a, float b);

reach_result reach_shell_apply_window_state(
    reach_platform_window_port *window, reach_rect_f32 bounds, float opacity,
    reach_rect_f32 *last_bounds, float *last_opacity, int32_t *bounds_valid,
    int32_t *opacity_valid, int32_t *out_changed);

void reach_shell_request_update(reach_shell *shell);
void reach_shell_on_window_event(void *user, const reach_ui_event *event);

/* Popup/window capture helpers */

reach_result reach_shell_render_popup_surface(
    reach_shell *shell, reach_surface_runtime *surface, reach_rect_f32 bounds,
    float notch_anchor_x, const reach_render_command_buffer *content_commands);

void reach_shell_start_popup_bounds_animation(
    reach_shell_popup_bounds_animation *animation,
    reach_rect_f32 current_bounds, reach_rect_f32 target_bounds,
    int32_t animate_width, int32_t animate_height, double duration_seconds);

reach_rect_f32 reach_shell_apply_popup_bounds_animation(
    reach_shell_popup_bounds_animation *animation, reach_rect_f32 target_bounds,
    float anchor_x, float reference_y, float gap, double delta_seconds);

int32_t reach_shell_popup_bounds_animation_active(
    const reach_shell_popup_bounds_animation *animation);

void reach_shell_sync_popup_mouse_hook(reach_shell *shell);

void reach_shell_capture_tray_input(reach_shell *shell);
void reach_shell_release_tray_input(reach_shell *shell);

void reach_shell_capture_dock_input(reach_shell *shell);
void reach_shell_release_dock_input(reach_shell *shell);

void reach_shell_capture_context_menu_input(reach_shell *shell);
void reach_shell_release_context_menu_input(reach_shell *shell);

/* Launcher orchestration */

void reach_shell_raise_launcher(reach_shell *shell);
void reach_shell_notify_launcher_search_ready(reach_shell *shell);

void reach_shell_close_launcher(reach_shell *shell);
reach_result reach_shell_open_launcher_result(reach_shell *shell);

void reach_shell_cancel_launcher_search(reach_shell *shell);
reach_result reach_shell_schedule_launcher_search(reach_shell *shell);
void reach_shell_apply_launcher_search_results(reach_shell *shell);
void reach_shell_stop_launcher_search_worker(reach_shell *shell);
void reach_shell_release_launcher_result_icons(reach_shell *shell);

/* Tray orchestration */

void reach_shell_set_tray_popup_open(reach_shell *shell, int32_t open);
void reach_shell_toggle_tray_popup(reach_shell *shell);
reach_result reach_shell_refresh_tray_items(reach_shell *shell);

void reach_shell_compute_tray_popup_layout(reach_shell *shell,
                                           const reach_dock_layout *dock_layout,
                                           reach_rect_f32 *out_bounds,
                                           reach_rect_f32 *out_item_slots);

reach_result reach_shell_execute_tray_action(reach_shell *shell,
                                             reach_tray_feature_action action);

/* Context-menu orchestration */

void reach_shell_close_context_menu(reach_shell *shell);
reach_result reach_shell_execute_context_command(reach_shell *shell,
                                                 uint32_t command);
reach_result reach_shell_show_power_context_menu(reach_shell *shell);

reach_result reach_shell_show_dock_app_context_menu(reach_shell *shell,
                                                    size_t item_index,
                                                    int32_t x, int32_t y);

/* Dock model/sync/orchestration */

reach_result reach_shell_load_pinned_icons(reach_shell *shell);
int32_t reach_shell_dock_icon_size_px(const reach_shell *shell);

void reach_shell_release_render_icon(reach_shell *shell, uint64_t icon_id);
void reach_shell_release_icon_handle(reach_shell *shell,
                                     reach_icon_handle *icon);
reach_result reach_shell_load_icon_handle(reach_shell *shell,
                                          const uint16_t *path, int32_t size_px,
                                          reach_icon_handle *out_icon);
void reach_shell_release_tray_render_icons(reach_shell *shell);
void reach_shell_release_dock_icons(reach_shell *shell);
void reach_shell_release_open_window_icons(reach_shell *shell,
                                           size_t old_count);
void reach_shell_load_open_window_icons(reach_shell *shell);
void reach_shell_release_quick_settings_audio_render_icons(reach_shell *shell);
reach_result reach_shell_reload_pins(reach_shell *shell);
reach_result reach_shell_refresh_open_windows(reach_shell *shell,
                                              int32_t *out_changed);

int32_t reach_shell_window_is_minimized(const reach_shell *shell,
                                        uintptr_t window_id);

void reach_shell_build_dock_items(reach_shell *shell,
                                  reach_dock_layout *layout);

float reach_shell_dock_slot_box_x(const reach_shell *shell,
                                  const reach_dock_layout *layout,
                                  size_t index);

float reach_shell_dock_drag_clamped_x(const reach_shell *shell,
                                      const reach_dock_layout *layout,
                                      int32_t cursor_x);

size_t reach_shell_dock_reorder_target(const reach_shell *shell,
                                       size_t current_index,
                                       float dragged_box_x);

size_t reach_shell_find_dock_item_key(const reach_shell *shell, int32_t pinned,
                                      uint32_t pin_id, uintptr_t window);

size_t reach_shell_find_dock_order_key(const reach_shell *shell, int32_t pinned,
                                       uint32_t pin_id, uintptr_t window);

void reach_shell_move_dock_order(reach_shell *shell, size_t source,
                                 size_t target);

uint32_t reach_shell_dock_item_pin_id(const reach_shell *shell, size_t index);

int32_t reach_shell_dock_item_matches_key(const reach_shell *shell,
                                          size_t index, int32_t pinned,
                                          uint32_t pin_id, uintptr_t window);

size_t reach_shell_pinned_order_index(const reach_shell *shell,
                                      uint32_t pin_id);

float reach_shell_dock_item_current_x(const reach_shell *shell,
                                      const reach_dock_layout *layout,
                                      size_t index);

void reach_shell_rebuild_dock_items_with_animations(reach_shell *shell,
                                                    reach_dock_layout *layout);

int32_t reach_shell_should_auto_hide_dock(const reach_shell *shell);

reach_rect_f32 reach_shell_apply_dock_animation(reach_shell *shell,
                                                reach_rect_f32 shown_bounds,
                                                reach_rect_f32 monitor_bounds,
                                                double delta_seconds);

void reach_shell_apply_dock_width_animation(reach_shell *shell,
                                            reach_dock_layout *layout,
                                            double delta_seconds);

void reach_shell_sync_dock_reveal_edge(reach_shell *shell,
                                       reach_rect_f32 shown_dock_bounds,
                                       reach_rect_f32 monitor_bounds);

const uint16_t *reach_shell_dock_item_path(const reach_shell *shell,
                                           size_t item_index);

reach_result reach_shell_launch_dock_item(reach_shell *shell, size_t item_index,
                                          int32_t force_new_instance);

reach_result
reach_shell_execute_dock_item_action(reach_shell *shell,
                                     reach_dock_item_action action);

/* Dock drag orchestration */

void reach_shell_begin_dock_drag(reach_shell *shell, size_t index,
                                 const reach_ui_event *event);

reach_result reach_shell_update_dock_drag(reach_shell *shell,
                                          const reach_ui_event *event);

reach_result reach_shell_end_dock_drag(reach_shell *shell);

/* Dock/tray feedback */

void reach_shell_press_dock_item(reach_shell *shell, size_t index);
void reach_shell_stick_dock_item(reach_shell *shell);
void reach_shell_release_dock_item(reach_shell *shell);

void reach_shell_clear_sticky_dock_feedback(reach_shell *shell);

void reach_shell_press_tray_button(reach_shell *shell);
void reach_shell_press_quick_settings_button(reach_shell *shell);
void reach_shell_press_power_button(reach_shell *shell);

void reach_shell_press_tray_item(reach_shell *shell, size_t index);
void reach_shell_release_tray_item(reach_shell *shell);

void reach_shell_set_dock_click_feedback_immediate(reach_shell *shell,
                                                   size_t index, float opacity);

/* Quick-settings orchestration */

void reach_shell_set_quick_settings_open(reach_shell *shell, int32_t open);
void reach_shell_toggle_quick_settings(reach_shell *shell);

void reach_shell_refresh_quick_settings_audio(reach_shell *shell);
void reach_shell_refresh_quick_settings_system(reach_shell *shell);
void reach_shell_process_quick_settings_system_changes(reach_shell *shell);
void reach_shell_refresh_quick_settings_layout(reach_shell *shell);

void reach_shell_update_quick_settings_animation(reach_shell *shell,
                                                 double delta_seconds);

void reach_shell_execute_quick_settings_action(
    reach_shell *shell, reach_quick_settings_action action);

void reach_shell_on_system_controls_changed(void *user, uint32_t change_flags);

void reach_shell_end_quick_settings_drag(reach_shell *shell);

reach_result
reach_shell_begin_quick_settings_drag_if_hit(reach_shell *shell,
                                             const reach_ui_event *event);

reach_result
reach_shell_update_quick_settings_drag(reach_shell *shell,
                                       const reach_ui_event *event);

/* Switcher orchestration */

size_t reach_shell_switcher_visible_count(const reach_shell *shell);
void reach_shell_update_switcher_visible_start(reach_shell *shell);

reach_result reach_shell_handle_switcher_event(reach_shell *shell,
                                               const reach_ui_event *event);

/* Config and wallpaper */

reach_result reach_shell_reload_config(reach_shell *shell);

void reach_shell_seed_or_apply_wallpaper(reach_shell *shell,
                                         reach_config_snapshot *snapshot);

void reach_shell_reload_wallpaper(reach_shell *shell, int32_t force);

/* Surface rendering */

reach_result reach_shell_render_dock_surface(reach_shell *shell,
                                             const reach_dock_layout *layout);

reach_result reach_shell_render_tray_surface(reach_shell *shell,
                                             reach_rect_f32 bounds);

reach_result reach_shell_render_quick_settings_surface(reach_shell *shell);

reach_result reach_shell_render_switcher_surface(reach_shell *shell,
                                                 reach_rect_f32 bounds);

reach_result
reach_shell_render_launcher_surface(reach_shell *shell,
                                    const reach_launcher_layout *layout);

reach_result reach_shell_render_context_menu_surface(reach_shell *shell);
void reach_shell_on_window_event(void *user, const reach_ui_event *event);
void reach_shell_request_update(reach_shell *shell);
int32_t reach_shell_game_mode_enabled(const reach_shell *shell);
reach_result reach_shell_update_game_mode(reach_shell *shell);
#endif
