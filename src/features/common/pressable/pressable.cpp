#include "reach/features/common/pressable.h"

#include <math.h>

#include "reach/core/theme.h"

static int32_t reach_pressable_feedback_valid(const reach_pressable_feedback_style *feedback)
{
    return feedback != nullptr && feedback->motion != nullptr;
}

static void reach_pressable_result_init(reach_pressable_result *out)
{
    if (out != nullptr)
    {
        *out = {};
        out->activated_target = REACH_PRESSABLE_TARGET_NONE;
    }
}

static int32_t reach_pressable_feedback_animate(reach_pressable *pressable, int32_t pressed,
                                                const reach_pressable_feedback_style *feedback)
{
    if (pressable == nullptr || pressable->feedback_index == REACH_PRESSABLE_FEEDBACK_NONE ||
        !reach_pressable_feedback_valid(feedback))
    {
        return 0;
    }
    reach_motion_animate_spec_to_at(
        feedback->motion, pressed ? feedback->pressed_value : 0.0f,
        reach_theme_motion_preset(nullptr, pressed ? REACH_MOTION_PRESET_PRESS
                                                   : REACH_MOTION_PRESET_RELEASE),
        reach_motion_now_seconds());
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
        reach_motion_set(feedback->motion, 0.0f);
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
    int32_t redraw = reach_pressable_feedback_animate(pressable, 1, feedback);
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
    int32_t redraw = reach_pressable_feedback_animate(pressable, 0, feedback);
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
    return reach_pressable_feedback_animate(pressable, 1, feedback);
}

int32_t reach_pressable_clear_latched_feedback(reach_pressable *pressable,
                                               const reach_pressable_feedback_style *feedback)
{
    if (pressable == nullptr || !pressable->feedback_latched)
    {
        return 0;
    }
    pressable->feedback_latched = 0;
    return reach_pressable_feedback_animate(pressable, 0, feedback);
}

void reach_pressable_settle_feedback(reach_pressable *pressable,
                                     const reach_pressable_feedback_style *feedback)
{
    if (pressable == nullptr || pressable->feedback_index == REACH_PRESSABLE_FEEDBACK_NONE ||
        pressable->tracking || pressable->armed || pressable->feedback_latched ||
        !reach_pressable_feedback_valid(feedback) || reach_motion_active(feedback->motion) ||
        fabsf(reach_motion_value(feedback->motion)) > 0.001f)
    {
        return;
    }
    reach_motion_set(feedback->motion, 0.0f);
    pressable->feedback_index = REACH_PRESSABLE_FEEDBACK_NONE;
}

size_t reach_pressable_feedback_index(const reach_pressable *pressable)
{
    return pressable != nullptr ? pressable->feedback_index : REACH_PRESSABLE_FEEDBACK_NONE;
}
