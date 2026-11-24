#ifndef REACH_FEATURES_STAGE_COMMON_H
#define REACH_FEATURES_STAGE_COMMON_H

#include "reach/features/stage.h"

#include "reach/core/theme.h"
#include "reach/features/common/pressable.h"
#include "reach/features/common/selection_pill.h"
#include "reach/support/motion.h"

#define REACH_STAGE_DESKTOP_ANIMATION_FACTOR 0.35
#define REACH_STAGE_PRESENCE_MIN_SCALE 0.88f
// Space the layout keeps around every tile's header and thumbnail, in DP, so the hover pill can
// surround the tile with an even ring without crowding its neighbours.
#define REACH_STAGE_TILE_MARGIN 4.0f

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
    REACH_STAGE_ANIMATION_RETARGET,
    REACH_STAGE_ANIMATION_COUNT
};

struct reach_stage
{
    reach_motion motions[REACH_STAGE_ANIMATION_COUNT];
    double sampled_seconds;
    reach_stage_state state;
    reach_motion close_hover;
    // The shared selection pill behind the hovered tile; it glides between hovered tiles and
    // fades out when no tile is hovered.
    reach_selection_pill hover_pill;
    reach_motion hover_pill_opacity;
    reach_pressable pressable;
    size_t pressable_generation;

    reach_window_tracking *windows;
    reach_icon_service *icons;
    reach_app_control *apps;
    reach_display_environment display;

    // Set when the surface's renderer composes live window thumbnails, so tiles move on the
    // compositor and Stage only wakes at motion events.
    int32_t compositor_motion;
    // The last render could not express a tile as compositor motion and samples every frame.
    int32_t compositor_sampled;
    double compositor_event_seconds;
};

typedef struct reach_stage_tile_motion
{
    reach_rect_motion rect;
    reach_motion bar;
    reach_motion alpha;
    reach_motion presence;
    uint32_t tracks;
} reach_stage_tile_motion;

// Returns 0 when the tile needs more than one moving track or would not match the CPU sample at
// now; the state must already be sampled at now.
int32_t reach_stage_tile_motion_at(const reach_stage *stage, size_t index, double now,
                                   reach_stage_tile_motion *out);
// The next moment Stage logic must run while its tiles move on the compositor: the earliest
// spring snap or the end of another moving track; 0 when nothing moves.
double reach_stage_motion_event_seconds(const reach_stage *stage,
                                        const reach_stage_tile_motion *motions, double now);
// Samples every track at now and lays tiles out from it, without snapping settled springs.
void reach_stage_sample_at(reach_stage *stage, double now);
int32_t reach_stage_compositor_active(const reach_stage *stage);
// Whether the hover pill is gliding or fading.
int32_t reach_stage_hover_pill_moving(const reach_stage *stage);
// Between compositor events the sampled state is stale; every entry point that reads or changes
// tile geometry brings it to now first.
void reach_stage_sync_compositor(reach_stage *stage);

reach_rect_f32 reach_stage_interpolate_rect(reach_rect_f32 from, reach_rect_f32 to, float factor);
reach_rect_f32 reach_stage_minimized_offscreen_rect(const reach_stage_state *state,
                                                    reach_rect_f32 rect);
void reach_stage_rebuild_layout(reach_stage *stage);

static inline void reach_stage_track_to(reach_stage *stage, size_t track, float target,
                                        reach_motion_spec spec)
{
    reach_motion_animate_spec_to_at(&stage->motions[track], target, spec,
                                    reach_motion_now_seconds());
}

static inline void reach_stage_track_start(reach_stage *stage, size_t track, float from, float to,
                                           reach_motion_spec spec)
{
    reach_motion_set(&stage->motions[track], from);
    reach_stage_track_to(stage, track, to, spec);
}

static inline int32_t reach_stage_track_active(const reach_stage *stage, size_t track)
{
    return reach_motion_active_at(&stage->motions[track], stage->sampled_seconds);
}

static inline float reach_stage_track_value(const reach_stage *stage, size_t track)
{
    return reach_motion_value_at(&stage->motions[track], stage->sampled_seconds);
}

static inline int32_t reach_stage_tracks_active(const reach_stage *stage)
{
    for (size_t track = 0; track < REACH_STAGE_ANIMATION_COUNT; ++track)
        if (reach_stage_track_active(stage, track))
            return 1;
    return 0;
}
void reach_stage_apply_progress(reach_stage *stage);
void reach_stage_start_reflow(reach_stage *stage);
void reach_stage_settle_reflow(reach_stage *stage);
void reach_stage_depart_tile(reach_stage *stage, size_t index);
float reach_stage_tile_bar_height(const reach_stage_state *state);

#endif
