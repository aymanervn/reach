#include "reach/core/loader.h"

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
    float delta = actual - expected;
    if (delta < 0.0f)
    {
        delta = -delta;
    }
    if (delta > 0.01f)
    {
        ++failures;
        fprintf(stderr, "FAILED: %s (expected %.4f, got %.4f)\n", message, expected, actual);
    }
}

static reach_rect_f32 make_container(void)
{
    reach_rect_f32 container = {10.0f, 5.0f, 100.0f, 4.0f};
    return container;
}

static reach_rect_f32 rect_at(int phase, float progress)
{
    reach_loader_model model = {};
    reach_loader_model_init_at(&model, 1.0f, 100);
    return reach_loader_bar_rect_at(&model, make_container(), 100 + phase + progress);
}

static void test_phase_endpoints_are_continuous(void)
{
    reach_rect_f32 grow_end = rect_at(0, 1.0f);
    reach_rect_f32 out_start = rect_at(1, 0.0f);
    expect_near(grow_end.x, out_start.x, "grow end origin matches slide out start");
    expect_near(grow_end.width, out_start.width, "grow end width matches slide out start");

    reach_rect_f32 out_end = rect_at(1, 1.0f);
    reach_rect_f32 back_start = rect_at(2, 0.0f);
    expect_near(out_end.x, back_start.x, "slide out end origin matches slide back start");
    expect_near(out_end.width, back_start.width, "slide out end width matches slide back start");

    reach_rect_f32 back_end = rect_at(2, 1.0f);
    reach_rect_f32 shrink_start = rect_at(3, 0.0f);
    expect_near(back_end.x, shrink_start.x, "slide back end origin matches shrink start");
    expect_near(back_end.width, shrink_start.width, "slide back end width matches shrink start");

    reach_rect_f32 shrink_end = rect_at(3, 1.0f);
    reach_rect_f32 grow_start = rect_at(0, 0.0f);
    expect_near(shrink_end.x, grow_start.x, "shrink end origin matches grow start");
    expect_near(shrink_end.width, grow_start.width, "shrink end width matches grow start");
}

static void test_bar_never_overflows_container(void)
{
    reach_rect_f32 container = make_container();
    int phases[4] = {0, 1, 2, 3};

    for (int phase_index = 0; phase_index < 4; ++phase_index)
    {
        for (int step = 0; step <= 20; ++step)
        {
            float progress = (float)step / 20.0f;
            reach_rect_f32 bar = rect_at(phases[phase_index], progress);
            expect_true(bar.x >= container.x - 0.01f, "bar origin stays inside container");
            expect_true(bar.x + bar.width <= container.x + container.width + 0.01f,
                        "bar right edge stays inside container");
            expect_true(bar.width >= container.width * REACH_LOADER_MINIMUM_WIDTH_RATIO - 0.01f,
                        "bar keeps minimum width");
        }
    }
}

static void test_absolute_cycle_and_reset(void)
{
    reach_loader_model model = {};
    reach_loader_model_init_at(&model, 1, 100);
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        reach_rect_f32 bar = reach_loader_bar_rect_at(&model, make_container(), 100 + cycle * 4);
        expect_near(bar.x, 10, "each cycle starts at the left edge");
        expect_near(bar.width, 10, "each cycle starts at minimum width");
    }
    reach_rect_f32 delayed = reach_loader_bar_rect_at(&model, make_container(), 10102.5);
    reach_rect_f32 reference = reach_loader_bar_rect_at(&model, make_container(), 102.5);
    expect_near(delayed.x, reference.x, "delayed sampling skips whole cycles without drift");
    expect_near(delayed.width, reference.width, "delayed width follows the absolute epoch");
    uint64_t generation = model.width.generation;
    reach_loader_model_reset_at(&model, 200);
    expect_true(model.width.generation != generation, "reset replaces periodic ownership");
    reach_rect_f32 reset = reach_loader_bar_rect_at(&model, make_container(), 200);
    expect_near(reset.x, 10, "reset restores initial origin immediately");
    expect_near(reset.width, 10, "reset restores minimum width immediately");
}

static void test_zero_width_container(void)
{
    reach_loader_model model = {};
    reach_loader_model_init(&model, 1.0f);
    reach_rect_f32 empty = {0.0f, 0.0f, 0.0f, 4.0f};
    reach_rect_f32 bar = reach_loader_bar_rect(&model, empty);
    expect_near(bar.width, 0.0f, "zero width container yields zero width bar");
}

int main(void)
{
    test_phase_endpoints_are_continuous();
    test_bar_never_overflows_container();
    test_absolute_cycle_and_reset();
    test_zero_width_container();

    if (failures != 0)
    {
        fprintf(stderr, "%d loader test failure(s)\n", failures);
        return 1;
    }
    printf("loader tests passed\n");
    return 0;
}
