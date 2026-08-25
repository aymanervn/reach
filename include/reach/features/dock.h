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
#include "reach/features/common/bar_visibility.h"
#include "reach/features/common/draggable.h"
#include "reach/features/common/pressable.h"
#include "reach/features/feature_capsule.h"
#include "reach/services/icon_service.h"
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
        uint32_t app_id;
    } reach_dock_order_key;

    typedef struct reach_dock_item_model
    {
        int32_t pinned;
        uint32_t app_id;
        uintptr_t window;
        size_t pinned_index;
    } reach_dock_item_model;

    typedef struct reach_dock_feature_model
    {
        reach_dock_item_model items[REACH_MAX_DOCK_ITEMS];
        size_t item_count;
        reach_dock_order_key order[REACH_MAX_DOCK_ITEMS];
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
    reach_dock_order_key reach_dock_item_key_at(const reach_dock_feature_model *model,
                                                size_t index);
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
        const uint32_t *window_group_ids, size_t open_window_count,
        reach_dock_window_matches_pinned_fn window_matches_pinned, void *match_user);
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
        size_t trigger_feedback_index;
        int32_t text_alignment_center;
        float dpi_scale;
    } reach_dock_render_input;

    reach_result reach_dock_build_render_commands(const reach_dock_render_input *input,
                                                  reach_render_command_buffer *out_commands);

    typedef enum reach_dock_pointer_region
    {
        REACH_DOCK_POINTER_REGION_NONE = 0,
        REACH_DOCK_POINTER_REGION_ITEM = 1,
        REACH_DOCK_POINTER_REGION_TRIGGER = 5
    } reach_dock_pointer_region;

    typedef enum reach_dock_pointer_action_kind
    {
        REACH_DOCK_POINTER_ACTION_NONE = 0,
        REACH_DOCK_POINTER_ACTION_PRESS_ITEM = 2,
        REACH_DOCK_POINTER_ACTION_LAUNCH_PINNED = 6,
        REACH_DOCK_POINTER_ACTION_FOCUS_WINDOW = 7,
        REACH_DOCK_POINTER_ACTION_LAUNCH_NEW_INSTANCE = 8,
        REACH_DOCK_POINTER_ACTION_SHOW_ITEM_CONTEXT = 9,
        REACH_DOCK_POINTER_ACTION_REBUILD_ITEMS = 16,
        REACH_DOCK_POINTER_ACTION_MOVE_PIN = 17,
        REACH_DOCK_POINTER_ACTION_HOVER_ITEM = 18,
        REACH_DOCK_POINTER_ACTION_PRESS_TRIGGER = 19,
        REACH_DOCK_POINTER_ACTION_ACTIVATE_TRIGGER = 20
    } reach_dock_pointer_action_kind;

#define REACH_DOCK_TRIGGER_PRIMARY 0

    size_t reach_dock_reorder_target(const reach_dock_feature_model *model,
                                     const reach_dock_layout *layout, size_t current_index,
                                     float dragged_box_x);

#define REACH_DOCK_SLOT_CAPACITY (REACH_MAX_DOCK_ITEMS + 1)

    enum reach_dock_animation_id
    {
        REACH_DOCK_ANIM_Y = 0,
        REACH_DOCK_ANIM_DRAG_SNAP,
        REACH_DOCK_ANIM_FEEDBACK_OPACITY,
        REACH_DOCK_ANIM_ITEM_X_BASE,
        REACH_DOCK_ANIM_SLOT_BASE = REACH_DOCK_ANIM_ITEM_X_BASE + REACH_MAX_DOCK_ITEMS,
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
                                    reach_window_tracking *windows);

    reach_animation_manager *reach_dock_manager(reach_dock *animations);

    const reach_feature_capsule_ops *reach_dock_capsule_ops(void);

    void reach_dock_mark_items_changed(reach_dock *dock);
    int32_t reach_dock_take_items_changed(reach_dock *dock);

    reach_dock_pointer_region reach_dock_pointer_region_at(const reach_dock *dock, int32_t local_x,
                                                           int32_t local_y);

    const reach_bar_reveal_ops *reach_dock_reveal_ops(void);

    typedef struct reach_dock_build_context
    {
        const reach_theme *theme;
        float dpi_scale;
        float icon_size;
        float gap;
        const reach_pinned_app_model *pinned_apps;
        size_t pinned_app_count;
    } reach_dock_build_context;

    typedef struct reach_dock_fit_result
    {
        float scale;
        float width;
        float height;
        float icon_size;
        float gap;
    } reach_dock_fit_result;

    reach_dock_fit_result reach_dock_fit_metrics(float native_height, float native_icon_size,
                                                 float native_gap, float native_border_thickness,
                                                 float available_width, float app_slot_units);

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
        REACH_DOCK_FEEDBACK_TRIGGER = REACH_MAX_DOCK_ITEMS
    };

    int32_t reach_dock_retain_context_feedback(reach_dock *dock);
    int32_t reach_dock_clear_context_feedback(reach_dock *dock);

    typedef struct reach_dock_render_context
    {
        const reach_theme *theme;
        const reach_dock_layout *layout;
        uintptr_t focused_window;
        const reach_pinned_app_model *pinned_apps;
        size_t pinned_app_count;
        int32_t icon_size_px;
        float dpi_scale;
    } reach_dock_render_context;

    reach_result reach_dock_append_render_commands(reach_dock *dock,
                                                   const reach_dock_render_context *ctx,
                                                   reach_render_command_buffer *out_commands);

    void reach_dock_touch_icons(reach_dock *dock, int32_t icon_size_px);

    typedef struct reach_dock_drag_state
    {
        reach_draggable gesture;
        size_t target_index;
        reach_dock_order_key key;
        float grab_offset_x;
        float x;
    } reach_dock_drag_state;

    typedef struct reach_dock_state
    {
        reach_dock_feature_model model;

        reach_bar_visibility_state visibility;
        reach_pressable pressable;

        reach_dock_drag_state drag;
        size_t hovered_item;

        int32_t item_x_valid[REACH_MAX_DOCK_ITEMS];
        reach_dock_order_key item_x_keys[REACH_MAX_DOCK_ITEMS];
        int32_t items_changed;
    } reach_dock_state;

    const reach_dock_state *reach_dock_state_ptr(reach_dock *animations);

    size_t reach_dock_item_count(reach_dock *dock);
    const reach_dock_item_model *reach_dock_item_at(reach_dock *dock, size_t index);

    size_t reach_dock_build_item_context_commands(reach_dock *dock, size_t item_index,
                                                  uint32_t *out_commands, size_t cap);

    typedef struct reach_dock_item_window
    {
        uintptr_t window;
        const uint16_t *title;
    } reach_dock_item_window;

    size_t reach_dock_collect_matching_windows(const reach_pinned_app_model *pinned_app,
                                               const reach_window_snapshot *windows,
                                               size_t window_count, const uintptr_t *focus_history,
                                               size_t focus_history_count,
                                               reach_dock_window_matches_pinned_fn matches,
                                               void *match_user, size_t *out_indices, size_t cap);

    size_t reach_dock_collect_item_windows(reach_dock *dock, size_t item_index,
                                           const reach_pinned_app_model *pinned_apps,
                                           size_t pinned_app_count, reach_dock_item_window *out,
                                           size_t cap);

    size_t reach_dock_order_count(reach_dock *dock);
    reach_dock_order_key reach_dock_order_key_at(reach_dock *dock, size_t index);
    void reach_dock_restore_order(reach_dock *dock, const reach_dock_order_key *keys, size_t count);

#ifdef __cplusplus
}
#endif

#endif
