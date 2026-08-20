#ifndef REACH_FEATURES_BATTERY_COMMON_H
#define REACH_FEATURES_BATTERY_COMMON_H

#include "reach/features/battery.h"

#include "battery_metrics.h"

struct reach_battery
{
    reach_battery_state state;
    double saver_pending_seconds;
};

#endif
