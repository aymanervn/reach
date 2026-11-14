#ifndef REACH_FEATURES_COMMON_PRESENTATION_H
#define REACH_FEATURES_COMMON_PRESENTATION_H

#include "reach/core/theme.h"
#include "reach/features/feature_capsule.h"
#include "reach/support/motion.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_feature_transition_direction
    {
        REACH_FEATURE_TRANSITION_FROM_BELOW = 0,
        REACH_FEATURE_TRANSITION_FROM_ABOVE = 1
    } reach_feature_transition_direction;

    typedef struct reach_feature_transition
    {
        reach_motion opacity;
        reach_motion_spec fade_in;
        reach_motion_spec fade_out;
        double completion_seconds;
        float dpi_scale;
        float anchor_x;
        float anchor_y;
        int32_t has_anchor;
        int32_t direction;
        int32_t visible;
        int32_t target_open;
    } reach_feature_transition;

    void reach_feature_transition_init(reach_feature_transition *transition, int32_t direction);
    void reach_feature_transition_reset(reach_feature_transition *transition);
    void reach_feature_transition_configure(reach_feature_transition *transition,
                                            const reach_theme *theme, float dpi_scale,
                                            int32_t direction);
    void reach_feature_transition_set_anchor(reach_feature_transition *transition, float x,
                                             float y);
    void reach_feature_transition_set_trigger(reach_feature_transition *transition,
                                              reach_rect_f32 surface_bounds, float trigger_x);
    int32_t reach_feature_transition_set_open(reach_feature_transition *transition, int32_t open);
    int32_t reach_feature_transition_tick(reach_feature_transition *transition,
                                          double delta_seconds);
    int32_t reach_feature_transition_active(const reach_feature_transition *transition);
    int32_t reach_feature_transition_needs_update(const reach_feature_transition *transition);
    int32_t reach_feature_transition_visible(const reach_feature_transition *transition);
    double reach_feature_transition_exit_seconds(const reach_feature_transition *transition);
    void reach_feature_transition_presentation(const reach_feature_transition *transition,
                                               reach_feature_surface_geometry *geometry);

#ifdef __cplusplus
}
#endif

#endif
