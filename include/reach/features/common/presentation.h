#ifndef REACH_FEATURES_COMMON_PRESENTATION_H
#define REACH_FEATURES_COMMON_PRESENTATION_H

#include "reach/core/theme.h"
#include "reach/features/feature_capsule.h"
#include "reach/support/animation.h"

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
        reach_animation_manager animations;
        reach_animation_track tracks[2];
        double open_seconds;
        double close_seconds;
        float dpi_scale;
        int32_t direction;
        int32_t visible;
        int32_t target_open;
    } reach_feature_transition;

    void reach_feature_transition_init(reach_feature_transition *transition, int32_t direction);
    void reach_feature_transition_reset(reach_feature_transition *transition);
    void reach_feature_transition_configure(reach_feature_transition *transition,
                                            const reach_theme *theme, float dpi_scale,
                                            int32_t direction);
    int32_t reach_feature_transition_set_open(reach_feature_transition *transition, int32_t open);
    int32_t reach_feature_transition_tick(reach_feature_transition *transition,
                                          double delta_seconds);
    int32_t reach_feature_transition_active(const reach_feature_transition *transition);
    int32_t reach_feature_transition_visible(const reach_feature_transition *transition);
    void reach_feature_transition_presentation(const reach_feature_transition *transition,
                                               reach_feature_surface_geometry *geometry);

#ifdef __cplusplus
}
#endif

#endif
