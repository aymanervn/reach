#ifndef REACH_ANIMATION_H
#define REACH_ANIMATION_H

#include <stddef.h>
#include <stdint.h>

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_easing
    {
        REACH_EASING_EASE_IN = 0,
        REACH_EASING_EASE_OUT = 1,
        REACH_EASING_EASE_IN_OUT = 2
    } reach_easing;

    typedef struct reach_animation_track
    {
        float from;
        float to;
        float value;
        double elapsed_seconds;
        double duration_seconds;
        reach_easing easing;
        int32_t active;
    } reach_animation_track;

    typedef struct reach_animation_manager
    {
        reach_animation_track *tracks;
        size_t track_count;
    } reach_animation_manager;

    void reach_animation_manager_init(reach_animation_manager *manager,
                                      reach_animation_track *tracks, size_t track_count);
    void reach_animation_manager_tick(reach_animation_manager *manager, double delta_seconds);
    void reach_animation_manager_set(reach_animation_manager *manager, size_t track_id,
                                     float value);
    void reach_animation_manager_start(reach_animation_manager *manager, size_t track_id,
                                       float from, float to, double duration_seconds,
                                       reach_easing easing);
    void reach_animation_manager_animate_to(reach_animation_manager *manager, size_t track_id,
                                            float to, double duration_seconds,
                                            reach_easing easing);
    void reach_animation_manager_reset(reach_animation_manager *manager, size_t track_id);
    float reach_animation_manager_value(const reach_animation_manager *manager, size_t track_id);
    float reach_animation_manager_target(const reach_animation_manager *manager, size_t track_id);
    int32_t reach_animation_manager_active(const reach_animation_manager *manager, size_t track_id);
    int32_t reach_animation_manager_any_active(const reach_animation_manager *manager);

#ifdef __cplusplus
}
#endif

#endif
