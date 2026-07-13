#ifndef REACH_FEATURES_DOCK_INTERACTION_H
#define REACH_FEATURES_DOCK_INTERACTION_H

#include "reach/features/dock.h"

typedef enum reach_dock_hit_type
{
    REACH_DOCK_HIT_NONE = 0,
    REACH_DOCK_HIT_ITEM = 1,
    REACH_DOCK_HIT_TRAY_BUTTON = 2,
    REACH_DOCK_HIT_QUICK_SETTINGS_BUTTON = 3,
    REACH_DOCK_HIT_POWER_BUTTON = 4
} reach_dock_hit_type;

typedef struct reach_dock_hit_result
{
    reach_dock_hit_type type;
    size_t index;
} reach_dock_hit_result;

reach_dock_hit_result reach_dock_hit_test(const reach_dock_layout *layout, int32_t x,
                                          int32_t y);
float reach_dock_drag_clamped_x(const reach_theme *theme, const reach_dock_layout *layout,
                                int32_t cursor_x, float grab_offset_x);

typedef enum reach_dock_item_action_type
{
    REACH_DOCK_ITEM_ACTION_NONE = 0,
    REACH_DOCK_ITEM_ACTION_LAUNCH_PINNED = 1,
    REACH_DOCK_ITEM_ACTION_FOCUS_WINDOW = 2
} reach_dock_item_action_type;

typedef struct reach_dock_item_action
{
    reach_dock_item_action_type type;
    size_t item_index;
    size_t pinned_index;
    uint32_t pin_id;
    uintptr_t window;
} reach_dock_item_action;

typedef struct reach_dock_interaction_context
{
    const reach_theme *theme;
    const reach_dock_layout *layout;
    const reach_pinned_app_model *pinned_apps;
    size_t pinned_app_count;
} reach_dock_interaction_context;

typedef struct reach_dock_interaction_result
{
    int32_t redraw;
    int32_t rebuild_items;
    int32_t move_pin;
    uint32_t move_pin_id;
    size_t move_pin_target;
} reach_dock_interaction_result;

int32_t reach_dock_feedback_press(reach_dock *dock, size_t slot);
int32_t reach_dock_feedback_press_immediate(reach_dock *dock, size_t slot, float opacity);
int32_t reach_dock_feedback_set_immediate(reach_dock *dock, size_t slot, float opacity);
int32_t reach_dock_feedback_release(reach_dock *dock);
int32_t reach_dock_take_power_release_suppressed(reach_dock *dock);
void reach_dock_clear_power_release_suppressed(reach_dock *dock);

void reach_dock_item_press(reach_dock *dock, size_t index, int32_t x, int32_t y,
                           const reach_dock_interaction_context *ctx,
                           reach_dock_interaction_result *out);
void reach_dock_drag_update(reach_dock *dock, int32_t x, int32_t y,
                            const reach_dock_interaction_context *ctx,
                            reach_dock_interaction_result *out);
void reach_dock_drag_end(reach_dock *dock, const reach_dock_interaction_context *ctx,
                         reach_dock_interaction_result *out);
int32_t reach_dock_item_release(reach_dock *dock, size_t index,
                                reach_dock_item_action *out_action,
                                reach_dock_interaction_result *out);
void reach_dock_clear_pressed(reach_dock *dock);
reach_dock_item_action reach_dock_item_action_for_index(const reach_dock_feature_model *model,
                                                        size_t item_index);

#endif
