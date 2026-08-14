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
    expect_near(placement.destination.x, 0.0f, "placement starts at the window position");
    expect_near(placement.destination.width, 400.0f, "placement starts at the window size");
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

static void test_hit_testing_resolves_tiles(void)
{
    reach_stage *stage = nullptr;
    if (reach_stage_create(&stage) != REACH_OK || stage == nullptr)
    {
        expect_true(0, "stage is created for hit testing");
        return;
    }

    reach_stage_open_window windows[2] = {
        make_window(11, make_rect(0.0f, 0.0f, 400.0f, 300.0f)),
        make_window(22, make_rect(400.0f, 0.0f, 400.0f, 300.0f))};
    (void)reach_stage_open(stage, make_rect(0.0f, 0.0f, 1000.0f, 1000.0f), 1.0f, windows, 2);

    const reach_stage_state *state = reach_stage_state_ptr(stage);
    reach_rect_f32 first = state->tiles[0].current_rect;

    reach_point_f32 inside = {first.x + first.width * 0.5f, first.y + first.height * 0.5f};
    size_t index = 99;
    expect_true(reach_stage_tile_at_point(stage, inside, &index), "center of a tile hits it");
    expect_true(index == 0, "the first tile is resolved");

    reach_point_f32 outside = {-50.0f, -50.0f};
    expect_true(!reach_stage_tile_at_point(stage, outside, &index),
                "a point outside every tile misses");

    const reach_feature_capsule_ops *ops = reach_stage_capsule_ops();
    reach_pointer_event event = {};
    reach_capsule_pointer_result result = {};

    event.kind = REACH_POINTER_EVENT_UP;
    event.x = (int32_t)inside.x;
    event.y = (int32_t)inside.y;
    ops->handle_pointer(stage, &event, &result);
    expect_true(result.handled, "a press on a tile is handled");
    expect_true(result.action.kind == REACH_STAGE_ACTION_ACTIVATE_WINDOW,
                "a press on a tile activates the window");
    expect_true(result.action.window == 11, "the pressed tile reports its window");

    result = {};
    event.kind = REACH_POINTER_EVENT_DOWN;
    event.x = (int32_t)inside.x;
    event.y = (int32_t)inside.y;
    ops->handle_pointer(stage, &event, &result);
    expect_true(result.handled, "a press on a tile is claimed by the stage");

    result = {};
    event.kind = REACH_POINTER_EVENT_DOWN;
    event.x = -50;
    event.y = -50;
    ops->handle_pointer(stage, &event, &result);
    expect_true(!result.handled,
                "a press outside every tile is unhandled so the host dismisses the stage");
    expect_true(result.action.kind == REACH_STAGE_ACTION_NONE,
                "the stage does not invent its own dismissal action");

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

int main(void)
{
    test_open_and_close_state_machine();
    test_hit_testing_resolves_tiles();
    test_force_close_keeps_configured_animation();
    return failures == 0 ? 0 : 1;
}
