#ifndef REACH_FEATURES_BATTERY_COMMON_H
#define REACH_FEATURES_BATTERY_COMMON_H

#include "reach/features/battery.h"
#include "reach/features/common/popup.h"
#include "reach/features/common/pressable.h"

#include "battery_metrics.h"

struct reach_battery
{
    reach_battery_state state;
    reach_system_status *status;
    reach_battery_routes routes;
    double saver_pending_deadline;
    reach_motion press_feedback;
    reach_motion time_reveal_opacity;
    reach_motion time_reveal_y;
    float time_reveal_offset;
    reach_motion_spec fade_in_spec;
    reach_motion_spec smooth_spec;
    reach_feature_transition popup_transition;
    reach_pressable pressable;
};

#endif
