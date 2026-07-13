#ifndef REACH_FEATURES_TRAY_COMMON_H
#define REACH_FEATURES_TRAY_COMMON_H

#include "reach/features/tray.h"

typedef enum reach_tray_hit_type
{
    REACH_TRAY_HIT_NONE = 0,
    REACH_TRAY_HIT_ITEM = 1,
    REACH_TRAY_HIT_POPUP = 2
} reach_tray_hit_type;

typedef struct reach_tray_hit_result
{
    reach_tray_hit_type type;
    size_t index;
} reach_tray_hit_result;

typedef enum reach_tray_action_type
{
    REACH_TRAY_FEATURE_ACTION_NONE = 0,
    REACH_TRAY_FEATURE_ACTION_ACTIVATE = 1
} reach_tray_action_type;

typedef struct reach_tray_feature_action
{
    reach_tray_action_type type;
    size_t item_index;
    uint32_t item_id;
    reach_tray_action provider_action;
} reach_tray_feature_action;

reach_tray_hit_result reach_tray_hit_test_popup(const reach_tray_model *model,
                                                reach_rect_f32 popup_bounds, int32_t x, int32_t y);
reach_tray_feature_action reach_tray_action_for_hit(const reach_tray_model *model,
                                                    reach_tray_hit_result hit,
                                                    reach_tray_action provider_action);
int32_t reach_tray_feedback_press(reach_tray *tray, size_t index);
int32_t reach_tray_feedback_release(reach_tray *tray);

#endif
