#ifndef REACH_FEATURES_SYSTEM_HUD_COMMON_H
#define REACH_FEATURES_SYSTEM_HUD_COMMON_H

#include "reach/features/system_hud.h"
#include "reach/features/common/presentation.h"

struct reach_system_hud
{
    reach_system_hud_state state;
    reach_now_playing_service *now_playing;
    reach_feature_transition transition;
};

#endif
