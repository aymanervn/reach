#ifndef REACH_FEATURES_FEATURE_CAPSULE_H
#define REACH_FEATURES_FEATURE_CAPSULE_H

#include <stddef.h>
#include <stdint.h>

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
        REACH_POINTER_EVENT_CANCEL = 6,
        REACH_POINTER_EVENT_CONTEXT = 7,
        REACH_POINTER_EVENT_MIDDLE = 8
    } reach_pointer_event_kind;

    typedef struct reach_pointer_event
    {
        reach_pointer_event_kind kind;
        int32_t x;
        int32_t y;
        int32_t wheel_delta;
        uint32_t modifiers;
    } reach_pointer_event;

    typedef struct reach_capsule_action
    {
        uint32_t kind;
        size_t index;
        uint64_t id;
        uintptr_t window;
        float value;
    } reach_capsule_action;

    typedef struct reach_capsule_pointer_result
    {
        int32_t handled;
        int32_t redraw;
        int32_t relayout;
        int32_t capture;
        int32_t sync_pointer_subscriptions;
        reach_capsule_action action;
    } reach_capsule_pointer_result;

    typedef struct reach_feature_capsule_ops
    {

        void (*reset)(void *capsule);

        void (*tick)(void *capsule, double delta_seconds, reach_feature_tick_result *out);
        int32_t (*is_open)(const void *capsule);

        void (*force_close)(void *capsule);
        void (*on_game_mode)(void *capsule, int32_t enabled);

        int32_t (*needs_frame)(const void *capsule);

        int32_t (*wants_pointer_move)(const void *capsule);

        void (*handle_pointer)(void *capsule, const reach_pointer_event *event,
                               reach_capsule_pointer_result *out);
    } reach_feature_capsule_ops;

#ifdef __cplusplus
}
#endif

#endif
