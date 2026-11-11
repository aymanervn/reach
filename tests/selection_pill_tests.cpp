#include "reach/features/common/selection_pill.h"

#include "motion_clock_test_support.h"

#include <math.h>
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

static int32_t uses_preset(const reach_motion &motion, reach_motion_preset preset)
{
    reach_motion_spec spec = reach_theme_motion_preset(reach_theme_default(), preset);
    return motion.spring_response == spec.response_seconds && motion.spring_damping == spec.damping;
}

static reach_selection_pill settled_pill(void)
{
    reach_selection_pill pill = {};
    reach_selection_pill_retarget(&pill, reach_theme_default(), {10, 20, 100, 40}, 2,
                                  REACH_SELECTION_PILL_SET, reach_motion_now_seconds());
    return pill;
}

static void test_mode_for_distance_matches_adjacency(void)
{
    expect_true(reach_selection_pill_mode_for_distance(2, 3) == REACH_SELECTION_PILL_STEP &&
                    reach_selection_pill_mode_for_distance(3, 2) == REACH_SELECTION_PILL_STEP,
                "adjacent slots step");
    expect_true(reach_selection_pill_mode_for_distance(0, 8) == REACH_SELECTION_PILL_JUMP &&
                    reach_selection_pill_mode_for_distance(1, 4) == REACH_SELECTION_PILL_JUMP &&
                    reach_selection_pill_mode_for_distance(2, 2) == REACH_SELECTION_PILL_JUMP,
                "wrap, multi-slot and same-slot moves jump");
}

static void test_step_uses_bouncy_and_jump_uses_smooth(void)
{
    reach_selection_pill pill = settled_pill();
    reach_selection_pill_retarget(&pill, reach_theme_default(), {130, 20, 100, 40}, 3,
                                  REACH_SELECTION_PILL_STEP, reach_motion_now_seconds());
    expect_true(pill.slot == 3 && reach_motion_active(&pill.rect.x) &&
                    uses_preset(pill.rect.x, REACH_MOTION_PRESET_BOUNCY),
                "step retargets with the bouncy preset");
    pill = settled_pill();
    reach_selection_pill_retarget(&pill, reach_theme_default(), {490, 20, 100, 40}, 6,
                                  REACH_SELECTION_PILL_JUMP, reach_motion_now_seconds());
    expect_true(pill.slot == 6 && reach_motion_active(&pill.rect.x) &&
                    uses_preset(pill.rect.x, REACH_MOTION_PRESET_SMOOTH),
                "jump retargets with the smooth preset");
}

static void test_set_does_not_move(void)
{
    reach_selection_pill pill = settled_pill();
    reach_selection_pill_retarget(&pill, reach_theme_default(), {300, 30, 80, 50}, 5,
                                  REACH_SELECTION_PILL_SET, reach_motion_now_seconds());
    reach_rect_f32 now = reach_rect_motion_value_at(&pill.rect, reach_motion_now_seconds());
    expect_true(!reach_motion_active(&pill.rect.x) && !reach_motion_active(&pill.rect.width) &&
                    now.x == 300 && now.y == 30 && now.width == 80 && now.height == 50 &&
                    pill.slot == 5,
                "set places the pill without motion");
}

static void test_unchanged_channels_are_not_restarted(void)
{
    reach_selection_pill pill = settled_pill();
    reach_motion y = pill.rect.y;
    reach_motion height = pill.rect.height;
    reach_selection_pill_retarget(&pill, reach_theme_default(), {130, 20, 120, 40}, 3,
                                  REACH_SELECTION_PILL_STEP, reach_motion_now_seconds());
    expect_true(reach_motion_equal(&pill.rect.y, &y) &&
                    reach_motion_equal(&pill.rect.height, &height),
                "unchanged channels keep their motion");
    reach_test_motion_clock_advance(0.05);
    reach_motion width = pill.rect.width;
    reach_selection_pill_retarget(&pill, reach_theme_default(), {250, 20, 120, 40}, 4,
                                  REACH_SELECTION_PILL_STEP, reach_motion_now_seconds());
    expect_true(reach_motion_equal(&pill.rect.width, &width),
                "an in-flight channel with an unchanged target is not restarted");
}

static void test_mid_flight_retarget_keeps_value_and_velocity(void)
{
    reach_selection_pill pill = settled_pill();
    reach_selection_pill_retarget(&pill, reach_theme_default(), {130, 20, 100, 40}, 3,
                                  REACH_SELECTION_PILL_STEP, reach_motion_now_seconds());
    reach_test_motion_clock_advance(0.05);
    double now = reach_motion_now_seconds();
    float value = reach_motion_value_at(&pill.rect.x, now);
    float velocity = reach_motion_velocity_at(&pill.rect.x, now);
    expect_true(fabsf(velocity) > 1.0f, "pill is moving before the retarget");
    reach_selection_pill_retarget(&pill, reach_theme_default(), {610, 20, 100, 40}, 7,
                                  REACH_SELECTION_PILL_JUMP, now);
    expect_true(fabsf(reach_motion_value_at(&pill.rect.x, now) - value) <= 0.01f,
                "retarget keeps the current value");
    expect_true(fabsf(reach_motion_velocity_at(&pill.rect.x, now) - velocity) <= 1.0f,
                "retarget keeps the current velocity");
}

int main(void)
{
    test_mode_for_distance_matches_adjacency();
    test_step_uses_bouncy_and_jump_uses_smooth();
    test_set_does_not_move();
    test_unchanged_channels_are_not_restarted();
    test_mid_flight_retarget_keeps_value_and_velocity();
    return failures == 0 ? 0 : 1;
}
