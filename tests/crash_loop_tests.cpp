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

static void test_restart_delays_then_fallback_on_the_third_exit(void)
{
    reach_crash_loop loop = {};
    reach_crash_loop_decision first = reach_crash_loop_record_exit(&loop, 1000);
    expect(first.action == REACH_CRASH_LOOP_RESTART && first.delay_ms == 0 &&
               first.window_count == 1,
           "the first unexpected exit restarts immediately");

    reach_crash_loop_decision second = reach_crash_loop_record_exit(&loop, 2000);
    expect(second.action == REACH_CRASH_LOOP_RESTART && second.delay_ms == 1000 &&
               second.window_count == 2,
           "the second unexpected exit restarts after one second");

    reach_crash_loop_decision third = reach_crash_loop_record_exit(&loop, 3000);
    expect(third.action == REACH_CRASH_LOOP_FALLBACK && third.window_count == 3,
           "the third unexpected exit inside the window falls back");
}

static void test_exits_outside_the_window_drop_out(void)
{
    reach_crash_loop loop = {};
    reach_crash_loop_record_exit(&loop, 0);
    reach_crash_loop_record_exit(&loop, 10000);
    reach_crash_loop_decision late = reach_crash_loop_record_exit(&loop, 190000);
    expect(late.action == REACH_CRASH_LOOP_RESTART && late.window_count == 2 &&
               late.delay_ms == 1000,
           "the first exit expired and the other two still count");

    reach_crash_loop_decision spaced = reach_crash_loop_record_exit(&loop, 400000);
    expect(spaced.action == REACH_CRASH_LOOP_RESTART && spaced.window_count == 1 &&
               spaced.delay_ms == 0,
           "isolated exits never accumulate into a fallback");
}

static void test_window_boundary_is_inclusive(void)
{
    reach_crash_loop loop = {};
    reach_crash_loop_record_exit(&loop, 0);
    reach_crash_loop_record_exit(&loop, 90000);
    reach_crash_loop_decision edge = reach_crash_loop_record_exit(&loop, 180000);
    expect(edge.action == REACH_CRASH_LOOP_FALLBACK && edge.window_count == 3,
           "an exit exactly 180 seconds old still counts");

    reach_crash_loop other = {};
    reach_crash_loop_record_exit(&other, 0);
    reach_crash_loop_record_exit(&other, 90000);
    reach_crash_loop_decision past = reach_crash_loop_record_exit(&other, 180001);
    expect(past.action == REACH_CRASH_LOOP_RESTART && past.window_count == 2,
           "an exit older than 180 seconds drops out");
}

static void test_fast_startup_failures_share_the_window(void)
{
    reach_crash_loop loop = {};
    reach_crash_loop_record_exit(&loop, 100);
    reach_crash_loop_record_exit(&loop, 400);
    reach_crash_loop_decision decision = reach_crash_loop_record_exit(&loop, 120000);
    expect(decision.action == REACH_CRASH_LOOP_FALLBACK,
           "startup and runtime exits use one window");
}

static void test_null_loop_is_refused(void)
{
    expect(reach_crash_loop_record_exit(nullptr, 0).action == REACH_CRASH_LOOP_FALLBACK,
           "a missing policy state never loops forever");
}

int main(void)
{
    test_restart_delays_then_fallback_on_the_third_exit();
    test_exits_outside_the_window_drop_out();
    test_window_boundary_is_inclusive();
    test_fast_startup_failures_share_the_window();
    test_null_loop_is_refused();
    return failed == 0 ? 0 : 1;
}
