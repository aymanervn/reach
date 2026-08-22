#ifndef REACH_FEATURES_BATTERY_COMMON_H
#define REACH_FEATURES_BATTERY_COMMON_H

#include "reach/features/battery.h"
#include "reach/features/common/pressable.h"

#include "battery_metrics.h"

enum
{
    REACH_BATTERY_ANIMATION_PRESS_FEEDBACK = 0,
    REACH_BATTERY_ANIMATION_COUNT
};

struct reach_battery
{
    reach_battery_state state;
    double saver_pending_seconds;
    reach_animation_manager animations;
    reach_animation_track animation_tracks[REACH_BATTERY_ANIMATION_COUNT];
    reach_pressable pressable;
};

float reach_battery_press_feedback_value(const reach_battery *battery);

#endif
