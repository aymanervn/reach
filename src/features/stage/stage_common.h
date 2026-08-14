#ifndef REACH_FEATURES_STAGE_COMMON_H
#define REACH_FEATURES_STAGE_COMMON_H

#include "reach/features/stage.h"

#include "reach/support/animation.h"

#define REACH_STAGE_CLOSE_HOVER_SECONDS 0.14

enum
{
    REACH_STAGE_ANIMATION_PROGRESS = 0,
    REACH_STAGE_ANIMATION_REFLOW,
    REACH_STAGE_ANIMATION_CLOSE_HOVER,
    REACH_STAGE_ANIMATION_COUNT
};

struct reach_stage
{
    reach_animation_manager animations;
    reach_animation_track animation_tracks[REACH_STAGE_ANIMATION_COUNT];
    reach_stage_state state;
    int32_t closing_settled;
};

reach_rect_f32 reach_stage_interpolate_rect(reach_rect_f32 from, reach_rect_f32 to, float factor);
void reach_stage_rebuild_layout(reach_stage *stage);
void reach_stage_apply_progress(reach_stage *stage);
void reach_stage_start_reflow(reach_stage *stage);
void reach_stage_settle_reflow(reach_stage *stage);
void reach_stage_depart_tile(reach_stage *stage, size_t index);
float reach_stage_tile_bar_height(const reach_stage_state *state);
float reach_stage_tile_border(const reach_stage_state *state);

#endif
