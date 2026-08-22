#ifndef REACH_FEATURES_COMMON_PRESSABLE_H
#define REACH_FEATURES_COMMON_PRESSABLE_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/ui_events.h"
#include "reach/support/animation.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_PRESSABLE_TARGET_NONE UINT64_MAX
#define REACH_PRESSABLE_FEEDBACK_NONE SIZE_MAX

    typedef struct reach_pressable_feedback_style
    {
        reach_animation_manager *animations;
        size_t track;
        float pressed_value;
        double press_seconds;
        double release_seconds;
        reach_easing press_easing;
        reach_easing release_easing;
    } reach_pressable_feedback_style;

    typedef struct reach_pressable
    {
        uint64_t target;
        size_t feedback_index;
        reach_pointer_button button;
        int32_t tracking;
        int32_t armed;
        int32_t activation_blocked;
        int32_t feedback_latched;
    } reach_pressable;

    typedef struct reach_pressable_result
    {
        uint64_t activated_target;
        int32_t activated;
        int32_t redraw;
        int32_t capture;
        int32_t sync_pointer_subscriptions;
    } reach_pressable_result;

    void reach_pressable_init(reach_pressable *pressable);
    void reach_pressable_reset(reach_pressable *pressable,
                               const reach_pressable_feedback_style *feedback);
    void reach_pressable_press(reach_pressable *pressable, reach_pointer_button button,
                               uint64_t target, size_t feedback_index,
                               const reach_pressable_feedback_style *feedback,
                               reach_pressable_result *out);
    void reach_pressable_update(reach_pressable *pressable, uint64_t target,
                                reach_pressable_result *out);
    void reach_pressable_disarm(reach_pressable *pressable,
                                const reach_pressable_feedback_style *feedback,
                                reach_pressable_result *out);
    void reach_pressable_release(reach_pressable *pressable, reach_pointer_button button,
                                 uint64_t target, const reach_pressable_feedback_style *feedback,
                                 reach_pressable_result *out);
    void reach_pressable_cancel(reach_pressable *pressable,
                                const reach_pressable_feedback_style *feedback,
                                reach_pressable_result *out);

    int32_t reach_pressable_tracking(const reach_pressable *pressable);
    int32_t reach_pressable_armed(const reach_pressable *pressable);
    reach_pointer_button reach_pressable_button(const reach_pressable *pressable);

    int32_t reach_pressable_latch_feedback(reach_pressable *pressable,
                                           const reach_pressable_feedback_style *feedback);
    int32_t reach_pressable_clear_latched_feedback(reach_pressable *pressable,
                                                   const reach_pressable_feedback_style *feedback);
    void reach_pressable_settle_feedback(reach_pressable *pressable,
                                         const reach_pressable_feedback_style *feedback);
    size_t reach_pressable_feedback_index(const reach_pressable *pressable);
    float reach_pressable_feedback_value(const reach_pressable *pressable,
                                         const reach_pressable_feedback_style *feedback);

#ifdef __cplusplus
}
#endif

#endif
