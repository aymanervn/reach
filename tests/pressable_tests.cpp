#include "reach/features/common/pressable.h"
#include "reach/features/common/draggable.h"

#include <stdio.h>

static int failures = 0;

static void expect_true(int32_t condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static reach_pressable_feedback_style feedback_style(reach_animation_manager *manager)
{
    reach_pressable_feedback_style style = {};
    style.animations = manager;
    style.track = 0;
    style.pressed_value = 0.5f;
    style.press_seconds = 0.055;
    style.release_seconds = 0.055;
    style.press_easing = REACH_EASING_EASE_IN_OUT;
    style.release_easing = REACH_EASING_EASE_IN_OUT;
    return style;
}

static void test_release_on_original_target_activates(void)
{
    reach_animation_track track = {};
    reach_animation_manager manager = {};
    reach_animation_manager_init(&manager, &track, 1);
    reach_pressable_feedback_style feedback = feedback_style(&manager);
    reach_pressable pressable = {};
    reach_pressable_init(&pressable);

    reach_pressable_result down = {};
    reach_pressable_press(&pressable, REACH_POINTER_BUTTON_PRIMARY, 7, 3, &feedback, &down);
    expect_true(reach_pressable_tracking(&pressable), "press starts pointer tracking");
    expect_true(reach_pressable_armed(&pressable), "press arms its target");
    expect_true(down.capture == 1, "press requests pointer capture");
    expect_true(reach_pressable_feedback_index(&pressable) == 3,
                "press selects its feedback target");

    reach_pressable_result up = {};
    reach_pressable_release(&pressable, REACH_POINTER_BUTTON_PRIMARY, 7, &feedback, &up);
    expect_true(up.activated && up.activated_target == 7,
                "release on the original target activates it");
    expect_true(up.capture == -1, "release gives up pointer capture");
    expect_true(!reach_pressable_tracking(&pressable), "release ends pointer tracking");
}

static void test_leave_temporarily_disarms_click(void)
{
    reach_animation_track track = {};
    reach_animation_manager manager = {};
    reach_animation_manager_init(&manager, &track, 1);
    reach_pressable_feedback_style feedback = feedback_style(&manager);
    reach_pressable pressable = {};
    reach_pressable_init(&pressable);
    reach_pressable_result result = {};
    reach_pressable_press(&pressable, REACH_POINTER_BUTTON_PRIMARY, 11, 2, &feedback, &result);
    reach_animation_manager_tick(&manager, 1.0);

    reach_pressable_update(&pressable, REACH_PRESSABLE_TARGET_NONE, &result);
    expect_true(!reach_pressable_armed(&pressable), "leaving disarms the target");
    expect_true(reach_pressable_tracking(&pressable), "leaving retains the active sequence");
    expect_true(result.capture == 0, "leaving retains pointer capture until release");
    expect_true(reach_animation_manager_value(&manager, 0) == feedback.pressed_value,
                "leaving keeps feedback pressed on the original target");

    reach_pressable_release(&pressable, REACH_POINTER_BUTTON_PRIMARY, 12, &feedback, &result);
    expect_true(!result.activated, "release away from the original target does not activate");
    expect_true(result.capture == -1, "release away ends the sequence");
    expect_true(reach_animation_manager_target(&manager, 0) == 0.0f,
                "release away starts feedback release");
}

static void test_reentering_the_original_target_rearms(void)
{
    reach_animation_track track = {};
    reach_animation_manager manager = {};
    reach_animation_manager_init(&manager, &track, 1);
    reach_pressable_feedback_style feedback = feedback_style(&manager);
    reach_pressable pressable = {};
    reach_pressable_init(&pressable);
    reach_pressable_result result = {};
    reach_pressable_press(&pressable, REACH_POINTER_BUTTON_PRIMARY, 13, 4, &feedback, &result);

    reach_pressable_update(&pressable, 14, &result);
    expect_true(!reach_pressable_armed(&pressable), "another target cannot inherit the press");
    reach_pressable_settle_feedback(&pressable, &feedback);
    expect_true(reach_pressable_feedback_index(&pressable) == 4,
                "feedback identity survives while the pointer remains held away");
    expect_true(reach_animation_manager_target(&manager, 0) == feedback.pressed_value,
                "feedback remains pressed while held away");
    reach_pressable_update(&pressable, 13, &result);
    expect_true(reach_pressable_armed(&pressable), "returning to the original target rearms it");
    expect_true(reach_animation_manager_target(&manager, 0) == feedback.pressed_value,
                "returning preserves uninterrupted feedback on the original target");
    reach_pressable_release(&pressable, REACH_POINTER_BUTTON_PRIMARY, 13, &feedback, &result);
    expect_true(result.activated && result.activated_target == 13,
                "release after re-entry activates only the original target");
}

static void test_drag_start_disarms_before_release(void)
{
    reach_pressable pressable = {};
    reach_pressable_init(&pressable);
    reach_pressable_result result = {};
    reach_pressable_press(&pressable, REACH_POINTER_BUTTON_PRIMARY, 17,
                          REACH_PRESSABLE_FEEDBACK_NONE, nullptr, &result);
    reach_pressable_disarm(&pressable, nullptr, &result);
    reach_pressable_update(&pressable, REACH_PRESSABLE_TARGET_NONE, &result);
    reach_pressable_update(&pressable, 17, &result);
    reach_pressable_release(&pressable, REACH_POINTER_BUTTON_PRIMARY, 17, nullptr, &result);
    expect_true(!result.activated, "a gesture blocked by drag cannot rearm or activate");
}

static void test_cancel_clears_feedback_and_capture(void)
{
    reach_animation_track track = {};
    reach_animation_manager manager = {};
    reach_animation_manager_init(&manager, &track, 1);
    reach_pressable_feedback_style feedback = feedback_style(&manager);
    reach_pressable pressable = {};
    reach_pressable_init(&pressable);
    reach_pressable_result result = {};
    reach_pressable_press(&pressable, REACH_POINTER_BUTTON_PRIMARY, 19, 5, &feedback, &result);
    reach_pressable_cancel(&pressable, &feedback, &result);
    expect_true(!reach_pressable_tracking(&pressable) && !reach_pressable_armed(&pressable),
                "cancel clears press state");
    expect_true(result.capture == -1, "cancel releases pointer capture");
    expect_true(reach_animation_manager_target(&manager, 0) == 0.0f,
                "cancel starts feedback release");
}

static void test_release_requires_the_pressed_button(void)
{
    reach_pressable pressable = {};
    reach_pressable_init(&pressable);
    reach_pressable_result result = {};
    reach_pressable_press(&pressable, REACH_POINTER_BUTTON_SECONDARY, 23,
                          REACH_PRESSABLE_FEEDBACK_NONE, nullptr, &result);

    reach_pressable_release(&pressable, REACH_POINTER_BUTTON_PRIMARY, 23, nullptr, &result);
    expect_true(!result.activated, "another button cannot complete the press");
    expect_true(reach_pressable_tracking(&pressable),
                "another button release leaves the press sequence active");
    expect_true(result.capture == 0, "another button release retains pointer capture");

    reach_pressable_release(&pressable, REACH_POINTER_BUTTON_SECONDARY, 23, nullptr, &result);
    expect_true(result.activated, "the pressed button completes its own sequence");
    expect_true(result.capture == -1, "the pressed button release gives up pointer capture");
}

static void test_draggable_starts_only_after_threshold(void)
{
    reach_draggable draggable = {};
    reach_draggable_init(&draggable);
    reach_draggable_begin(&draggable, 31, 10, 20);

    reach_draggable_result result = {};
    reach_draggable_update(&draggable, 13, 24, 36, &result);
    expect_true(!result.started && !result.moved,
                "movement below the drag threshold remains a click candidate");

    reach_draggable_update(&draggable, 16, 20, 36, &result);
    expect_true(result.started && result.moved,
                "movement at the drag threshold starts the drag once");

    reach_draggable_update(&draggable, 18, 20, 36, &result);
    expect_true(!result.started && result.moved,
                "continued drag movement does not restart the gesture");
}

static void test_draggable_end_reports_and_clears_state(void)
{
    reach_draggable draggable = {};
    reach_draggable_init(&draggable);
    reach_draggable_begin(&draggable, 37, 0, 0);
    reach_draggable_result result = {};
    reach_draggable_update(&draggable, 6, 0, 36, &result);
    reach_draggable_end(&draggable, &result);

    expect_true(result.ended && result.moved && result.target == 37,
                "drag end reports the completed target and movement");
    expect_true(!reach_draggable_tracking(&draggable) && !reach_draggable_moved(&draggable),
                "drag end clears reusable gesture state");
}

static void test_horizontal_reorder_uses_neighbor_thresholds(void)
{
    reach_rect_f32 slots[3] = {
        {0.0f, 0.0f, 20.0f, 20.0f}, {24.0f, 0.0f, 20.0f, 20.0f}, {48.0f, 0.0f, 20.0f, 20.0f}};
    expect_true(reach_horizontal_reorder_target(slots, 3, 1, 2.0f, 0.25f) == 0,
                "dragging through the left neighbor threshold reorders left");
    expect_true(reach_horizontal_reorder_target(slots, 3, 1, 66.0f, 0.25f) == 2,
                "dragging through the right neighbor threshold reorders right");
    expect_true(reach_horizontal_reorder_target(slots, 3, 1, 24.0f, 0.25f) == 1,
                "remaining between neighbor thresholds retains the current slot");
}

int main(void)
{
    test_release_on_original_target_activates();
    test_leave_temporarily_disarms_click();
    test_reentering_the_original_target_rearms();
    test_drag_start_disarms_before_release();
    test_cancel_clears_feedback_and_capture();
    test_release_requires_the_pressed_button();
    test_draggable_starts_only_after_threshold();
    test_draggable_end_reports_and_clears_state();
    test_horizontal_reorder_uses_neighbor_thresholds();
    return failures == 0 ? 0 : 1;
}
