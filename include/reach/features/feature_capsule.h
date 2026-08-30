#ifndef REACH_FEATURES_FEATURE_CAPSULE_H
#define REACH_FEATURES_FEATURE_CAPSULE_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/geometry.h"
#include "reach/core/ui_events.h"
#include "reach/features/common/feature_action.h"
#include "reach/features/common/feature_target.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_feature_tick_result
    {
        int32_t redraw;
        int32_t relayout;
        int32_t request_update;
    } reach_feature_tick_result;

    typedef enum reach_pointer_event_kind
    {
        REACH_POINTER_EVENT_DOWN = 1,
        REACH_POINTER_EVENT_UP = 2,
        REACH_POINTER_EVENT_MOVE = 3,
        REACH_POINTER_EVENT_WHEEL = 4,
        REACH_POINTER_EVENT_LEAVE = 5,
        REACH_POINTER_EVENT_CANCEL = 6
    } reach_pointer_event_kind;

    typedef enum reach_pointer_coordinate_space
    {
        REACH_POINTER_COORDINATE_SCREEN = 0,
        REACH_POINTER_COORDINATE_SURFACE_LOCAL = 1
    } reach_pointer_coordinate_space;

    typedef enum reach_pointer_surface_relation
    {
        REACH_POINTER_SURFACE_UNKNOWN = 0,
        REACH_POINTER_SURFACE_INSIDE = 1,
        REACH_POINTER_SURFACE_OUTSIDE = 2
    } reach_pointer_surface_relation;

    typedef struct reach_feature_control
    {
        uint32_t slot;
        size_t index;
        int32_t valid;
        int32_t popup_trigger;
    } reach_feature_control;

    typedef struct reach_pointer_event
    {
        reach_pointer_event_kind kind;
        reach_pointer_coordinate_space coordinate_space;
        int32_t x;
        int32_t y;
        int32_t wheel_delta;
        uint32_t modifiers;
        reach_pointer_button button;
        reach_pointer_surface_relation surface_relation;
        int32_t owner_trigger;
    } reach_pointer_event;

    typedef struct reach_capsule_action
    {
        uint32_t kind;
        uint32_t flags;
        size_t index;
        uint64_t id;
        uintptr_t window;
        float value;

        reach_feature_target target;
        const uintptr_t *windows;
        size_t window_count;
    } reach_capsule_action;

    typedef struct reach_capsule_event_result
    {
        int32_t handled;
        int32_t redraw;
        int32_t relayout;
        int32_t request_update;
        reach_capsule_action action;
    } reach_capsule_event_result;

    typedef struct reach_capsule_pointer_result
    {
        int32_t handled;
        int32_t redraw;
        int32_t relayout;
        int32_t capture;
        int32_t sync_pointer_subscriptions;
        int32_t continue_source_sequence;
        int32_t cancel_source_sequence;
        reach_feature_control control;
        reach_capsule_action action;
    } reach_capsule_pointer_result;

    typedef struct reach_feature_surface_geometry
    {
        reach_rect_f32 visible_bounds;
        reach_rect_f32 envelope_bounds;
        float notch_anchor_x;
        int32_t notch_side;
    } reach_feature_surface_geometry;

    typedef struct reach_feature_capsule_ops
    {

        void (*reset)(void *capsule);

        void (*tick)(void *capsule, double delta_seconds, reach_feature_tick_result *out);
        int32_t (*is_open)(const void *capsule);

        void (*on_game_mode)(void *capsule, int32_t enabled);

        int32_t (*needs_frame)(const void *capsule);

        int32_t (*wants_pointer_move)(const void *capsule);

        void (*handle_pointer)(void *capsule, const reach_pointer_event *event,
                               reach_capsule_pointer_result *out);

        int32_t (*pointer_sequence_active)(const void *capsule);

        size_t (*input_regions)(const void *capsule, reach_rect_f32 *out_regions,
                                size_t max_regions);

        void (*surface_geometry)(const void *capsule, reach_feature_surface_geometry *out);

        int32_t (*pointer_capture_active)(const void *capsule);

        void (*handle_event)(void *capsule, const reach_ui_event *event,
                             reach_capsule_event_result *out);

        /* Which of this surface's own controls sits under a screen point, so a press can be
           matched against the control a popup hangs off without naming either feature. */
        int32_t (*control_at_point)(const void *capsule, int32_t screen_x, int32_t screen_y,
                                    reach_feature_control *out);
    } reach_feature_capsule_ops;

#ifdef __cplusplus
}
#endif

#endif
