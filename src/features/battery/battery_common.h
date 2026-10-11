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
    double saver_pending_seconds;
    reach_motion press_feedback;
    reach_feature_transition popup_transition;
    reach_pressable pressable;
};

#endif
