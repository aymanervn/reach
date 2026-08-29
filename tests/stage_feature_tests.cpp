#include "reach/features/stage.h"

#include <math.h>
#include <stdio.h>

static int failures;

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        ++failures;
        fprintf(stderr, "FAILED: %s\n", message);
    }
}

static void expect_near(float actual, float expected, const char *message)
{
    if (fabsf(actual - expected) > 0.01f)
    {
        ++failures;
        fprintf(stderr, "FAILED: %s (expected %f, got %f)\n", message, (double)expected,
                (double)actual);
    }
}

static reach_rect_f32 make_rect(float x, float y, float width, float height)
{
    reach_rect_f32 rect = {x, y, width, height};
    return rect;
}

static reach_stage_open_window make_window(uintptr_t id, reach_rect_f32 frame)
{
    reach_stage_open_window window = {};
    window.window = id;
    window.frame = frame;
    return window;
}

static void test_open_and_close_state_machine(void)
{
    reach_stage *stage = nullptr;
    expect_true(reach_stage_create(&stage) == REACH_OK && stage != nullptr, "stage is created");
    if (stage == nullptr)
    {
        return;
    }

    expect_true(!reach_stage_is_open(stage), "a new stage is closed");

    expect_true(reach_stage_open(stage, make_rect(0.0f, 0.0f, 1000.0f, 1000.0f), 1.0f, nullptr, 0) != REACH_OK,
                "opening with no windows fails");
    expect_true(!reach_stage_is_open(stage), "a failed open leaves the stage closed");

    reach_stage_open_window windows[2] = {
        make_window(1, make_rect(0.0f, 0.0f, 400.0f, 300.0f)),
        make_window(2, make_rect(400.0f, 0.0f, 400.0f, 300.0f))};

    expect_true(reach_stage_open(stage, make_rect(0.0f, 0.0f, 1000.0f, 1000.0f), 1.0f, windows, 2) == REACH_OK,
                "opening with windows succeeds");
    expect_true(reach_stage_is_open(stage), "stage is open after a successful open");
    expect_true(reach_stage_thumbnail_count(stage) == 2, "stage tracks both windows");

    const reach_stage_state *state = reach_stage_state_ptr(stage);
    expect_near(state->progress, 0.0f, "stage starts at progress zero");
    expect_true(reach_stage_animation_active(stage), "opening animation is active");

    reach_stage_thumbnail_placement placement = {};
    expect_true(reach_stage_thumbnail_at(stage, 0, &placement) == REACH_OK,
                "placement is readable");
    expect_true(placement.window == 1, "placement reports the source window");
    expect_true(reach_stage_thumbnail_at(stage, 2, &placement) != REACH_OK,
                "out of range placement is rejected");

    const reach_feature_capsule_ops *ops = reach_stage_capsule_ops();
    reach_feature_tick_result tick = {};
    for (int step = 0; step < 40; ++step)
    {
        ops->tick(stage, 0.016, &tick);
    }
    expect_near(reach_stage_state_ptr(stage)->progress, 1.0f, "stage settles at progress one");
    expect_true(!reach_stage_animation_active(stage), "settled stage stops animating");

    expect_true(reach_stage_thumbnail_at(stage, 0, &placement) == REACH_OK,
                "settled placement is readable");

    reach_stage_begin_close(stage);
    expect_true(reach_stage_is_open(stage), "stage stays open while closing animates");
    expect_true(reach_stage_animation_active(stage), "closing animation is active");

    for (int step = 0; step < 40; ++step)
    {
        ops->tick(stage, 0.016, &tick);
    }
    expect_true(!reach_stage_is_open(stage), "stage closes once the animation finishes");
    expect_true(reach_stage_thumbnail_count(stage) == 0, "closed stage tracks no windows");

    reach_stage_destroy(stage);
}

static void test_force_close_keeps_configured_animation(void)
{
    reach_stage *stage = nullptr;
    if (reach_stage_create(&stage) != REACH_OK || stage == nullptr)
    {
        expect_true(0, "stage is created for animation config");
        return;
    }

    reach_stage_set_animation_seconds(stage, 0.5f);
    expect_near(reach_stage_state_ptr(stage)->animation_seconds, 0.5f,
                "animation duration is configurable");

    reach_stage_set_animation_seconds(stage, 0.0f);
    expect_near(reach_stage_state_ptr(stage)->animation_seconds, 0.5f,
                "a non positive duration is ignored");

    reach_stage_open_window window = make_window(1, make_rect(0.0f, 0.0f, 400.0f, 300.0f));
    (void)reach_stage_open(stage, make_rect(0.0f, 0.0f, 1000.0f, 1000.0f), 1.0f, &window, 1);
    reach_stage_force_close(stage);

    expect_true(!reach_stage_is_open(stage), "force close closes immediately");
    expect_near(reach_stage_state_ptr(stage)->animation_seconds, 0.5f,
                "force close keeps the configured duration");

    reach_stage_destroy(stage);
}

static void test_closing_stage_finishes_without_external_wake_ups(void)
{
    reach_stage *stage = nullptr;
    if (reach_stage_create(&stage) != REACH_OK || stage == nullptr)
    {
        expect_true(0, "stage is created for the closing lifecycle");
        return;
    }

    reach_stage_open_window window = make_window(1, make_rect(0.0f, 0.0f, 400.0f, 300.0f));
    (void)reach_stage_open(stage, make_rect(0.0f, 0.0f, 1000.0f, 1000.0f), 1.0f, &window, 1);

    const reach_feature_capsule_ops *ops = reach_stage_capsule_ops();
    reach_feature_tick_result tick = {};
    for (int step = 0; step < 40; ++step)
    {
        ops->tick(stage, 0.016, &tick);
    }
    expect_true(ops->is_open(stage), "a settled open stage reports open");

    reach_stage_begin_close(stage);
    expect_true(reach_stage_is_open(stage), "a closing stage stays visible");
    expect_true(!ops->is_open(stage), "a closing stage stops holding transient surfaces open");

    int32_t always_requested_frames = 1;
    int guard = 0;
    while (reach_stage_is_open(stage) && guard < 200)
    {
        always_requested_frames = always_requested_frames && ops->needs_frame(stage);
        ops->tick(stage, 0.016, &tick);
        ++guard;
    }

    expect_true(always_requested_frames,
                "a closing stage never stops requesting frames before it closes");
    expect_true(!reach_stage_is_open(stage), "the close completes without external wake ups");
    expect_true(!ops->needs_frame(stage), "a closed stage stops requesting frames");

    reach_stage_destroy(stage);
}

static void test_close_before_the_first_tick_completes(void)
{
    reach_stage *stage = nullptr;
    if (reach_stage_create(&stage) != REACH_OK || stage == nullptr)
    {
        expect_true(0, "stage is created for the immediate close");
        return;
    }

    reach_stage_open_window window = make_window(1, make_rect(0.0f, 0.0f, 400.0f, 300.0f));
    (void)reach_stage_open(stage, make_rect(0.0f, 0.0f, 1000.0f, 1000.0f), 1.0f, &window, 1);
    reach_stage_begin_close(stage);

    const reach_feature_capsule_ops *ops = reach_stage_capsule_ops();
    expect_true(!reach_stage_animation_active(stage),
                "closing at progress zero leaves no animation to run");
    expect_true(ops->needs_frame(stage),
                "a stage closed before its first tick still requests a frame");

    reach_feature_tick_result tick = {};
    int guard = 0;
    while (reach_stage_is_open(stage) && guard < 200)
    {
        ops->tick(stage, 0.016, &tick);
        ++guard;
    }
    expect_true(!reach_stage_is_open(stage), "an immediate close still completes");

    reach_stage_destroy(stage);
}

static void test_closing_lands_on_the_current_window_frame(void)
{
    reach_stage *stage = nullptr;
    if (reach_stage_create(&stage) != REACH_OK || stage == nullptr)
    {
        expect_true(0, "stage is created for the displaced close");
        return;
    }

    reach_rect_f32 opened = make_rect(0.0f, 0.0f, 1000.0f, 800.0f);
    reach_stage_open_window window = make_window(1, opened);
    (void)reach_stage_open(stage, make_rect(0.0f, 0.0f, 1000.0f, 1000.0f), 1.0f, &window, 1);

    const reach_feature_capsule_ops *ops = reach_stage_capsule_ops();
    reach_feature_tick_result tick = {};
    for (int step = 0; step < 40; ++step)
    {
        ops->tick(stage, 0.016, &tick);
    }

    reach_rect_f32 grid_rect = reach_stage_state_ptr(stage)->tiles[0].target_rect;

    reach_rect_f32 displaced = make_rect(0.0f, 40.0f, 1000.0f, 760.0f);
    reach_stage_open_window pushed = make_window(1, displaced);
    reach_stage_refresh_tile_frames(stage, &pushed, 1);

    const reach_stage_tile *tile = &reach_stage_state_ptr(stage)->tiles[0];
    expect_near(tile->source_rect.y, displaced.y, "a refresh re-seats the tile landing rect");
    expect_near(tile->target_rect.y, grid_rect.y,
                "a refresh leaves the tile where it sits in the grid");
    expect_near(tile->target_rect.height, grid_rect.height,
                "a refresh does not resize the tile in the grid");

    reach_stage_begin_close(stage);
    reach_rect_f32 last = tile->current_rect;
    int guard = 0;
    while (reach_stage_is_open(stage) && guard < 200)
    {
        ops->tick(stage, 0.016, &tick);
        if (reach_stage_is_open(stage))
        {
            last = tile->current_rect;
        }
        ++guard;
    }

    expect_true(fabsf(last.y - displaced.y) < fabsf(last.y - opened.y),
                "the close animation settles onto the moved window, not where it opened");
    expect_true(fabsf(last.height - displaced.height) < fabsf(last.height - opened.height),
                "the close animation settles at the moved window size");

    reach_stage_destroy(stage);
}

int main(void)
{
    test_open_and_close_state_machine();
    test_force_close_keeps_configured_animation();
    test_closing_stage_finishes_without_external_wake_ups();
    test_close_before_the_first_tick_completes();
    test_closing_lands_on_the_current_window_frame();
    return failures == 0 ? 0 : 1;
}
