#ifndef REACH_FEATURES_COMMON_BAR_VISIBILITY_H
#define REACH_FEATURES_COMMON_BAR_VISIBILITY_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/geometry.h"
#include "reach/support/animation.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_bar_edge
    {
        REACH_BAR_EDGE_BOTTOM = 0,
        REACH_BAR_EDGE_TOP = 1
    } reach_bar_edge;

    typedef struct reach_bar_visibility_state
    {
        int32_t target_hidden;
        int32_t reveal_session_active;
        int32_t animation_initialized;
    } reach_bar_visibility_state;

    typedef struct reach_bar_visibility_request
    {
        reach_bar_edge edge;
        reach_rect_f32 shown_bounds;
        reach_rect_f32 monitor_bounds;
        reach_point_i32 pointer;
        int32_t pointer_valid;
        int32_t can_hide;
        int32_t pointer_sequence_active;
        int32_t force_shown;
        int32_t force_hidden;
        int32_t hold_open;
        uintptr_t excluded_window;
        float reveal_span_inset;
        float reveal_seconds;
        float shadow_clearance;
    } reach_bar_visibility_request;

    typedef struct reach_bar_visibility_result
    {
        reach_rect_f32 animated_bounds;
        reach_rect_f32 reveal_bounds;
        reach_rect_f32 pointer_observation_bounds;
        float reveal_progress;
        int32_t reveal_transition_active;
        int32_t hover_revealed;
        int32_t reveal_edge_shown;
        int32_t pointer_observation_active;
        int32_t visible;
        int32_t redraw;
    } reach_bar_visibility_result;

    typedef struct reach_bar_reveal_animation
    {
        int32_t position_animating;
        int32_t content_animating;
        float animated_y;
    } reach_bar_reveal_animation;

    typedef struct reach_bar_reveal_ops
    {
        void (*begin_session)(void *capsule);
        reach_bar_visibility_result (*update_visibility)(
            void *capsule, const reach_bar_visibility_request *request);
        reach_bar_reveal_animation (*animation)(const void *capsule);
        void (*position_frame)(void *capsule);
        void (*invalidate_coverage)(void *capsule);
    } reach_bar_reveal_ops;

    void reach_bar_visibility_reset(reach_bar_visibility_state *state);
    void reach_bar_begin_reveal_session(reach_bar_visibility_state *state);

    float reach_bar_hidden_position(reach_bar_edge edge, reach_rect_f32 shown_bounds,
                                    reach_rect_f32 monitor_bounds, float shadow_clearance);
    float reach_bar_reveal_progress(float animated_y, float shown_y, float hidden_y);
    reach_rect_f32 reach_bar_protected_band(reach_bar_edge edge, reach_rect_f32 shown_bounds,
                                            reach_rect_f32 monitor_bounds, float shadow_clearance);

    reach_bar_visibility_result
    reach_bar_update_visibility(reach_bar_visibility_state *state, reach_animation_manager *manager,
                                size_t y_track, const reach_bar_visibility_request *request);

#ifdef __cplusplus
}
#endif

#endif
