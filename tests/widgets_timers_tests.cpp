#include "../src/features/widgets/widgets_timers.h"
#include <cstdio>

static int failed = 0;
static void expect(bool value, const char *message)
{
    if (!value)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failed;
    }
}
int main()
{
    reach_pomodoro timer;
    expect(timer.remaining == 1500 && !timer.running, "focus defaults to 25 minutes");
    reach_pomodoro_toggle(timer, 100);
    expect(reach_pomodoro_remaining(timer, 160) == 1440,
           "elapsed time comes from the monotonic clock");
    reach_pomodoro_toggle(timer, 160);
    expect(reach_pomodoro_remaining(timer, 1000) == 1440, "pause preserves remaining time");
    reach_pomodoro_toggle(timer, 1000);
    expect(!reach_pomodoro_tick(timer, 2439), "focus remains active before its deadline");
    expect(reach_pomodoro_tick(timer, 2440) && timer.phase == 1 && timer.remaining == 300 &&
               timer.completed == 1 && !timer.running,
           "a complete focus session offers a five-minute break");
    reach_pomodoro_advance(timer, false);
    expect(timer.phase == 0 && timer.remaining == 1500, "skipping a break returns to focus");
    reach_pomodoro_advance(timer, false);
    expect(timer.completed == 1 && timer.phase == 1,
           "skipping focus does not count a completed session");
    timer = {};
    double now = 0;
    for (int session = 0; session < 4; ++session)
    {
        reach_pomodoro_toggle(timer, now);
        now += 1500;
        expect(reach_pomodoro_tick(timer, now), "focus completes at its deadline");
        if (session < 3)
            reach_pomodoro_advance(timer, false);
    }
    expect(timer.phase == 2 && timer.remaining == 900 && timer.completed == 4,
           "four focus sessions offer a fifteen-minute break");
    return failed ? 1 : 0;
}
