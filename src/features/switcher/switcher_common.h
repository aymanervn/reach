#ifndef REACH_FEATURES_SWITCHER_COMMON_H
#define REACH_FEATURES_SWITCHER_COMMON_H

#include "reach/features/switcher.h"

reach_switcher_state *reach_switcher_state_mut(reach_switcher *switcher);
reach_icon_service *reach_switcher_icons(reach_switcher *switcher);
reach_window_tracking *reach_switcher_windows(reach_switcher *switcher);

#endif
