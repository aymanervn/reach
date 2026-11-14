#ifndef REACH_FEATURES_STAGE_COMMON_H
#define REACH_FEATURES_STAGE_COMMON_H

#include "reach/features/stage.h"

#include "reach/core/theme.h"
#include "reach/features/common/pressable.h"
#include "reach/support/animation.h"

#define REACH_STAGE_DESKTOP_ANIMATION_FACTOR 0.35

static inline float reach_stage_animation_seconds_default(void)
{
    return reach_theme_default()->stage_animation_seconds;
}

static inline reach_motion_spec reach_stage_motion_preset(reach_motion_preset preset)
{
    return reach_theme_motion_preset(reach_theme_default(), preset);
}

static inline reach_motion_spec reach_stage_presentation_spec(float animation_seconds, float factor)
{
    reach_motion_spec spec = reach_stage_motion_preset(REACH_MOTION_PRESET_SMOOTH);
    float seconds =
        animation_seconds > 0.0f ? animation_seconds : reach_stage_animation_seconds_default();
    return reach_motion_spec_scaled(
        spec, reach_theme_stage_motion_scale(reach_theme_default(), seconds * factor));
}

enum
{
    REACH_STAGE_ANIMATION_PROGRESS = 0,
    REACH_STAGE_ANIMATION_DESKTOP_PROGRESS,
    REACH_STAGE_ANIMATION_REFLOW,
    REACH_STAGE_ANIMATION_DEPARTURE,
    REACH_STAGE_ANIMATION_BACKDROP,
    REACH_STAGE_ANIMATION_RETARGET,
    REACH_STAGE_ANIMATION_COUNT
};

struct reach_stage
{
    reach_animation_manager animations;
    reach_animation_track animation_tracks[REACH_STAGE_ANIMATION_COUNT];
    reach_stage_state state;
    reach_motion close_hover;
    reach_pressable pressable;
    size_t pressable_generation;

    reach_window_tracking *windows;
    reach_icon_service *icons;
    reach_app_control *apps;
    reach_display_environment display;
};

reach_rect_f32 reach_stage_interpolate_rect(reach_rect_f32 from, reach_rect_f32 to, float factor);
void reach_stage_rebuild_layout(reach_stage *stage);

static inline void reach_stage_track_to(reach_stage *stage, size_t track, float target,
                                        reach_motion_spec spec)
{
    double now = reach_motion_now_seconds();
    if (spec.easing == REACH_EASING_SPRING)
        reach_animation_manager_spring_to_at(&stage->animations, track, target,
                                             spec.response_seconds, spec.damping, now);
    else
        reach_animation_manager_animate_to_at(&stage->animations, track, target,
                                              spec.duration_seconds, spec.easing, now);
}

static inline void reach_stage_track_start(reach_stage *stage, size_t track, float from, float to,
                                           reach_motion_spec spec)
{
    reach_animation_manager_set(&stage->animations, track, from);
    reach_stage_track_to(stage, track, to, spec);
}
void reach_stage_apply_progress(reach_stage *stage);
void reach_stage_start_reflow(reach_stage *stage);
void reach_stage_settle_reflow(reach_stage *stage);
void reach_stage_depart_tile(reach_stage *stage, size_t index);
float reach_stage_tile_bar_height(const reach_stage_state *state);

#endif
