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
        int32_t battery_valid;
        int32_t battery_percent;
        float power_hover;
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

#define REACH_DOCK_SLOT_CAPACITY (REACH_MAX_PINNED_APPS + 2)

    enum reach_dock_animation_id
    {
        REACH_DOCK_ANIM_Y = 0,
        REACH_DOCK_ANIM_DRAG_SNAP,
        REACH_DOCK_ANIM_FEEDBACK_OPACITY,
        REACH_DOCK_ANIM_NOW_PLAYING_CONTENT,
        REACH_DOCK_ANIM_POWER_HOVER,
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

    void reach_dock_attach_services(reach_dock *dock, reach_icon_service *icons,
                                    reach_window_tracking *windows,
                                    reach_now_playing_service *now_playing);

    reach_animation_manager *reach_dock_manager(reach_dock *animations);

    const reach_feature_capsule_ops *reach_dock_capsule_ops(void);

    void reach_dock_mark_items_changed(reach_dock *dock);
    int32_t reach_dock_take_items_changed(reach_dock *dock);

    int32_t reach_dock_pointer_sequence_active(const reach_dock *dock);

    reach_dock_pointer_region reach_dock_pointer_region_at(const reach_dock *dock, int32_t local_x,
                                                           int32_t local_y);

    void reach_dock_begin_reveal_session(reach_dock *dock);

    typedef struct reach_dock_build_context
    {
        const reach_theme *theme;
        float dpi_scale;
        float icon_size;
        float gap;
        const reach_pinned_app_model *pinned_apps;
        size_t pinned_app_count;
    } reach_dock_build_context;

    void reach_dock_build_layout(reach_dock *dock, const reach_dock_build_context *ctx,
                                 reach_dock_layout *layout);

    reach_point_i32 reach_dock_local_point(const reach_dock_layout *layout, int32_t x, int32_t y);
    reach_rect_f32 reach_dock_rect_to_screen(const reach_dock_layout *layout, reach_rect_f32 rect);
    reach_dock_layout reach_dock_layout_to_screen(reach_dock_layout layout);

    void reach_dock_rebuild_items(reach_dock *dock, const reach_dock_build_context *ctx,
                                  const reach_dock_layout *old_layout,
                                  reach_dock_layout *out_layout);

    int32_t reach_dock_slots_animating(const reach_dock *dock);

    void reach_dock_clear_item_x_animations(reach_dock *dock);

    enum
    {
        REACH_DOCK_FEEDBACK_TRAY_BUTTON = REACH_MAX_PINNED_APPS,
        REACH_DOCK_FEEDBACK_QUICK_SETTINGS_BUTTON,
        REACH_DOCK_FEEDBACK_POWER_BUTTON,
        REACH_DOCK_FEEDBACK_NONE
    };

    void reach_dock_suppress_power_release(reach_dock *dock);

    int32_t reach_dock_feedback_stick(reach_dock *dock);
    int32_t reach_dock_feedback_clear_sticky(reach_dock *dock);

    int32_t reach_dock_update_clock(reach_dock *dock);

    typedef struct reach_dock_render_context
    {
        const reach_theme *theme;
        const reach_dock_layout *layout;
        uintptr_t focused_window;
        const reach_pinned_app_model *pinned_apps;
        size_t pinned_app_count;
        int32_t icon_size_px;
        float dpi_scale;
        float dock_gap;
        int32_t battery_valid;
        int32_t battery_percent;
    } reach_dock_render_context;

    reach_result reach_dock_append_render_commands(reach_dock *dock,
                                                   const reach_dock_render_context *ctx,
                                                   reach_render_command_buffer *out_commands);

    void reach_dock_touch_icons(reach_dock *dock, int32_t icon_size_px);

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

        int32_t target_hidden;
        int32_t reveal_session_active;
        int32_t pointer_sequence_active;
        int32_t dock_animation_initialized;

        reach_dock_drag_state drag;
        size_t pressed_index;
        int32_t pressed_control;

        int32_t power_release_suppressed;
        int32_t power_hovered;

        size_t feedback_index;
        int32_t feedback_pressed;
        int32_t feedback_sticky;

        int32_t item_x_valid[REACH_MAX_PINNED_APPS];
        int32_t item_x_pinned[REACH_MAX_PINNED_APPS];
        uint32_t item_x_pin_ids[REACH_MAX_PINNED_APPS];
        uintptr_t item_x_windows[REACH_MAX_PINNED_APPS];
        int32_t items_changed;

        uint16_t clock_time_text[32];
        uint16_t clock_date_text[64];
        int32_t clock_initialized;
        int64_t clock_last_minute;
    } reach_dock_state;

    const reach_dock_state *reach_dock_state_ptr(reach_dock *animations);

    size_t reach_dock_item_count(reach_dock *dock);
    const reach_dock_item_model *reach_dock_item_at(reach_dock *dock, size_t index);

    size_t reach_dock_build_item_context_commands(reach_dock *dock, size_t item_index,
                                                  uint32_t *out_commands, size_t cap);

    size_t reach_dock_order_count(reach_dock *dock);
    reach_dock_order_key reach_dock_order_key_at(reach_dock *dock, size_t index);
    void reach_dock_restore_order(reach_dock *dock, const reach_dock_order_key *keys, size_t count);

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
        int32_t edge_mode;
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
