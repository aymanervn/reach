#ifndef REACH_FEATURES_SYSTEM_HUD_COMMON_H
#define REACH_FEATURES_SYSTEM_HUD_COMMON_H

#include "reach/features/system_hud.h"
#include "reach/support/animation.h"

enum
{
    REACH_SYSTEM_HUD_ANIMATION_OPACITY = 0,
    REACH_SYSTEM_HUD_ANIMATION_COUNT
};

struct reach_system_hud
{
    reach_system_hud_state state;
    reach_now_playing_service *now_playing;
    reach_animation_manager animations;
    reach_animation_track animation_tracks[REACH_SYSTEM_HUD_ANIMATION_COUNT];
    double open_seconds;
    double close_seconds;
};

void reach_system_hud_begin_close(reach_system_hud *hud);

#endif
