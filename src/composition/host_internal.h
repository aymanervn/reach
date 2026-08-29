#ifndef REACH_HOST_INTERNAL_H
#define REACH_HOST_INTERNAL_H

#include "reach/composition/host.h"

#include "reach/core/render_commands.h"
#include "reach/core/ui_events.h"
#include "reach/core/ui_layout.h"

#include "reach/features/common/layout.h"
#include "reach/features/common/text_edit.h"
#include "reach/features/context_menu.h"
#include "reach/features/feature_capsule.h"
#include "reach/features/clipboard.h"
#include "reach/features/dock.h"
#include "reach/features/launcher.h"
#include "reach/services/pin_config.h"
#include "reach/features/popup.h"
#include "reach/features/battery.h"
#include "reach/features/quick_settings.h"
#include "reach/features/stage.h"
#include "reach/features/switcher.h"
#include "reach/features/system_hud.h"
#include "reach/features/top_bar.h"
#include "reach/features/wallpaper.h"

#include "reach/core/runtime_policy.h"
#include "reach/composition/surface_runtime.h"
#include "reach/services/app_control.h"
#include "reach/services/config.h"
#include "reach/services/icon_service.h"
#include "reach/services/idle_watch.h"
#include "reach/services/now_playing.h"
#include "reach/services/search.h"
#include "reach/services/tray.h"
#include "reach/services/clock.h"
#include "reach/services/input_language.h"
#include "reach/services/system_stats.h"
#include "reach/services/system_status.h"
#include "reach/services/window_tracking.h"
#include "reach/support/animation.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#define REACH_HOST_MAX_ITEM_WINDOWS 16
#define REACH_SURFACE_NATIVE_OVERLAY_CAPACITY (REACH_MAX_OPEN_WINDOWS + 1)

typedef enum reach_host_animation_id
{
    REACH_HOST_ANIMATION_LAUNCHER_TRANSITION_Y = 0,
    REACH_HOST_ANIMATION_LAUNCHER_TRANSITION_OPACITY,
    REACH_HOST_ANIMATION_LAUNCHER_TRANSITION_SCALE,
    REACH_HOST_ANIMATION_TRAY_TRANSITION_Y,
    REACH_HOST_ANIMATION_TRAY_TRANSITION_OPACITY,
    REACH_HOST_ANIMATION_QUICK_SETTINGS_TRANSITION_Y,
    REACH_HOST_ANIMATION_QUICK_SETTINGS_TRANSITION_OPACITY,
    REACH_HOST_ANIMATION_BATTERY_TRANSITION_Y,
    REACH_HOST_ANIMATION_BATTERY_TRANSITION_OPACITY,
    REACH_HOST_ANIMATION_SWITCHER_TRANSITION_Y,
    REACH_HOST_ANIMATION_SWITCHER_TRANSITION_OPACITY,
    REACH_HOST_ANIMATION_CONTEXT_MENU_TRANSITION_Y,
    REACH_HOST_ANIMATION_CONTEXT_MENU_TRANSITION_OPACITY,
    REACH_HOST_ANIMATION_CLIPBOARD_TRANSITION_Y,
    REACH_HOST_ANIMATION_CLIPBOARD_TRANSITION_OPACITY,
    REACH_HOST_ANIMATION_STAGE_TRANSITION_Y,
    REACH_HOST_ANIMATION_STAGE_TRANSITION_OPACITY,
    REACH_HOST_ANIMATION_COUNT
} reach_host_animation_id;

static const float REACH_HOST_TRANSITION_SETTLE_FROM_BELOW = 8.0f;
static const float REACH_HOST_TRANSITION_SETTLE_FROM_ABOVE = -8.0f;
static const float REACH_HOST_LAUNCHER_TRANSITION_SCALE = 1.08f;

typedef struct reach_host_surface_transition
{
    int32_t visible;
    int32_t target_open;
    size_t y_track;
    size_t opacity_track;
    size_t scale_track;
    float settle_offset;
    float start_scale;
    double open_seconds;
    double close_seconds;
} reach_host_surface_transition;

typedef struct reach_host_surface_transition_frame
{
    reach_rect_f32 window_bounds;
    reach_rect_f32 content_rect;
    reach_transform_f32 render_transform;
    reach_transform_f32 pointer_transform;
    float scale;
    int32_t scale_envelope_active;
} reach_host_surface_transition_frame;

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
    REACH_SURFACE_ID_TOP_BAR,
    REACH_SURFACE_ID_LAUNCHER,
    REACH_SURFACE_ID_CLIPBOARD,
    REACH_SURFACE_ID_TRAY,
    REACH_SURFACE_ID_QUICK_SETTINGS,
    REACH_SURFACE_ID_CONTEXT_MENU,
    REACH_SURFACE_ID_SWITCHER,
    REACH_SURFACE_ID_STAGE,
    REACH_SURFACE_ID_BATTERY,
    REACH_SURFACE_ID_SYSTEM_HUD,
    REACH_HOST_SURFACE_COUNT
} reach_surface_id;

typedef enum reach_surface_shadow
{
    REACH_SURFACE_SHADOW_NONE = 0,
    REACH_SURFACE_SHADOW_BAR = 1,
    REACH_SURFACE_SHADOW_POPUP = 2
} reach_surface_shadow;

typedef enum reach_surface_pointer_flags
{
    REACH_SURFACE_POINTER_NONE = 0,
    REACH_SURFACE_POINTER_RELAYOUT_REDRAWS = 1u << 0,
    REACH_SURFACE_POINTER_SOURCE_GATED = 1u << 1,

    REACH_SURFACE_POINTER_DOWN_CLOSES_ON_UNHANDLED = 1u << 2,

    REACH_SURFACE_POINTER_DOWN_APPLIES_UNHANDLED = 1u << 3,

    REACH_SURFACE_POINTER_EXCLUSIVE_WHILE_OPEN = 1u << 4,

    REACH_SURFACE_POINTER_CAPTURE_CONSUMES_RELEASE = 1u << 5,

    REACH_SURFACE_POINTER_CAPTURE_OWNS_MOVE = 1u << 6
} reach_surface_pointer_flags;

typedef enum reach_surface_behavior_flags
{
    REACH_SURFACE_BEHAVIOR_NONE = 0,

    REACH_SURFACE_BEHAVIOR_ACTIVATES = 1u << 0,

    REACH_SURFACE_BEHAVIOR_EXCLUSIVE = 1u << 1,
    REACH_SURFACE_BEHAVIOR_GAME_MODE_VISIBLE = 1u << 2
} reach_surface_behavior_flags;

typedef enum reach_edge_reveal_anchor
{
    REACH_EDGE_REVEAL_ANCHOR_MANAGED = 0,
    REACH_EDGE_REVEAL_ANCHOR_TOP_LEFT,
    REACH_EDGE_REVEAL_ANCHOR_TOP,
    REACH_EDGE_REVEAL_ANCHOR_TOP_RIGHT,
    REACH_EDGE_REVEAL_ANCHOR_RIGHT,
    REACH_EDGE_REVEAL_ANCHOR_BOTTOM_RIGHT,
    REACH_EDGE_REVEAL_ANCHOR_BOTTOM,
    REACH_EDGE_REVEAL_ANCHOR_BOTTOM_LEFT,
    REACH_EDGE_REVEAL_ANCHOR_LEFT
} reach_edge_reveal_anchor;

typedef struct reach_edge_reveal_geometry
{
    reach_edge_reveal_anchor anchor;
    float width_dp;
    float height_dp;
    int32_t visible;
} reach_edge_reveal_geometry;

typedef struct reach_surface_edge_reveal_spec
{
    int32_t enabled;
    int32_t layer;
    reach_edge_reveal_geometry geometry;
    void (*handle_event)(reach_host *host, reach_screen_hotspot_event event);
} reach_surface_edge_reveal_spec;

typedef struct reach_surface_bar_reveal_spec
{
    const reach_bar_reveal_ops *ops;
    int32_t active_layer;
    float span_start_inset_dp;
} reach_surface_bar_reveal_spec;

typedef struct reach_feature_factory
{
    reach_result (*create)(void **out_capsule);
    void (*destroy)(void *capsule);
} reach_feature_factory;

typedef struct reach_feature_surface_context
{
    const reach_theme *theme;
    reach_rect_f32 monitor_bounds;
    reach_rect_f32 anchor_bounds;
    reach_rect_f32 last_bounds;
    reach_rect_f32 visible_bounds;
    reach_rect_f32 render_bounds;
    reach_rect_f32 content_rect;
    reach_transform_f32 render_transform;
    reach_text_measure_port text_measure;
    float dpi_scale;
    int32_t icon_size_px;
    int32_t transition_visible;
    int32_t bounds_valid;
    reach_rect_f32 anchor_button;
    float anchor_bar_edge_y;
    float anchor_bar_height;
    int32_t anchor_direction;
    int32_t anchor_valid;
    int32_t content_transform_active;
} reach_feature_surface_context;

typedef struct reach_feature_layout_anchor
{
    reach_surface_id surface;
    uint32_t slot;
    size_t index;
} reach_feature_layout_anchor;

typedef struct reach_feature_native_overlay_item
{
    reach_window_id source;
    reach_window_thumbnail_placement placement;
} reach_feature_native_overlay_item;

typedef struct reach_feature_native_overlay_ops
{
    size_t (*generation)(const void *capsule);
    size_t (*count)(const void *capsule);
    reach_result (*item)(const void *capsule, size_t index, reach_feature_native_overlay_item *out);
} reach_feature_native_overlay_ops;

typedef struct reach_feature_surface_ops
{
    int32_t (*arrange)(void *capsule, const reach_feature_surface_context *ctx);
    reach_result (*append_render_commands)(void *capsule, const reach_feature_surface_context *ctx,
                                           reach_render_command_buffer *out_commands);
    int32_t (*layout_anchor)(const void *capsule, reach_feature_layout_anchor *out);
    void (*set_pointer_transform)(void *capsule, reach_transform_f32 transform);
    const reach_feature_native_overlay_ops *native_overlay;
} reach_feature_surface_ops;

typedef struct reach_surface_spec
{
    reach_surface_class cls;
    uint32_t pointer_flags;
    reach_surface_shadow shadow;
    uint32_t behavior_flags;
    int32_t layer;
    reach_surface_role role;
    int32_t pointer_priority;
    int32_t has_transition;
    int32_t scale_in_envelope;
    int32_t popup_chrome;
    int32_t bar_shown_while_open;
    reach_surface_edge_reveal_spec edge_reveal;
    reach_surface_bar_reveal_spec bar_reveal;
} reach_surface_spec;

typedef struct reach_layout_spec
{
    reach_surface_id anchor;
    uint32_t anchor_slot;
    int32_t priority;
} reach_layout_spec;

typedef struct reach_feature_anchor
{
    reach_rect_f32 button;
    float bar_edge_y;
    float bar_height;
    int32_t direction;
} reach_feature_anchor;

typedef struct reach_feature_definition
{
    reach_surface_id id;
    reach_feature_factory factory;
    const reach_feature_capsule_ops *capsule_ops;
    const reach_feature_surface_ops *surface_ops;
    int32_t (*resolve_anchor)(const void *capsule, uint32_t slot, size_t index,
                              reach_feature_anchor *out);
    reach_surface_spec surface;
    reach_layout_spec layout;

    void (*force_close)(reach_host *host);
    void (*dismiss)(reach_host *host);
    const reach_ui_event_type *toggle_events;
    size_t toggle_event_count;
    void (*toggle)(reach_host *host);
    const reach_ui_event_type *routed_events;
    size_t routed_event_count;
    reach_result (*handle_routed)(reach_host *host, const reach_ui_event *event);
} reach_feature_definition;

typedef struct reach_feature_runtime
{
    reach_surface_runtime *surface;
    reach_host_surface_transition *transition;
    void *capsule;
    reach_rect_f32 resolved_bounds;
    int32_t resolved_bounds_valid;
    reach_window_thumbnail_id native_overlay_ids[REACH_SURFACE_NATIVE_OVERLAY_CAPACITY];
    size_t native_overlay_generation;
    int32_t native_overlay_registered;
    const reach_feature_definition *definition;
} reach_feature_runtime;

typedef struct reach_host_edge_reveal_runtime
{
    reach_host *host;
    const reach_feature_runtime *owner;
    reach_screen_hotspot_port port;
    reach_layout_participant participant;
    int32_t bounds_valid;
    reach_rect_f32 bounds;
} reach_host_edge_reveal_runtime;

typedef struct reach_host_window_manipulation_state
{
    reach_window_manipulation manual;
    reach_window_manipulation programmatic;
    int32_t manual_relevant;
    int32_t programmatic_relevant;
    reach_window_id active_window;
    int32_t relevant;
} reach_host_window_manipulation_state;

typedef struct reach_host_layout_target
{
    const reach_feature_runtime *runtime;
    reach_screen_hotspot_port *edge_reveal;
} reach_host_layout_target;

void reach_host_init_feature_registry(reach_host *host);
reach_result reach_host_apply_feature_action(reach_host *host, const reach_feature_runtime *runtime,
                                             const reach_capsule_pointer_result *result);
reach_result reach_host_open_launcher_result_and_close_transients(reach_host *host);
void reach_host_bind_interfeature_routes(reach_host *host);
void reach_host_clear_interfeature_routes(reach_host *host);
reach_result reach_host_create_registered_features(reach_host *host);
void reach_host_destroy_registered_features(reach_host *host);

void reach_host_init_layout(reach_host *host);
void reach_host_apply_layout(reach_host *host);
void reach_host_invalidate_surface_z_order(reach_host *host, reach_surface_id id);
void reach_host_hide_all_surfaces(reach_host *host);
reach_host_edge_reveal_runtime *reach_host_edge_reveal_for_surface(reach_host *host,
                                                                   reach_surface_id id);
reach_result reach_host_create_edge_reveals(reach_host *host);
reach_result reach_host_start_edge_reveals(reach_host *host);
void reach_host_destroy_edge_reveals(reach_host *host);
void reach_host_sync_edge_reveals(reach_host *host, reach_rect_f32 monitor_bounds);
void reach_host_set_edge_reveal_bounds(reach_host_edge_reveal_runtime *runtime,
                                       reach_rect_f32 bounds);
void reach_host_set_edge_reveal_visible(reach_host *host, reach_host_edge_reveal_runtime *runtime,
                                        int32_t visible);

int32_t reach_host_surface_is_open(const reach_feature_runtime *runtime);
int32_t reach_host_surface_needs_frame(const reach_feature_runtime *runtime);
int32_t reach_host_surface_presented(const reach_feature_runtime *runtime);
void reach_host_close_activating_surfaces_on_focus_loss(reach_host *host);
int32_t reach_host_any_surface_open(reach_host *host, uint32_t class_mask);
int32_t reach_host_any_surface_dirty(const reach_host *host);
void reach_host_mark_all_surfaces_dirty(reach_host *host);

#define REACH_SURFACE_ORIGIN_NONE REACH_HOST_SURFACE_COUNT

void reach_host_surface_opening(reach_host *host, reach_surface_id opening,
                                reach_surface_id origin);

struct reach_host_frame_context
{
    reach_rect_f32 monitor_bounds;
};
typedef struct reach_host_frame_context reach_host_frame_context;

void reach_host_sync_surface_input_regions(const reach_host *host,
                                           const reach_feature_runtime *runtime);

reach_result reach_host_frame_registered_surface(reach_host *host, reach_feature_runtime *runtime,
                                                 const reach_host_frame_context *ctx);
reach_result reach_host_redraw_registered_surface(reach_host *host, reach_surface_id id);

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
    int32_t z_order;
    int32_t render;
    int32_t update_requested;
    int32_t events_dispatched_this_cycle;
    int32_t monitors;
} reach_host_dirty_state;

typedef struct reach_host_window_list_state
{
    int32_t dwell_active;
    size_t dwell_item;
    double dwell_seconds;
    size_t open_item;
    double grace_seconds;
} reach_host_window_list_state;

typedef struct reach_host_pointer_move_state
{
    int32_t subscriptions_initialized;
    int32_t move_enabled[REACH_HOST_SURFACE_COUNT];
} reach_host_pointer_move_state;

struct reach_host
{
    reach_monitor_port monitors;

    reach_dock_model dock_config;

    reach_pinned_app_model pinned_apps[REACH_MAX_PINNED_APPS];
    size_t pinned_app_count;
    reach_surface_runtime launcher;
    reach_surface_runtime dock;
    reach_surface_runtime top_bar;
    reach_screen_hotspot_factory_port screen_hotspots;
    reach_image_loader_port image_loader;
    reach_surface_runtime tray;
    reach_surface_runtime switcher;
    reach_surface_runtime stage;
    reach_window_thumbnail_port window_thumbnails;
    reach_surface_runtime context_menu;
    reach_surface_runtime quick_settings;
    reach_surface_runtime battery;
    reach_surface_runtime system_hud;
    reach_surface_runtime clipboard_surface;
    reach_render_command_buffer render_commands;
    reach_render_command_buffer popup_render_commands;
    reach_host_surface_transition launcher_transition;
    int32_t launcher_restore_pending;
    reach_host_surface_transition tray_transition;
    reach_host_surface_transition switcher_transition;
    reach_host_surface_transition stage_transition;
    reach_host_surface_transition context_menu_transition;
    reach_host_surface_transition quick_settings_transition;
    reach_host_surface_transition battery_transition;
    reach_host_surface_transition clipboard_transition;

    reach_feature_definition feature_definitions[REACH_HOST_SURFACE_COUNT];
    reach_feature_runtime feature_runtimes[REACH_HOST_SURFACE_COUNT];
    reach_host_edge_reveal_runtime edge_reveals[REACH_HOST_SURFACE_COUNT];
    reach_layout layout_manager;
    reach_host_layout_target layout_targets[REACH_LAYOUT_MAX_PARTICIPANTS];
    reach_layout_participant surface_participants[REACH_HOST_SURFACE_COUNT];
    reach_layout_plan applied_layout_plan;
    int32_t has_applied_layout_plan;
    reach_input_source_port input_source;
    reach_clock *clock;
    reach_input_language_service *input_language;
    reach_window_manager_port window_manager;
    reach_foreground_watcher_port foreground_watcher;
    reach_config_store_port config_store;
    reach_tray_service *tray_service;
    reach_search_provider_port search_provider;
    reach_app_launcher_port app_launcher;
    reach_terminal_launcher_port terminal_launcher;
    reach_settings_launcher_port settings_launcher;
    reach_icon_service *icon_service;
    reach_explorer_service_port explorer_service;
    reach_wallpaper_service_port wallpaper_service;
    reach_wallpaper_surface_port wallpaper_surface;
    reach_power_session_port power_session;
    reach_idle_watch *idle_watch;
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
    int32_t top_bar_hidden;
    reach_host_window_manipulation_state window_manipulation;
    reach_host_pointer_move_state pointer_move;
    reach_host_window_list_state window_list;
    reach_clipboard_port clipboard;
    reach_search_service *search_service;
    reach_app_control *app_control;
    reach_host_deferred_launch deferred_launch;
    int32_t running;
    reach_runtime_policy_state runtime_policy;
    reach_audio_volume_port audio_volume;
    reach_system_controls_port system_controls;
    reach_media_controls_port media_controls;
    reach_now_playing_service *now_playing_service;

    double popup_hook_reassert_seconds;
    int32_t high_refresh_rate;
    int32_t bundled_font;
    std::atomic<uint32_t> quick_settings_system_change_flags;
    std::atomic<uint32_t> audio_volume_changed;
    reach_system_status *system_status;
    reach_system_stats *system_stats;
    reach_popup_capture_port popup_capture;
};

template <typename Feature>
static inline Feature *reach_host_feature_capsule(reach_host *host, reach_surface_id id)
{
    return static_cast<Feature *>(host->feature_runtimes[id].capsule);
}

template <typename Feature>
static inline Feature *reach_host_feature_capsule(const reach_host *host, reach_surface_id id)
{
    return static_cast<Feature *>(host->feature_runtimes[id].capsule);
}

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

static inline int32_t reach_host_primary_monitor_bounds(const reach_host *host,
                                                        reach_rect_f32 *out_bounds)
{
    if (host == nullptr || out_bounds == nullptr)
    {
        return 0;
    }

    *out_bounds = {};

    if (host->monitors.list == nullptr || host->monitors.ops.primary == nullptr ||
        host->monitors.ops.count == nullptr || host->monitors.ops.count(host->monitors.list) == 0)
    {
        return 0;
    }

    const reach_monitor_info *monitor = host->monitors.ops.primary(host->monitors.list);
    if (monitor == nullptr)
    {
        return 0;
    }

    out_bounds->x = (float)monitor->bounds.left;
    out_bounds->y = (float)monitor->bounds.top;
    out_bounds->width = (float)(monitor->bounds.right - monitor->bounds.left);
    out_bounds->height = (float)(monitor->bounds.bottom - monitor->bounds.top);
    return out_bounds->width > 0.0f && out_bounds->height > 0.0f ? 1 : 0;
}

int32_t reach_host_rect_equal(reach_rect_f32 a, reach_rect_f32 b);
int32_t reach_host_scalar_equal(float a, float b);

const reach_shadow *reach_host_surface_shadow(const reach_host *host, reach_surface_id id);
reach_shadow_pad reach_host_surface_shadow_pad(const reach_host *host, reach_surface_id id);
void reach_host_stamp_surface_content(const reach_host *host, reach_surface_id id,
                                      reach_render_command_buffer *commands);
reach_result reach_host_apply_window_state(reach_platform_window_port *window,
                                           reach_rect_f32 bounds, reach_shadow_pad pad,
                                           float opacity, reach_rect_f32 *last_bounds,
                                           float *last_opacity, int32_t *bounds_valid,
                                           int32_t *opacity_valid, int32_t *out_changed);
void reach_host_surface_transition_init(reach_host *host, reach_host_surface_transition *transition,
                                        size_t y_track, size_t opacity_track, float settle_offset);
void reach_host_surface_transition_set_scale(reach_host *host,
                                             reach_host_surface_transition *transition,
                                             size_t scale_track, float start_scale);
void reach_host_surface_transition_set_settle_offset(reach_host *host,
                                                     reach_host_surface_transition *transition,
                                                     float settle_offset);
void reach_host_open_context_menu_transition(reach_host *host);
void reach_host_surface_transitions_init(reach_host *host);
void reach_host_surface_transition_set(reach_host *host, reach_host_surface_transition *transition,
                                       int32_t open);
reach_rect_f32 reach_host_surface_transition_bounds(const reach_host *host,
                                                    const reach_host_surface_transition *transition,
                                                    reach_rect_f32 target_bounds);
float reach_host_surface_transition_opacity(const reach_host *host,
                                            const reach_host_surface_transition *transition);
reach_host_surface_transition_frame reach_host_surface_transition_frame_compute(
    const reach_host *host, const reach_host_surface_transition *transition,
    reach_rect_f32 target_bounds, reach_shadow_pad shadow_pad);
reach_host_surface_transition_frame reach_host_surface_transition_frame_compute_in_envelope(
    const reach_host *host, const reach_host_surface_transition *transition,
    reach_rect_f32 target_bounds, reach_rect_f32 envelope_bounds, reach_shadow_pad shadow_pad);
int32_t reach_host_surface_transition_visible(const reach_host_surface_transition *transition);
int32_t reach_host_surface_transition_active(const reach_host *host,
                                             const reach_host_surface_transition *transition);
void reach_host_surface_transition_finish(reach_host *host,
                                          reach_host_surface_transition *transition);
void reach_host_finish_surface_transitions(reach_host *host);

reach_pointer_event reach_host_surface_pointer_event(const reach_feature_runtime *desc,
                                                     const reach_ui_event *event,
                                                     reach_pointer_event_kind kind);
int32_t reach_host_popup_owner_trigger(const reach_feature_runtime *popup,
                                       const reach_feature_runtime *source,
                                       const reach_capsule_pointer_result *source_result);

void reach_host_request_update(reach_host *host);
void reach_host_on_launcher_window_event(void *user, const reach_ui_event *event);
void reach_host_on_dock_window_event(void *user, const reach_ui_event *event);
void reach_host_on_top_bar_window_event(void *user, const reach_ui_event *event);
void reach_host_on_tray_window_event(void *user, const reach_ui_event *event);
void reach_host_on_switcher_window_event(void *user, const reach_ui_event *event);
void reach_host_on_context_menu_window_event(void *user, const reach_ui_event *event);
void reach_host_on_quick_settings_window_event(void *user, const reach_ui_event *event);
void reach_host_on_battery_window_event(void *user, const reach_ui_event *event);
void reach_host_on_system_hud_window_event(void *user, const reach_ui_event *event);
void reach_host_on_clipboard_window_event(void *user, const reach_ui_event *event);
void reach_host_on_stage_window_event(void *user, const reach_ui_event *event);

reach_result reach_host_render_popup_surface(reach_host *host, reach_surface_id surface_id,
                                             reach_surface_runtime *surface, reach_rect_f32 bounds,
                                             float notch_anchor_x, int32_t notch_side,
                                             const reach_render_command_buffer *content_commands);

void reach_host_sync_popup_mouse_hook(reach_host *host);
void reach_host_close_transient_surfaces(reach_host *host, int32_t restore_focus);
void reach_host_close_surface_classes(reach_host *host, uint32_t class_mask, int32_t restore_focus);

void reach_host_notify_launcher_search_ready(reach_host *host);
void reach_host_cleanup_closed_launcher(reach_host *host);

void reach_host_close_launcher(reach_host *host);
void reach_host_close_launcher_without_focus_restore(reach_host *host);
void reach_host_remember_launcher_restore_window(reach_host *host);

void reach_host_toggle_launcher(reach_host *host);

void reach_host_open_stage(reach_host *host);
void reach_host_sync_stage_window_states(reach_host *host);
void reach_host_on_stage_edge_reveal(reach_host *host, reach_screen_hotspot_event event);
void reach_host_close_stage(reach_host *host);
void reach_host_toggle_stage(reach_host *host);
void reach_host_clear_launcher_restore_window(reach_host *host);
void reach_host_restore_launcher_focus(reach_host *host);
void reach_host_request_launcher_focus_restore(reach_host *host);
void reach_host_flush_launcher_focus_restore(reach_host *host);
reach_result reach_host_open_launcher_result(reach_host *host);
reach_result reach_host_reveal_launcher_result(reach_host *host, size_t result_index);
reach_result reach_host_schedule_app_launch(reach_host *host,
                                            const reach_app_launch_request *request);
reach_result reach_host_schedule_reveal_path(reach_host *host, const uint16_t *path);
reach_result reach_host_launch_settings_app(reach_host *host);
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

void reach_host_release_clipboard_item(reach_host *host, const reach_clipboard_item *item);
void reach_host_clear_clipboard(reach_host *host);
void reach_host_set_clipboard_open(reach_host *host, int32_t open);
void reach_host_toggle_clipboard(reach_host *host);
void reach_host_process_clipboard_refresh(reach_host *host);
void reach_host_release_clipboard_items(reach_host *host);

void reach_host_set_tray_popup_open(reach_host *host, int32_t open);
void reach_host_toggle_tray_popup(reach_host *host);
reach_result reach_host_refresh_tray_items(reach_host *host);
reach_result reach_host_activate_tray_item(reach_host *host, uint32_t item_id,
                                           reach_tray_action action);

void reach_host_close_context_menu(reach_host *host);
reach_result reach_host_execute_context_command(reach_host *host, uint32_t command);
reach_result reach_host_show_power_context_menu(reach_host *host);

reach_result reach_host_show_dock_app_context_menu(reach_host *host, size_t item_index, int32_t x,
                                                   int32_t y);

void reach_host_dock_item_hovered(reach_host *host, size_t item_index);
void reach_host_window_list_update(reach_host *host, double delta_seconds);
int32_t reach_host_window_list_wants_frames(const reach_host *host);
reach_result reach_host_show_dock_window_list(reach_host *host, size_t item_index);
reach_result reach_host_close_window(reach_host *host, uintptr_t window_id);

int32_t reach_host_icon_size_px(const reach_host *host);

void reach_host_release_render_icon(reach_host *host, uint64_t icon_id);

void reach_host_drain_icon_evictions(reach_host *host);
void reach_host_drain_tray_retired_icons(reach_host *host);
void reach_host_release_tray_render_icons(reach_host *host);
void reach_host_release_quick_settings_audio_render_icons(reach_host *host);
void reach_host_drain_now_playing_retired_covers(reach_host *host);
reach_result reach_host_refresh_open_windows(reach_host *host, int32_t *out_changed);
void reach_host_note_foreground_window(reach_host *host, uintptr_t foreground_window);
void reach_host_apply_foreground_change(reach_host *host);

int32_t reach_host_window_is_minimized(const reach_host *host, uintptr_t window_id);
reach_result reach_host_schedule_window_control(reach_host *host,
                                                reach_window_control_action action,
                                                uintptr_t window_id);
reach_result reach_host_schedule_minimize_open_windows(reach_host *host);
reach_result reach_host_schedule_open_terminal(reach_host *host);
reach_result reach_host_schedule_terminal_command(reach_host *host, const uint16_t *command);
reach_result reach_host_schedule_window_controls(reach_host *host,
                                                 reach_window_control_action action,
                                                 const uintptr_t *window_ids, size_t window_count);
void reach_host_apply_window_control_result(reach_host *host);

reach_dock_build_context reach_host_dock_build_context(reach_host *host);

void reach_host_invalidate_bar_coverage(reach_host *host);
void reach_host_refresh_window_world(reach_host *host);
void reach_host_sync_window_manipulation(reach_host *host);
void reach_host_begin_programmatic_window_manipulation(reach_host *host, reach_window_id window);
void reach_host_end_programmatic_window_manipulation(reach_host *host);
int32_t reach_host_get_pointer_position(reach_host *host, reach_point_i32 *out_pointer);
void reach_host_sync_pointer_move_subscriptions(reach_host *host);
void reach_host_suspend_pointer_move_subscriptions(reach_host *host);
void reach_host_request_bar_visibility_update(reach_host *host);
void reach_host_sync_bar_layout_conditions(reach_host *host);

reach_rect_f32 reach_host_reconcile_bar_visibility(reach_host *host, reach_surface_id id,
                                                   reach_rect_f32 shown_bounds,
                                                   reach_rect_f32 monitor_bounds);
reach_result reach_host_cycle_input_language(reach_host *host);

reach_result reach_host_refresh_monitor_layout(reach_host *host);
int32_t reach_host_can_move_bars_without_redraw(const reach_host *host);
reach_result reach_host_move_bar_animation_frame(reach_host *host);

const uint16_t *reach_host_dock_item_path(const reach_host *host, size_t item_index);

reach_result reach_host_launch_dock_item(reach_host *host, size_t item_index,
                                         int32_t force_new_instance);

void reach_host_clear_sticky_dock_feedback(reach_host *host);

void reach_host_set_quick_settings_open(reach_host *host, int32_t open);
void reach_host_toggle_quick_settings(reach_host *host);
void reach_host_set_battery_open(reach_host *host, int32_t open);
void reach_host_toggle_battery(reach_host *host);
void reach_host_refresh_battery_power(reach_host *host);
void reach_host_sync_battery_saver_pending(reach_host *host);

void reach_host_process_quick_settings_changes(reach_host *host);

reach_result reach_host_execute_media_action(reach_host *host, reach_now_playing_action action);
reach_result reach_host_step_main_volume(reach_host *host, float delta);
reach_result reach_host_toggle_main_volume_mute(reach_host *host);
reach_result reach_host_step_brightness(reach_host *host, float delta);
reach_result reach_host_snap_foreground_window(reach_host *host, reach_split_mode mode);
void reach_host_relayout_quick_settings(reach_host *host, int32_t animate_height);

void reach_host_on_system_controls_changed(void *user, uint32_t change_flags);
void reach_host_on_audio_volume_changed(void *user);

void reach_host_refresh_switcher_windows(reach_host *host);

reach_result reach_host_handle_switcher_event(reach_host *host, const reach_ui_event *event);

reach_result reach_host_request_config_reload(reach_host *host);
reach_result reach_host_pin_app(reach_host *host, const reach_pinned_app_model *app);
reach_result reach_host_unpin_id(reach_host *host, uint32_t pin_id);
reach_result reach_host_move_pin(reach_host *host, uint32_t pin_id, size_t target_index);
int32_t reach_host_apply_config_update(reach_host *host);
void reach_host_stop_config_service(reach_host *host);
reach_result reach_host_apply_config_snapshot(reach_host *host,
                                              const reach_config_snapshot *snapshot,
                                              int32_t apply_pins, int32_t apply_wallpaper);
void reach_host_apply_power_config(reach_host *host, const reach_config_snapshot *snapshot);

void reach_host_seed_or_apply_wallpaper(reach_host *host, reach_config_snapshot *snapshot);

void reach_host_reload_wallpaper(reach_host *host, int32_t force);

void reach_host_apply_ui_font(reach_host *host, int32_t bundled_font);

void reach_host_apply_theme_mode(reach_host *host, int32_t light_theme);

void reach_host_apply_display_config(reach_host *host, const reach_config_snapshot *snapshot);

int32_t reach_host_game_mode_enabled(const reach_host *host);
reach_result reach_host_update_game_mode(reach_host *host);
#endif
