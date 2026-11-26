#ifndef REACH_HOST_INTERNAL_H
#define REACH_HOST_INTERNAL_H

#include "reach/composition/host.h"

#include "reach/core/menu_commands.h"
#include "reach/core/render_commands.h"
#include "reach/core/ui_events.h"
#include "reach/core/ui_layout.h"

#include "reach/features/common/bar_visibility.h"
#include "reach/features/common/layout.h"
#include "reach/features/common/text_edit.h"
#include "reach/features/feature_capsule.h"
#include "reach/services/pin_config.h"
#include "reach/features/common/popup.h"

#include "reach/core/runtime_policy.h"
#include "reach/composition/surface_runtime.h"
#include "reach/services/app_control.h"
#include "reach/services/config.h"
#include "reach/services/icon_service.h"
#include "reach/services/idle_watch.h"
#include "reach/services/now_playing.h"
#include "reach/services/monitor_refresh_retry.h"
#include "reach/services/monitor_topology.h"
#include "reach/services/search.h"
#include "reach/services/tray.h"
#include "reach/services/clock.h"
#include "reach/services/input_language.h"
#include "reach/services/system_stats.h"
#include "reach/services/bluetooth.h"
#include "reach/services/profile.h"
#include "reach/services/system_status.h"
#include "reach/services/wifi.h"
#include "reach/services/window_tracking.h"
#include "reach/services/wallpaper.h"
#include "reach/support/motion.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#define REACH_HOST_MAX_ITEM_WINDOWS 16
#define REACH_SURFACE_NATIVE_OVERLAY_CAPACITY (REACH_MAX_OPEN_WINDOWS + 1)

typedef struct reach_host_surface_presentation_frame
{
    reach_rect_f32 window_bounds;
    reach_rect_f32 content_rect;
    reach_transform_f32 render_transform;
} reach_host_surface_presentation_frame;

typedef enum reach_surface_class
{
    REACH_SURFACE_CLASS_PERSISTENT = 0,
    REACH_SURFACE_CLASS_TRANSIENT = 1,
    REACH_SURFACE_CLASS_POPUP = 2,
    REACH_SURFACE_CLASS_OVERLAY = 3
} reach_surface_class;

using reach_surface_id = uint32_t;

constexpr reach_surface_id REACH_SURFACE_ID_INVALID = UINT32_MAX;
constexpr size_t REACH_HOST_SURFACE_CAPACITY = 24;

// A definition and its references use the address of the same static key object.
struct reach_surface_key
{
};

// A route can share a key with the registry using the feature type or a feature-owned tag.
template <typename Tag> const reach_surface_key &reach_surface_key_for()
{
    // Writable storage keeps distinct keys from being folded together by the linker.
    static reach_surface_key key;
    return key;
}

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

    REACH_SURFACE_POINTER_CAPTURE_OWNS_MOVE = 1u << 6,
    REACH_SURFACE_POINTER_CANCEL_BEFORE_DOWN = 1u << 7
} reach_surface_pointer_flags;

typedef enum reach_surface_behavior_flags
{
    REACH_SURFACE_BEHAVIOR_NONE = 0,

    REACH_SURFACE_BEHAVIOR_EXCLUSIVE = 1u << 0,
    REACH_SURFACE_BEHAVIOR_GAME_MODE_VISIBLE = 1u << 1,
    REACH_SURFACE_BEHAVIOR_INPUT_PASSTHROUGH = 1u << 2,
    REACH_SURFACE_BEHAVIOR_DISMISS_ON_ESCAPE = 1u << 3
} reach_surface_behavior_flags;

enum reach_surface_redraw_reason
{
    REACH_SURFACE_REDRAW_OPEN_WINDOWS = 1u << 0,
    REACH_SURFACE_REDRAW_FOREGROUND = 1u << 1,
    REACH_SURFACE_REDRAW_ACTIVATION_REQUESTED = 1u << 2,
    REACH_SURFACE_REDRAW_ACTIVATION_COMPLETED = 1u << 3,
    REACH_SURFACE_REDRAW_CLOCK = 1u << 4,
    REACH_SURFACE_REDRAW_INPUT_LANGUAGE = 1u << 5
};

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
    int32_t open_on_enter;
} reach_surface_edge_reveal_spec;

typedef struct reach_surface_bar_reveal_spec
{
    const reach_bar_reveal_ops *ops;
    int32_t active_layer;
    float span_start_inset_dp;
    float protected_clearance_dp;
} reach_surface_bar_reveal_spec;

typedef struct reach_feature_factory
{
    reach_result (*create)(void **out_capsule);
    void (*destroy)(void *capsule);
} reach_feature_factory;

typedef struct reach_feature_dependencies
{
    reach_search_service *search;
    reach_icon_service *icons;
    reach_window_tracking *windows;
    reach_now_playing_service *now_playing;
    reach_system_stats *system_stats;
    reach_clock *clock;
    reach_input_language_service *input_language;
    reach_tray_service *tray;
    reach_app_control *app_control;
    reach_system_status *system_status;
    reach_wifi_service *wifi;
    reach_bluetooth_service *bluetooth;
    reach_profile_service *profile;
    const reach_power_session_port *power_session;
    const reach_clipboard_port *clipboard;
    void (*request_update)(void *user);
    void *request_update_user;
    const uint16_t *terminal_icon_ref;
    const reach_screen_capture_port *screen_capture;
    reach_config_service *config;
} reach_feature_dependencies;

typedef struct reach_feature_lifecycle_ops
{
    // Each successful attach or start is paired with cleanup. A failing hook unwinds its own work.
    reach_result (*attach)(void *capsule, const reach_feature_dependencies *dependencies);
    void (*detach)(void *capsule);
    reach_result (*start)(void *capsule);
    // Without a start hook, the entry is ready to stop once its start phase is reached.
    void (*stop)(void *capsule);
} reach_feature_lifecycle_ops;

typedef enum reach_feature_notification_kind
{
    REACH_FEATURE_NOTIFICATION_SYSTEM_STATS_CHANGED = 1,
    REACH_FEATURE_NOTIFICATION_NOW_PLAYING_CHANGED = 2,
    REACH_FEATURE_NOTIFICATION_MEDIA_ACTION = 3,
    REACH_FEATURE_NOTIFICATION_MAIN_VOLUME = 4,
    REACH_FEATURE_NOTIFICATION_BRIGHTNESS = 5,
    REACH_FEATURE_NOTIFICATION_WINDOWS_CHANGED = 6,
    REACH_FEATURE_NOTIFICATION_PINNED_APPS_CHANGED = 7,
    REACH_FEATURE_NOTIFICATION_ICONS_RETAIN = 8,
    REACH_FEATURE_NOTIFICATION_DISPLAY_CHANGED = 9,
    REACH_FEATURE_NOTIFICATION_CONFIG_CHANGED = 10,
    REACH_FEATURE_NOTIFICATION_POPUPS_CLOSED = 11,
    REACH_FEATURE_NOTIFICATION_APP_LAUNCH_FAILED = 12
} reach_feature_notification_kind;

typedef struct reach_feature_notification
{
    reach_feature_notification_kind kind;
    reach_text_measure_port text_measure;
    float dpi_scale;
    reach_system_stats_snapshot system_stats;
    reach_now_playing_action media_action;
    reach_audio_volume_state volume;
    reach_brightness_state brightness;
    reach_window_tracking_refresh_report windows;
    const reach_pinned_app_model *pinned_apps;
    size_t pinned_app_count;
    int32_t icon_size_px;
    reach_display_environment display;
    const reach_config_snapshot *config;
    reach_app_launch_failure app_launch_failure;
    uint16_t app_name[REACH_APPLICATION_TEXT_CAPACITY];
} reach_feature_notification;

typedef struct reach_feature_render_resource
{
    uint64_t render_icon_id;
    uint64_t source_id;
    int32_t release_source;
} reach_feature_render_resource;

typedef struct reach_feature_render_resource_ops
{
    size_t (*take_retired)(void *capsule, reach_feature_render_resource *out, size_t cap);
    size_t (*active_count)(const void *capsule);
    int32_t (*active_at)(const void *capsule, size_t index, reach_feature_render_resource *out);
    void (*release_source)(void *capsule, const reach_feature_render_resource *resource);
    void (*clear_active)(void *capsule);
} reach_feature_render_resource_ops;

typedef struct reach_feature_control_ops
{
    int32_t (*set_open)(void *capsule, int32_t open, reach_feature_tick_result *out);
    void (*surface_hidden)(void *capsule, reach_feature_tick_result *out);
    void (*notify)(void *capsule, const reach_feature_notification *notification,
                   reach_feature_tick_result *out);
    int32_t (*blocks_position_only_frame)(const void *capsule);
    const reach_feature_render_resource_ops *render_resources;
} reach_feature_control_ops;

typedef struct reach_feature_surface_context
{
    const reach_theme *theme;
    uint32_t theme_revision;
    reach_rect_f32 monitor_bounds;
    reach_rect_f32 available_bounds;
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
    int32_t retained_supported;
    int32_t window_thumbnails_supported;
    int32_t content_transform_active;
    reach_window_id preferred_front_source;
} reach_feature_surface_context;

typedef enum reach_layout_reservation_edge
{
    REACH_LAYOUT_RESERVATION_NONE = 0,
    REACH_LAYOUT_RESERVATION_TOP = 1,
    REACH_LAYOUT_RESERVATION_BOTTOM = 2
} reach_layout_reservation_edge;

// One surface's share of a monitor's work area; the host merges every surface's claim so bars
// on different edges never overwrite each other's reservation.
typedef struct reach_host_work_area_claim
{
    int32_t active;
    reach_layout_reservation_edge edge;
    float edge_value;
    reach_rect_i32 monitor;
} reach_host_work_area_claim;

typedef struct reach_feature_layout_anchor
{
    reach_surface_id surface = REACH_SURFACE_ID_INVALID;
    uint32_t slot;
    size_t index;
    uint64_t identity;
} reach_feature_layout_anchor;

typedef enum reach_popup_activation_mode
{
    REACH_POPUP_ACTIVATION_TOGGLE = 0,
    REACH_POPUP_ACTIVATION_PASSIVE = 1,
    REACH_POPUP_ACTIVATION_REPLACE = 2
} reach_popup_activation_mode;

typedef enum reach_popup_activation_decision
{
    REACH_POPUP_ACTIVATION_NONE = 0,
    REACH_POPUP_ACTIVATION_PRESENT = 1,
    REACH_POPUP_ACTIVATION_CLOSE = 2
} reach_popup_activation_decision;

typedef struct reach_feature_native_overlay_item
{
    reach_window_id source;
    reach_window_thumbnail_plane plane;
    reach_window_thumbnail_placement placement;
    int32_t has_destination_motion;
    reach_rect_motion destination_motion;
} reach_feature_native_overlay_item;

typedef struct reach_feature_native_overlay_ops
{
    size_t (*generation)(const void *capsule);
    size_t (*count)(const void *capsule);
    reach_result (*item)(const void *capsule, size_t index, const reach_theme *theme,
                         reach_feature_native_overlay_item *out);
} reach_feature_native_overlay_ops;

typedef struct reach_feature_surface_ops
{
    int32_t (*arrange)(void *capsule, const reach_feature_surface_context *ctx);
    reach_result (*append_render_commands)(void *capsule, const reach_feature_surface_context *ctx,
                                           reach_render_command_buffer *out_commands);
    const reach_feature_native_overlay_ops *native_overlay;
    int32_t (*pointer_observation)(const void *capsule, reach_rect_union *out);
} reach_feature_surface_ops;

typedef struct reach_surface_spec
{
    reach_surface_class cls;
    uint32_t pointer_flags;
    reach_surface_shadow shadow;
    uint32_t behavior_flags;
    uint32_t redraw_on;
    int32_t layer;
    uint32_t window_flags = REACH_WINDOW_COMPOSITION_BACKED;
    int32_t pointer_priority;
    int32_t popup_chrome;
    int32_t bar_shown_while_open;
    int32_t restores_focus_on_close;
    int32_t close_on_persistent_press;
    int32_t refresh_world_on_open;

    /* A press on the control this surface hangs off must not dismiss it. By default that is its
       layout anchor; a surface with no anchor names the control explicitly. */
    const reach_surface_key *dismiss_guard_surface;
    uint32_t dismiss_guard_slot;
    int32_t dismiss_guard_any_control;
    const reach_surface_key *opening_origin;
    reach_surface_edge_reveal_spec edge_reveal;
    reach_surface_bar_reveal_spec bar_reveal;
} reach_surface_spec;

typedef struct reach_layout_spec
{
    const reach_surface_key *anchor;
    uint32_t anchor_slot;
    int32_t priority;
    reach_layout_reservation_edge reservation_edge;
    int32_t uses_reserved_bounds;
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
    reach_surface_id id = REACH_SURFACE_ID_INVALID;
    const reach_surface_key *key;
    const reach_surface_key *capsule_owner;
    reach_feature_factory factory;
    reach_feature_lifecycle_ops lifecycle;
    const reach_feature_capsule_ops *capsule_ops;
    const reach_feature_surface_ops *surface_ops;
    const reach_feature_control_ops *control_ops;
    int32_t (*resolve_anchor)(const void *capsule, uint32_t slot, size_t index, uint64_t identity,
                              reach_feature_anchor *out);
    reach_surface_spec surface;
    reach_layout_spec layout;

    const reach_ui_event_type *toggle_events;
    size_t toggle_event_count;
    // Open-only and close-only counterparts of toggle events. A close event that closes an open
    // surface is consumed.
    const reach_ui_event_type *open_events;
    size_t open_event_count;
    const reach_ui_event_type *close_events;
    size_t close_event_count;
    // Routed events and toggles are shell-wide regardless of which window emitted the event.
    const reach_ui_event_type *routed_events;
    size_t routed_event_count;
    // Window events reach only the entry bound to the originating platform window.
    // Events managed by the host, such as pointer input and focus loss, use its own dispatch.
    const reach_ui_event_type *window_events;
    size_t window_event_count;
    reach_ui_event_type refresh_world_before_event;
    reach_result (*commit_config)(void *capsule);
} reach_feature_definition;

// Registry-resolved IDs stay separate from the declarative keys.
struct reach_feature_links
{
    reach_surface_id capsule_owner = REACH_SURFACE_ID_INVALID;
    reach_surface_id layout_anchor = REACH_SURFACE_ID_INVALID;
    reach_surface_id dismiss_guard_surface = REACH_SURFACE_ID_INVALID;
    reach_surface_id opening_origin = REACH_SURFACE_ID_INVALID;
};

typedef struct reach_feature_runtime
{
    reach_surface_runtime *surface;
    void *capsule;
    const reach_feature_links *links;
    bool attached;
    bool started;
    reach_rect_f32 resolved_bounds;
    int32_t resolved_bounds_valid;
    int32_t bar_auto_hide_active;
    reach_host_work_area_claim work_area_claim;
    reach_window_thumbnail_id native_overlay_ids[REACH_SURFACE_NATIVE_OVERLAY_CAPACITY];
    reach_window_id native_overlay_sources[REACH_SURFACE_NATIVE_OVERLAY_CAPACITY];
    reach_window_thumbnail_plane native_overlay_planes[REACH_SURFACE_NATIVE_OVERLAY_CAPACITY];
    reach_window_id native_overlay_target;
    size_t native_overlay_generation;
    int32_t native_overlay_registered;
    reach_window_id native_overlay_front_source;
    reach_window_thumbnail_id native_overlay_front_id;
    int32_t presentation_visible;
    int32_t frame_context_valid;
    int32_t native_overlay_motion_pending;
    int32_t frame_retained_supported;
    reach_platform_window *frame_window;
    reach_window_id frame_native_target;
    reach_feature_surface_context layout_context;
    reach_feature_surface_context native_overlay_context;
    reach_feature_layout_anchor popup_owner;
    reach_feature_layout_anchor resolved_anchor_owner;
    reach_feature_anchor resolved_anchor;
    reach_rect_f32 resolved_anchor_bounds;
    int32_t resolved_anchor_valid;
    reach_feature_surface_geometry frame_geometry;
    reach_shadow_pad native_overlay_shadow_pad;
    reach_platform_window *cursor_region_window;
    reach_cursor_kind cursor_kind;
    reach_rect_f32 cursor_region;
    // When this surface's frame last failed, in reach_motion_now_seconds time; 0 when never.
    double frame_failure_seconds;
    const reach_feature_definition *definition;
} reach_feature_runtime;

typedef struct reach_host_surface_event_binding
{
    reach_host *host;
    reach_feature_runtime *runtime;
} reach_host_surface_event_binding;

struct reach_host_started_ports
{
    bool window_manager;
    bool foreground_watcher;
    bool system_controls;
    bool audio_volume;
    bool now_playing;
    bool wallpaper;
};

typedef struct reach_host_edge_reveal_runtime
{
    reach_host *host;
    const reach_feature_runtime *owner;
    reach_screen_hotspot_port port;
    reach_layout_participant participant;
    int32_t bounds_valid;
    int32_t enter_suppressed;
    reach_rect_f32 bounds;
    bool callback_bound;
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

reach_result reach_host_init_feature_registry(reach_host *host);
reach_result reach_host_set_feature_registry(reach_host *host,
                                             const reach_feature_definition *definitions,
                                             size_t count);
reach_surface_id reach_host_find_surface(const reach_host *host, const reach_surface_key &key);
reach_result reach_host_apply_feature_action(reach_host *host, reach_feature_runtime *runtime,
                                             const reach_capsule_action *action);
void reach_host_bind_interfeature_routes(reach_host *host);
void reach_host_clear_interfeature_routes(reach_host *host);
reach_result reach_host_create_registered_features(reach_host *host);
reach_result
reach_host_create_registered_surface_ports(reach_host *host,
                                           const reach_host_dependencies *dependencies);
void reach_host_destroy_registered_surface_ports(reach_host *host);
void reach_host_destroy_registered_features(reach_host *host);
void reach_host_stop_runtime(reach_host *host, bool stop_workers);
void reach_host_apply_feature_tick_result(reach_host *host, reach_feature_runtime *runtime,
                                          const reach_feature_tick_result *result);
typedef enum reach_surface_close_intent
{
    REACH_SURFACE_CLOSE_DISMISS = 0,
    REACH_SURFACE_CLOSE_SUPERSEDED = 1
} reach_surface_close_intent;

void reach_host_set_registered_surface_open(reach_host *host, reach_surface_id id, int32_t open);
void reach_host_close_registered_surface(reach_host *host, reach_surface_id id,
                                         reach_surface_close_intent intent);
void reach_host_set_pointer_observation(reach_host *host, reach_surface_id id,
                                        const reach_rect_union *shape);
void reach_host_close_surfaces_on_persistent_press(reach_host *host);
int32_t reach_host_layout_anchor_equal(const reach_feature_layout_anchor *left,
                                       const reach_feature_layout_anchor *right);
int32_t reach_host_layout_anchor_matches_control(const reach_feature_layout_anchor *anchor,
                                                 const reach_feature_control *control);
reach_popup_activation_decision
reach_host_popup_activation_decide(int32_t open, const reach_feature_layout_anchor *current,
                                   const reach_feature_layout_anchor *requested,
                                   reach_popup_activation_mode mode);
reach_popup_activation_decision
reach_host_prepare_registered_popup(reach_host *host, reach_surface_id id,
                                    const reach_feature_layout_anchor *owner,
                                    reach_popup_activation_mode mode);
void reach_host_toggle_registered_popup(reach_host *host, reach_surface_id id);
void reach_host_present_registered_popup(reach_host *host, reach_surface_id id,
                                         reach_surface_id origin);
int32_t reach_host_surface_closable(const reach_feature_runtime *runtime);
void reach_host_apply_surface_open_change(reach_host *host, reach_feature_runtime *runtime,
                                          int32_t open);
reach_result reach_host_dispatch_capsule_event(reach_host *host, reach_feature_runtime *runtime,
                                               const reach_ui_event *event, int32_t *out_handled);
void reach_host_capture_focus_restore(reach_host *host, reach_surface_id id);
void reach_host_arm_focus_restore(reach_host *host, reach_surface_id id);
void reach_host_cancel_focus_restore(reach_host *host, reach_surface_id id);
void reach_host_flush_focus_restore(reach_host *host, reach_surface_id id);
void reach_host_toggle_registered_surface(reach_host *host, reach_surface_id id);
reach_result reach_host_execute_menu_command(reach_host *host, uint32_t command);
void reach_host_post_feature_work_ready(reach_host *host);
void reach_host_post_event(reach_host *host, reach_ui_event_type event);
void reach_host_invalidate_surface_content(reach_host *host, reach_surface_redraw_reason reason);
void reach_host_notify_windows_changed(reach_host *host);
void reach_host_notify_display_changed(reach_host *host);
void reach_host_notify_config_changed(reach_host *host, const reach_config_snapshot *snapshot);
void reach_host_notify_popups_closed(reach_host *host);
void reach_host_notify_windows_refreshed(reach_host *host,
                                         const reach_window_tracking_refresh_report *report);
void reach_host_notify_pinned_apps_changed(reach_host *host);
void reach_host_notify_icons_retained(reach_host *host);
void reach_host_stop_search_service(reach_host *host);
void reach_host_notify_registered_features(reach_host *host,
                                           const reach_feature_notification *notification);

reach_result reach_host_init_layout(reach_host *host);
void reach_host_apply_layout(reach_host *host);
void reach_host_invalidate_surface_z_order(reach_host *host, reach_surface_id id);
void reach_host_hide_all_surfaces(reach_host *host);
reach_host_edge_reveal_runtime *reach_host_edge_reveal_for_surface(reach_host *host,
                                                                   reach_surface_id id);
reach_result reach_host_create_edge_reveals(reach_host *host);
reach_result reach_host_start_edge_reveals(reach_host *host);
void reach_host_stop_edge_reveals(reach_host *host);
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
void reach_host_render_device_recovered(reach_host *host);
void reach_host_invalidate_display_geometry(reach_host *host);

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
    reach_surface_id surface;
    uint32_t pin_id;
    uint16_t app_name[REACH_APPLICATION_TEXT_CAPACITY];
    reach_app_launch_request request;
} reach_host_deferred_launch;

typedef struct reach_host_app_launch_context
{
    uint64_t request_id;
    uint32_t pin_id;
    uint16_t app_name[REACH_APPLICATION_TEXT_CAPACITY];
} reach_host_app_launch_context;

typedef struct reach_host_pending_unpin
{
    uint32_t pin_id;
    double elapsed_seconds;
} reach_host_pending_unpin;

typedef struct reach_host_dirty_state
{
    int32_t layout;
    int32_t z_order;
    int32_t render;
    int32_t update_requested;
    int32_t bar_visibility;
    int32_t events_dispatched_this_cycle;
    int32_t monitors;
} reach_host_dirty_state;

typedef struct reach_host_pointer_move_state
{
    int32_t subscriptions_initialized;
    int32_t move_enabled[REACH_HOST_SURFACE_CAPACITY];
} reach_host_pointer_move_state;

struct reach_host
{
    reach_monitor_port monitors;

    size_t surface_count;
    reach_surface_runtime surfaces[REACH_HOST_SURFACE_CAPACITY];

    reach_pinned_app_model pinned_apps[REACH_MAX_PINNED_APPS];
    size_t pinned_app_count;
    reach_screen_hotspot_factory_port screen_hotspots;
    reach_image_loader_port image_loader;
    reach_window_thumbnail_port window_thumbnails;
    reach_render_command_buffer render_commands;
    reach_render_command_buffer popup_render_commands;
    uintptr_t focus_restore_window[REACH_HOST_SURFACE_CAPACITY];
    int32_t focus_restore_pending[REACH_HOST_SURFACE_CAPACITY];
    int32_t keyboard_focus_taken[REACH_HOST_SURFACE_CAPACITY];

    reach_feature_definition feature_definitions[REACH_HOST_SURFACE_CAPACITY];
    reach_feature_links feature_links[REACH_HOST_SURFACE_CAPACITY];
    reach_feature_runtime feature_runtimes[REACH_HOST_SURFACE_CAPACITY];
    reach_surface_id feature_lifecycle_order[REACH_HOST_SURFACE_CAPACITY];
    reach_surface_id feature_layout_order[REACH_HOST_SURFACE_CAPACITY];
    reach_host_surface_event_binding surface_event_bindings[REACH_HOST_SURFACE_CAPACITY];
    reach_host_edge_reveal_runtime edge_reveals[REACH_HOST_SURFACE_CAPACITY];
    reach_layout layout_manager;
    reach_host_layout_target layout_targets[REACH_LAYOUT_MAX_PARTICIPANTS];
    reach_layout_participant surface_participants[REACH_HOST_SURFACE_CAPACITY];
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
    reach_application_resolver_port application_resolver;
    reach_terminal_launcher_port terminal_launcher;
    reach_settings_launcher_port settings_launcher;
    reach_icon_service *icon_service;
    reach_wallpaper_service_port wallpaper_service;
    reach_wallpaper_surface_port wallpaper_surface;
    reach_power_session_port power_session;
    reach_idle_watch *idle_watch;
    const reach_theme *theme;
    reach_theme theme_storage;
    uint32_t theme_revision;

    reach_window_tracking *window_tracking;
    void (*pointer_moved_route)(void *user, reach_point_i32 point);
    void (*pointer_region_route)(void *user, uint32_t region_id, const reach_point_i32 *pointer);
    float layout_dpi_scale;
    int32_t has_layout;
    reach_host_dirty_state dirty;
    reach_monitor_refresh_retry monitor_refresh_retry;
    reach_monitor_topology monitor_topology;
    reach_config_service *config_service;
    // Version stamped in the config by the last Reach that ran, read before anything can save.
    uint16_t config_version_at_start[32];
    reach_wallpaper *wallpaper;
    reach_host_window_manipulation_state window_manipulation;
    reach_host_pointer_move_state pointer_move;
    reach_clipboard_port clipboard;
    reach_screen_capture_port screen_capture;
    reach_search_service *search_service;
    reach_app_control *app_control;
    reach_host_deferred_launch deferred_launch;
    std::deque<reach_host_app_launch_context> app_launch_contexts;
    std::deque<reach_host_pending_unpin> pending_unpins;
    int32_t running;
    reach_host_started_ports started_ports;
    reach_runtime_policy_state runtime_policy;
    reach_audio_volume_port audio_volume;
    reach_system_controls_port system_controls;
    reach_media_controls_port media_controls;
    reach_now_playing_service *now_playing_service;

    double popup_hook_reassert_seconds;
    int32_t high_refresh_rate;
    int32_t bundled_font;
    std::atomic<uint32_t> system_change_flags;
    std::atomic<uint32_t> audio_volume_changed;
    reach_system_status *system_status;
    reach_wifi_service *wifi_service;
    reach_bluetooth_service *bluetooth_service;
    reach_profile_service *profile_service;
    reach_system_stats *system_stats;
    reach_popup_capture_port popup_capture;
};

template <typename Tag> reach_surface_id reach_host_find_surface(const reach_host *host)
{
    return reach_host_find_surface(host, reach_surface_key_for<Tag>());
}

inline reach_feature_runtime *reach_host_surface_runtime(reach_host *host, reach_surface_id id)
{
    return host != nullptr && id < host->surface_count ? &host->feature_runtimes[id] : nullptr;
}

template <typename Feature>
static inline Feature *reach_host_feature_capsule(reach_host *host, reach_surface_id id)
{
    return host != nullptr && id < host->surface_count
               ? static_cast<Feature *>(host->feature_runtimes[id].capsule)
               : nullptr;
}

template <typename Feature>
static inline Feature *reach_host_feature_capsule(const reach_host *host, reach_surface_id id)
{
    return host != nullptr && id < host->surface_count
               ? static_cast<Feature *>(host->feature_runtimes[id].capsule)
               : nullptr;
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

int32_t reach_host_scalar_equal(float a, float b);

const reach_shadow *reach_host_surface_shadow(const reach_host *host, reach_surface_id id);
reach_shadow_pad reach_host_surface_shadow_pad(const reach_host *host, reach_surface_id id);
void reach_host_stamp_surface_content(const reach_host *host, reach_surface_id id,
                                      reach_render_command_buffer *commands);
reach_result reach_host_apply_window_state(reach_platform_window_port *window,
                                           reach_rect_f32 bounds, reach_shadow_pad pad,
                                           reach_rect_f32 *last_bounds, int32_t *bounds_valid,
                                           int32_t *out_changed);
reach_host_surface_presentation_frame reach_host_surface_presentation_frame_compute(
    reach_rect_f32 target_bounds, reach_rect_f32 envelope_bounds, reach_shadow_pad shadow_pad,
    reach_rect_f32 translation_bounds, float max_scale);
void reach_host_surface_presentation_extent(const reach_render_group *group,
                                            reach_rect_f32 *translation_bounds, float *max_scale);
void reach_host_finish_surface_presentations(reach_host *host);
reach_window_thumbnail_placement reach_host_native_overlay_placement(
    reach_window_thumbnail_placement placement, const reach_feature_surface_geometry *geometry,
    const reach_feature_surface_context *ctx, reach_shadow_pad shadow_pad, double now_seconds);

reach_point_i32
reach_host_project_surface_screen_point(const reach_feature_runtime *desc,
                                        const reach_feature_surface_geometry *geometry,
                                        reach_point_i32 point, double now_seconds);
reach_pointer_event reach_host_surface_pointer_event(const reach_feature_runtime *desc,
                                                     const reach_ui_event *event,
                                                     reach_pointer_event_kind kind);
int32_t reach_host_popup_owner_trigger(const reach_feature_runtime *popup,
                                       const reach_feature_runtime *source,
                                       const reach_capsule_pointer_result *source_result);
int32_t reach_host_resolve_popup_owner(const reach_feature_runtime *popup,
                                       reach_feature_layout_anchor *out);

void reach_host_request_update(reach_host *host);
void reach_host_on_registered_surface_event(void *user, const reach_ui_event *event);

reach_result reach_host_render_popup_surface(reach_host *host, reach_surface_id surface_id,
                                             reach_surface_runtime *surface, reach_rect_f32 bounds,
                                             float notch_anchor_x, int32_t notch_side,
                                             const reach_render_command_buffer *content_commands);

void reach_host_sync_popup_mouse_hook(reach_host *host);
void reach_host_close_transient_surfaces(reach_host *host, int32_t restore_focus);
void reach_host_close_surface_classes(reach_host *host, uint32_t class_mask, int32_t restore_focus);

reach_result reach_host_schedule_app_launch(reach_host *host,
                                            const reach_app_launch_request *request);
reach_result reach_host_schedule_reveal_path(reach_host *host, const uint16_t *path);
reach_result reach_host_launch_settings_app(reach_host *host);
void reach_host_stop_app_control(reach_host *host);
void reach_host_process_deferred_launch(reach_host *host);
void reach_host_apply_app_launch_results(reach_host *host);
void reach_host_tick_pending_unpins(reach_host *host, double delta_seconds);
void reach_host_release_native_overlay(reach_host *host, reach_feature_runtime *desc);
void reach_host_set_native_overlay_front_source(reach_host *host, reach_feature_runtime *desc,
                                                reach_window_id source);
reach_result reach_host_defer_launch_until_surface_closed(reach_host *host, reach_surface_id source,
                                                          const reach_app_launch_request *request,
                                                          uint32_t pin_id,
                                                          const uint16_t *app_name);
reach_result reach_host_open_feature_target(reach_host *host, reach_surface_id source,
                                            const reach_feature_target *target, uint32_t flags,
                                            uint32_t pin_id);
reach_result reach_host_pin_feature_target(reach_host *host, const reach_feature_target *target,
                                           uintptr_t window_id);

reach_result reach_host_focus_window(reach_host *host, uintptr_t window_id,
                                     int32_t minimize_if_foreground);
// Whether toggling the window's focus minimizes it (it is the unminimized foreground window).
// Refreshes the window world first.
int32_t reach_host_window_toggle_minimizes(reach_host *host, uintptr_t window_id);
reach_result reach_host_launch_app(reach_host *host, const uint16_t *path,
                                   const uint16_t *arguments, const uint16_t *app_user_model_id,
                                   reach_application_launch_kind launch_kind,
                                   int32_t force_new_instance, int32_t run_as_admin,
                                   reach_surface_id source, int32_t defer_until_closed,
                                   uint32_t pin_id, const uint16_t *app_name);
reach_result reach_host_open_app(reach_host *host, const uint16_t *path, const uint16_t *arguments,
                                 const uint16_t *app_user_model_id,
                                 reach_application_launch_kind launch_kind,
                                 int32_t force_new_instance, int32_t run_as_admin,
                                 reach_surface_id source, int32_t defer_until_closed,
                                 uint32_t pin_id, const uint16_t *app_name);
reach_result reach_host_set_pinned_apps(reach_host *host, const reach_pinned_app_model *apps,
                                        size_t count);

reach_result reach_host_close_window(reach_host *host, uintptr_t window_id);

int32_t reach_host_icon_size_px(const reach_host *host);
int32_t reach_host_icon_size_px_for_scale(float dpi_scale);

void reach_host_release_render_icon(reach_host *host, uint64_t icon_id);

void reach_host_drain_icon_evictions(reach_host *host);
void reach_host_drain_registered_render_resources(reach_host *host);
void reach_host_release_registered_render_resources(reach_host *host);
void reach_host_drain_now_playing_retired_covers(reach_host *host);
reach_result reach_host_refresh_open_windows(reach_host *host, int32_t *out_changed);
void reach_host_note_foreground_window(reach_host *host, uintptr_t foreground_window);
void reach_host_apply_foreground_change(reach_host *host);

int32_t reach_host_window_is_minimized(const reach_host *host, uintptr_t window_id);
reach_result reach_host_schedule_window_control(reach_host *host,
                                                reach_window_control_action action,
                                                uintptr_t window_id, uint64_t *out_request_id);
reach_result reach_host_schedule_covered_activation(reach_host *host, uintptr_t window_id,
                                                    uint64_t *out_request_id);
reach_result reach_host_schedule_minimize_open_windows(reach_host *host, uint64_t *out_request_id);
reach_result reach_host_schedule_open_terminal(reach_host *host);
reach_result reach_host_schedule_terminal_command(reach_host *host, const uint16_t *command);
reach_result reach_host_schedule_window_controls(reach_host *host,
                                                 reach_window_control_action action,
                                                 const uintptr_t *window_ids, size_t window_count,
                                                 uint64_t *out_request_id);
void reach_host_apply_window_control_result(reach_host *host);
void reach_host_settle_close_handoffs(reach_host *host);
void reach_host_cancel_close_handoff(reach_host *host, reach_feature_runtime *runtime);

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
int32_t reach_host_bar_reveal_enabled(const reach_feature_runtime *runtime);
float reach_host_bar_parking_offset(const reach_feature_runtime *runtime,
                                    reach_rect_f32 shown_bounds);
float reach_host_bar_protected_clearance(const reach_host *host,
                                         const reach_feature_runtime *runtime);
reach_rect_i32 reach_host_merge_work_area(reach_rect_i32 monitor,
                                          const reach_host_work_area_claim *claims, size_t count);

reach_rect_f32 reach_host_reconcile_bar_visibility(reach_host *host, reach_surface_id id,
                                                   reach_rect_f32 shown_bounds,
                                                   reach_rect_f32 monitor_bounds,
                                                   const reach_feature_surface_geometry *geometry);
reach_result reach_host_cycle_input_language(reach_host *host);

reach_result reach_host_refresh_monitor_layout(reach_host *host);
int32_t reach_host_can_move_bars_without_redraw(const reach_host *host);
reach_result reach_host_move_bar_animation_frame(reach_host *host);

reach_result reach_host_execute_media_action(reach_host *host, reach_now_playing_action action);
reach_result reach_host_step_main_volume(reach_host *host, float delta);
reach_result reach_host_toggle_main_volume_mute(reach_host *host);
reach_result reach_host_step_brightness(reach_host *host, float delta);
void reach_host_show_brightness(reach_host *host, const reach_brightness_state *state);
reach_result reach_host_snap_foreground_window(reach_host *host, reach_split_mode mode);

void reach_host_on_system_controls_changed(void *user, uint32_t change_flags);
void reach_host_on_audio_volume_changed(void *user);

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

void reach_host_apply_display_config(reach_host *host, const reach_config_snapshot *snapshot);
void reach_host_set_light_theme(reach_host *host, int32_t light_theme);
void reach_host_take_keyboard_focus(reach_host *host, reach_feature_runtime *runtime);

int32_t reach_host_game_mode_enabled(const reach_host *host);
reach_result reach_host_update_game_mode(reach_host *host);
#endif
