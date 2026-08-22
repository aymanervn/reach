#ifndef REACH_FEATURES_STAGE_COMMON_H
#define REACH_FEATURES_STAGE_COMMON_H

#include "reach/features/stage.h"

#include "reach/core/theme.h"
#include "reach/features/common/pressable.h"
#include "reach/support/animation.h"

static inline float reach_stage_animation_seconds_default(void)
{
    return reach_theme_default()->stage_animation_seconds;
}

static inline double reach_stage_close_hover_seconds(void)
{
    return (double)reach_theme_default()->stage_close_hover_seconds;
}

static inline double reach_stage_reflow_seconds(void)
{
    return (double)reach_theme_default()->stage_reflow_seconds;
}

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
    reach_pressable pressable;
    size_t pressable_generation;
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
