#include "reach/support/crash_loop.h"

#include <stdio.h>

static int failed = 0;

static void expect(int condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        failed += 1;
    }
}

static void test_third_start_inside_the_window_is_a_loop(void)
{
    reach_crash_loop loop = {};
    expect(!reach_crash_loop_record_start(&loop, 1000), "the first start is never a loop");
    expect(!reach_crash_loop_record_start(&loop, 2000), "the second start is never a loop");
    expect(reach_crash_loop_record_start(&loop, 3000), "three starts inside the window loop");
}

static void test_a_long_run_breaks_the_loop(void)
{
    reach_crash_loop loop = {};
    reach_crash_loop_record_start(&loop, 1000);
    reach_crash_loop_record_start(&loop, 2000);
    expect(!reach_crash_loop_record_start(&loop, 70000), "a start after a long run is not a loop");
    expect(!reach_crash_loop_record_start(&loop, 71000),
           "the oldest kept start is still outside the window");
}

static void test_window_boundary_is_inclusive(void)
{
    reach_crash_loop loop = {};
    reach_crash_loop_record_start(&loop, 1000);
    reach_crash_loop_record_start(&loop, 30000);
    expect(reach_crash_loop_record_start(&loop, 61000), "starts exactly 60 s apart still loop");

    reach_crash_loop other = {};
    reach_crash_loop_record_start(&other, 1000);
    reach_crash_loop_record_start(&other, 30000);
    expect(!reach_crash_loop_record_start(&other, 61001), "starts past 60 s do not loop");
}

static void test_clock_moving_back_is_not_a_loop(void)
{
    reach_crash_loop loop = {};
    reach_crash_loop_record_start(&loop, 500000);
    reach_crash_loop_record_start(&loop, 500100);
    expect(!reach_crash_loop_record_start(&loop, 1000), "a clock set back never trips");
}

static void test_null_loop_is_not_a_loop(void)
{
    expect(!reach_crash_loop_record_start(nullptr, 0), "missing history never falls back");
}

int main(void)
{
    test_third_start_inside_the_window_is_a_loop();
    test_a_long_run_breaks_the_loop();
    test_window_boundary_is_inclusive();
    test_clock_moving_back_is_not_a_loop();
    test_null_loop_is_not_a_loop();
    return failed == 0 ? 0 : 1;
}
