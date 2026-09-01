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

static reach_rect_f32 rect_at(reach_loader_phase phase, float progress)
{
    reach_loader_model model = {};
    reach_loader_model_init(&model, 1.0f);
    model.phase = phase;
    model.phase_progress = progress;
    return reach_loader_bar_rect(&model, make_container());
}

static void test_phase_endpoints_are_continuous(void)
{
    reach_rect_f32 grow_end = rect_at(REACH_LOADER_PHASE_GROW, 1.0f);
    reach_rect_f32 out_start = rect_at(REACH_LOADER_PHASE_SLIDE_OUT, 0.0f);
    expect_near(grow_end.x, out_start.x, "grow end origin matches slide out start");
    expect_near(grow_end.width, out_start.width, "grow end width matches slide out start");

    reach_rect_f32 out_end = rect_at(REACH_LOADER_PHASE_SLIDE_OUT, 1.0f);
    reach_rect_f32 back_start = rect_at(REACH_LOADER_PHASE_SLIDE_BACK, 0.0f);
    expect_near(out_end.x, back_start.x, "slide out end origin matches slide back start");
    expect_near(out_end.width, back_start.width, "slide out end width matches slide back start");

    reach_rect_f32 back_end = rect_at(REACH_LOADER_PHASE_SLIDE_BACK, 1.0f);
    reach_rect_f32 shrink_start = rect_at(REACH_LOADER_PHASE_SHRINK, 0.0f);
    expect_near(back_end.x, shrink_start.x, "slide back end origin matches shrink start");
    expect_near(back_end.width, shrink_start.width, "slide back end width matches shrink start");

    reach_rect_f32 shrink_end = rect_at(REACH_LOADER_PHASE_SHRINK, 1.0f);
    reach_rect_f32 grow_start = rect_at(REACH_LOADER_PHASE_GROW, 0.0f);
    expect_near(shrink_end.x, grow_start.x, "shrink end origin matches grow start");
    expect_near(shrink_end.width, grow_start.width, "shrink end width matches grow start");
}

static void test_bar_never_overflows_container(void)
{
    reach_rect_f32 container = make_container();
    reach_loader_phase phases[4] = {REACH_LOADER_PHASE_GROW, REACH_LOADER_PHASE_SLIDE_OUT,
                                    REACH_LOADER_PHASE_SLIDE_BACK, REACH_LOADER_PHASE_SHRINK};

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

static void test_phase_cycle_order(void)
{
    reach_loader_model model = {};
    reach_loader_model_init(&model, 1.0f);
    expect_true(model.phase == REACH_LOADER_PHASE_GROW, "starts in grow");

    reach_loader_update(&model, 1.0);
    expect_true(model.phase == REACH_LOADER_PHASE_SLIDE_OUT, "grow advances to slide out");

    reach_loader_update(&model, 1.0);
    expect_true(model.phase == REACH_LOADER_PHASE_SLIDE_BACK, "slide out advances to slide back");

    reach_loader_update(&model, 1.0);
    expect_true(model.phase == REACH_LOADER_PHASE_SHRINK, "slide back advances to shrink");

    reach_loader_update(&model, 1.0);
    expect_true(model.phase == REACH_LOADER_PHASE_GROW, "shrink returns to grow");
}

static void test_update_reports_and_clamps(void)
{
    reach_loader_model model = {};
    reach_loader_model_init(&model, 1.0f);

    expect_true(reach_loader_update(&model, 0.0) == 0, "zero delta does not animate");
    expect_true(reach_loader_update(&model, 0.5) == 1, "positive delta animates");
    expect_near(model.phase_progress, 0.5f, "progress accumulates");

    reach_loader_update(&model, 10000.0);
    expect_true(model.phase_progress >= 0.0f && model.phase_progress < 1.0f,
                "long stall leaves progress normalised");

    expect_true(reach_loader_update(nullptr, 1.0) == 0, "null model is rejected");
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
    test_phase_cycle_order();
    test_update_reports_and_clamps();
    test_zero_width_container();

    if (failures != 0)
    {
        fprintf(stderr, "%d loader test failure(s)\n", failures);
        return 1;
    }
    printf("loader tests passed\n");
    return 0;
}
