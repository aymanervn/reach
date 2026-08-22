#include "reach/features/common/pressable.h"

static int32_t reach_pressable_feedback_valid(const reach_pressable_feedback_style *feedback)
{
    return feedback != nullptr && feedback->animations != nullptr &&
           feedback->track < feedback->animations->track_count;
}

static void reach_pressable_result_init(reach_pressable_result *out)
{
    if (out != nullptr)
    {
        *out = {};
        out->activated_target = REACH_PRESSABLE_TARGET_NONE;
    }
}

static int32_t reach_pressable_feedback_animate(reach_pressable *pressable, float target,
                                                double seconds, reach_easing easing,
                                                const reach_pressable_feedback_style *feedback)
{
    if (pressable == nullptr || pressable->feedback_index == REACH_PRESSABLE_FEEDBACK_NONE ||
        !reach_pressable_feedback_valid(feedback))
    {
        return 0;
    }
    if (seconds <= 0.0)
    {
        reach_animation_manager_set(feedback->animations, feedback->track, target);
    }
    else
    {
        reach_animation_manager_animate_to(feedback->animations, feedback->track, target, seconds,
                                           easing);
    }
    return 1;
}

static void reach_pressable_end_tracking(reach_pressable *pressable, reach_pressable_result *out)
{
    if (pressable == nullptr || !pressable->tracking)
    {
        return;
    }
    pressable->tracking = 0;
    pressable->target = REACH_PRESSABLE_TARGET_NONE;
    pressable->activation_blocked = 0;
    if (out != nullptr)
    {
        out->capture = -1;
        out->sync_pointer_subscriptions = 1;
    }
}

void reach_pressable_init(reach_pressable *pressable)
{
    if (pressable == nullptr)
    {
        return;
    }
    *pressable = {};
    pressable->target = REACH_PRESSABLE_TARGET_NONE;
    pressable->feedback_index = REACH_PRESSABLE_FEEDBACK_NONE;
}

void reach_pressable_reset(reach_pressable *pressable,
                           const reach_pressable_feedback_style *feedback)
{
    if (pressable == nullptr)
    {
        return;
    }
    if (reach_pressable_feedback_valid(feedback))
    {
        reach_animation_manager_set(feedback->animations, feedback->track, 0.0f);
    }
    reach_pressable_init(pressable);
}

void reach_pressable_press(reach_pressable *pressable, reach_pointer_button button, uint64_t target,
                           size_t feedback_index, const reach_pressable_feedback_style *feedback,
                           reach_pressable_result *out)
{
    reach_pressable_result_init(out);
    if (pressable == nullptr || pressable->tracking || target == REACH_PRESSABLE_TARGET_NONE)
    {
        return;
    }
    pressable->target = target;
    pressable->button = button;
    pressable->tracking = 1;
    pressable->armed = 1;
    pressable->activation_blocked = 0;
    pressable->feedback_latched = 0;
    pressable->feedback_index = feedback_index;
    int32_t redraw = reach_pressable_feedback_animate(
        pressable, feedback != nullptr ? feedback->pressed_value : 0.0f,
        feedback != nullptr ? feedback->press_seconds : 0.0,
        feedback != nullptr ? feedback->press_easing : REACH_EASING_EASE_IN_OUT, feedback);
    if (out != nullptr)
    {
        out->redraw = redraw;
        out->capture = 1;
        out->sync_pointer_subscriptions = 1;
    }
}

static void reach_pressable_release_feedback(reach_pressable *pressable,
                                             const reach_pressable_feedback_style *feedback,
                                             reach_pressable_result *out)
{
    if (pressable == nullptr)
    {
        return;
    }
    pressable->feedback_latched = 0;
    int32_t redraw = reach_pressable_feedback_animate(
        pressable, 0.0f, feedback != nullptr ? feedback->release_seconds : 0.0,
        feedback != nullptr ? feedback->release_easing : REACH_EASING_EASE_IN_OUT, feedback);
    if (out != nullptr)
    {
        out->redraw |= redraw;
    }
}

void reach_pressable_disarm(reach_pressable *pressable,
                            const reach_pressable_feedback_style *feedback,
                            reach_pressable_result *out)
{
    reach_pressable_result_init(out);
    if (pressable == nullptr)
    {
        return;
    }
    pressable->activation_blocked = 1;
    pressable->armed = 0;
    reach_pressable_release_feedback(pressable, feedback, out);
}

void reach_pressable_update(reach_pressable *pressable, uint64_t target,
                            reach_pressable_result *out)
{
    reach_pressable_result_init(out);
    if (pressable == nullptr || !pressable->tracking)
    {
        return;
    }
    pressable->armed = !pressable->activation_blocked && target == pressable->target;
}

void reach_pressable_release(reach_pressable *pressable, reach_pointer_button button,
                             uint64_t target, const reach_pressable_feedback_style *feedback,
                             reach_pressable_result *out)
{
    reach_pressable_result_init(out);
    if (pressable == nullptr || !pressable->tracking || button != pressable->button)
    {
        return;
    }
    uint64_t pressed_target = pressable->target;
    int32_t activated = pressable->tracking && pressable->armed && target == pressed_target;
    pressable->armed = 0;
    reach_pressable_release_feedback(pressable, feedback, out);
    reach_pressable_end_tracking(pressable, out);
    if (out != nullptr && activated)
    {
        out->activated = 1;
        out->activated_target = pressed_target;
    }
}

void reach_pressable_cancel(reach_pressable *pressable,
                            const reach_pressable_feedback_style *feedback,
                            reach_pressable_result *out)
{
    reach_pressable_result_init(out);
    if (pressable == nullptr)
    {
        return;
    }
    pressable->armed = 0;
    reach_pressable_release_feedback(pressable, feedback, out);
    reach_pressable_end_tracking(pressable, out);
}

int32_t reach_pressable_tracking(const reach_pressable *pressable)
{
    return pressable != nullptr && pressable->tracking;
}

int32_t reach_pressable_armed(const reach_pressable *pressable)
{
    return pressable != nullptr && pressable->armed;
}

reach_pointer_button reach_pressable_button(const reach_pressable *pressable)
{
    return pressable != nullptr ? pressable->button : REACH_POINTER_BUTTON_PRIMARY;
}

int32_t reach_pressable_latch_feedback(reach_pressable *pressable,
                                       const reach_pressable_feedback_style *feedback)
{
    if (pressable == nullptr || pressable->feedback_index == REACH_PRESSABLE_FEEDBACK_NONE)
    {
        return 0;
    }
    pressable->armed = 0;
    pressable->feedback_latched = 1;
    return reach_pressable_feedback_animate(pressable,
                                            feedback != nullptr ? feedback->pressed_value : 0.0f,
                                            0.0, REACH_EASING_EASE_IN_OUT, feedback);
}

int32_t reach_pressable_clear_latched_feedback(reach_pressable *pressable,
                                               const reach_pressable_feedback_style *feedback)
{
    if (pressable == nullptr || !pressable->feedback_latched)
    {
        return 0;
    }
    pressable->feedback_latched = 0;
    return reach_pressable_feedback_animate(
        pressable, 0.0f, feedback != nullptr ? feedback->release_seconds : 0.0,
        feedback != nullptr ? feedback->release_easing : REACH_EASING_EASE_IN_OUT, feedback);
}

void reach_pressable_settle_feedback(reach_pressable *pressable,
                                     const reach_pressable_feedback_style *feedback)
{
    if (pressable == nullptr || pressable->feedback_index == REACH_PRESSABLE_FEEDBACK_NONE ||
        pressable->tracking || pressable->armed || pressable->feedback_latched ||
        !reach_pressable_feedback_valid(feedback) ||
        reach_animation_manager_active(feedback->animations, feedback->track) ||
        reach_animation_manager_value(feedback->animations, feedback->track) > 0.001f)
    {
        return;
    }
    reach_animation_manager_set(feedback->animations, feedback->track, 0.0f);
    pressable->feedback_index = REACH_PRESSABLE_FEEDBACK_NONE;
}

size_t reach_pressable_feedback_index(const reach_pressable *pressable)
{
    return pressable != nullptr ? pressable->feedback_index : REACH_PRESSABLE_FEEDBACK_NONE;
}

float reach_pressable_feedback_value(const reach_pressable *pressable,
                                     const reach_pressable_feedback_style *feedback)
{
    return pressable != nullptr && pressable->feedback_index != REACH_PRESSABLE_FEEDBACK_NONE &&
                   reach_pressable_feedback_valid(feedback)
               ? reach_animation_manager_value(feedback->animations, feedback->track)
               : 0.0f;
}
