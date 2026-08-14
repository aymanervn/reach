#ifndef REACH_FEATURES_STAGE_COMMON_H
#define REACH_FEATURES_STAGE_COMMON_H

#include "reach/features/stage.h"

#include "reach/support/animation.h"

enum
{
    REACH_STAGE_ANIMATION_PROGRESS = 0,
    REACH_STAGE_ANIMATION_COUNT
};

typedef struct reach_stage_grid
{
    size_t columns;
    size_t rows;
} reach_stage_grid;

struct reach_stage
{
    reach_animation_manager animations;
    reach_animation_track animation_tracks[REACH_STAGE_ANIMATION_COUNT];
    reach_stage_state state;
    reach_icon_service *icons;
    reach_window_tracking *windows;
    int32_t closing_settled;
};

void reach_stage_rebuild_layout(reach_stage *stage);
void reach_stage_apply_progress(reach_stage *stage);

#endif
