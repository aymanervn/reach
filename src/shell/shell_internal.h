#ifndef REACH_SHELL_INTERNAL_H
#define REACH_SHELL_INTERNAL_H

#include "reach/shell.h"

#include "reach/core/render_commands.h"
#include "reach/core/ui_events.h"
#include "reach/core/ui_layout.h"
#include "reach/animation.h"
#include "reach/features/context_menu.h"
#include "reach/features/dock.h"
#include "reach/features/launcher.h"
#include "reach/features/switcher.h"
#include "reach/features/tray.h"
#include "reach/features/wallpaper.h"
#include "reach/hotkeys.h"
#include "reach/monitor.h"
#include "reach/pin_config.h"
#include "reach/shell/surface_runtime.h"
#include "reach/theme.h"

struct reach_shell {
    reach_hotkeys *hotkeys;
    reach_monitor_list *monitors;
    reach_ui_state ui;
    reach_surface_runtime launcher;
    reach_surface_runtime dock;
    reach_surface_runtime tray;
    reach_surface_runtime switcher;
    reach_surface_runtime context_menu;
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
    const reach_theme *theme;
    reach_window_snapshot open_windows[REACH_MAX_PINNED_APPS];
    size_t open_window_count;
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
    int32_t layout_dirty;
    int32_t render_dirty;
    size_t hovered_dock_index;
    int32_t dock_animation_initialized;
    int32_t dock_animating;
    int32_t dock_target_hidden;
    int32_t dock_reveal_active;
    int32_t dock_width_animation_initialized;
    int32_t dock_width_animating;
    size_t dock_width_item_count;
    reach_float_animation dock_width_animation;
    size_t dock_click_feedback_index;
    int32_t dock_click_feedback_pressed;
    int32_t dock_click_feedback_sticky;
    int32_t dock_click_feedback_animating;
    reach_float_animation dock_click_feedback_opacity;
    size_t tray_click_feedback_index;
    int32_t tray_click_feedback_pressed;
    int32_t tray_click_feedback_animating;
    reach_float_animation tray_click_feedback_opacity;
    int32_t dock_drag_active;
    int32_t dock_drag_moved;
    size_t dock_drag_source_index;
    size_t dock_drag_target_index;
    int32_t dock_drag_pinned;
    uint32_t dock_drag_pin_id;
    uintptr_t dock_drag_window;
    int32_t dock_drag_start_x;
    int32_t dock_drag_start_y;
    float dock_drag_grab_offset_x;
    float dock_drag_x;
    int32_t dock_drag_snapping;
    int32_t dock_reload_pins_after_snap;
    reach_float_animation dock_drag_snap_animation;
    reach_float_animation dock_y_animation;
    double window_manager_refresh_elapsed;
    int32_t tray_popup_open;
    reach_tray_model tray_model;
    int32_t switcher_open;
    size_t switcher_selected_index;
    size_t switcher_visible_start;
    int32_t context_menu_open;
    size_t context_menu_target_index;
    reach_rect_f32 context_menu_bounds;
    reach_rect_f32 context_menu_item_slots[4];
    uint32_t context_menu_item_commands[4];
    size_t context_menu_item_count;
    size_t context_menu_hovered_index;
    int32_t running;
    uint16_t wallpaper_path[260];
    reach_popup_capture_port popup_capture;
};

static const size_t REACH_SHELL_DOCK_FEEDBACK_TRAY_BUTTON = REACH_MAX_PINNED_APPS;
static const size_t REACH_SHELL_DOCK_FEEDBACK_NONE = REACH_MAX_PINNED_APPS + 1;
static const size_t REACH_SHELL_SWITCHER_VISIBLE_MAX = 12;

int32_t reach_shell_rect_equal(reach_rect_f32 a, reach_rect_f32 b);
int32_t reach_shell_opacity_equal(float a, float b);
reach_result reach_shell_apply_window_state(
    reach_platform_window_port *window,
    reach_rect_f32 bounds,
    float opacity,
    reach_rect_f32 *last_bounds,
    float *last_opacity,
    int32_t *bounds_valid,
    int32_t *opacity_valid,
    int32_t *out_changed);

void reach_shell_raise_launcher(reach_shell *shell);
void reach_shell_set_tray_popup_open(reach_shell *shell, int32_t open);
void reach_shell_toggle_tray_popup(reach_shell *shell);
reach_result reach_shell_refresh_tray_items(reach_shell *shell);
void reach_shell_compute_tray_popup_layout(
    reach_shell *shell,
    const reach_dock_layout *dock_layout,
    reach_rect_f32 *out_bounds,
    reach_rect_f32 *out_item_slots);
void reach_shell_close_context_menu(reach_shell *shell);
void reach_shell_sync_popup_mouse_hook(reach_shell *shell);
void reach_shell_clear_sticky_dock_feedback(reach_shell *shell);

reach_result reach_shell_load_pinned_icons(reach_shell *shell);
reach_result reach_shell_reload_pins(reach_shell *shell);
void reach_shell_seed_or_apply_wallpaper(reach_shell *shell, reach_config_snapshot *snapshot);
void reach_shell_reload_wallpaper(reach_shell *shell, int32_t force);
reach_result reach_shell_refresh_open_windows(reach_shell *shell);
int32_t reach_shell_window_is_minimized(const reach_shell *shell, uintptr_t window_id);
void reach_shell_build_dock_items(reach_shell *shell, reach_dock_layout *layout);
float reach_shell_dock_slot_box_x(const reach_shell *shell, const reach_dock_layout *layout, size_t index);
float reach_shell_dock_drag_clamped_x(const reach_shell *shell, const reach_dock_layout *layout, int32_t cursor_x);
size_t reach_shell_dock_reorder_target(const reach_shell *shell, int32_t x);
size_t reach_shell_find_dock_item_key(const reach_shell *shell, int32_t pinned, uint32_t pin_id, uintptr_t window);
size_t reach_shell_find_dock_order_key(const reach_shell *shell, int32_t pinned, uint32_t pin_id, uintptr_t window);
void reach_shell_move_dock_order(reach_shell *shell, size_t source, size_t target);
uint32_t reach_shell_dock_item_pin_id(const reach_shell *shell, size_t index);
int32_t reach_shell_dock_item_matches_key(const reach_shell *shell, size_t index, int32_t pinned, uint32_t pin_id, uintptr_t window);
size_t reach_shell_pinned_order_index(const reach_shell *shell, uint32_t pin_id);
float reach_shell_dock_item_current_x(const reach_shell *shell, const reach_dock_layout *layout, size_t index);
void reach_shell_rebuild_dock_items_with_animations(reach_shell *shell, reach_dock_layout *layout);
int32_t reach_shell_should_auto_hide_dock(const reach_shell *shell);
reach_rect_f32 reach_shell_apply_dock_animation(reach_shell *shell, reach_rect_f32 shown_bounds, reach_rect_f32 monitor_bounds, double delta_seconds);
void reach_shell_apply_dock_width_animation(reach_shell *shell, reach_dock_layout *layout, double delta_seconds);

reach_result reach_shell_render_dock_surface(reach_shell *shell, const reach_dock_layout *layout);
reach_result reach_shell_render_tray_surface(reach_shell *shell, reach_rect_f32 bounds);
size_t reach_shell_switcher_visible_count(const reach_shell *shell);
void reach_shell_update_switcher_visible_start(reach_shell *shell);
reach_result reach_shell_render_switcher_surface(reach_shell *shell, reach_rect_f32 bounds);
reach_result reach_shell_render_launcher_surface(reach_shell *shell, const reach_launcher_layout *layout);
reach_result reach_shell_render_context_menu_surface(reach_shell *shell);

void reach_shell_on_window_event(void *user, const reach_ui_event *event);

#endif
