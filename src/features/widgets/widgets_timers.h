#ifndef REACH_WIDGETS_TIMERS_H
#define REACH_WIDGETS_TIMERS_H

#include <algorithm>

struct reach_pomodoro
{
    double remaining = 1500;
    double started = 0;
    bool running = false;
    int phase = 0;
    int completed = 0;
};

static inline double reach_pomodoro_remaining(const reach_pomodoro &timer, double now)
{
    return std::max(0.0, timer.remaining - (timer.running ? now - timer.started : 0.0));
}

static inline void reach_pomodoro_toggle(reach_pomodoro &timer, double now)
{
    if (timer.running)
        timer.remaining = reach_pomodoro_remaining(timer, now);
    timer.started = now;
    timer.running = !timer.running;
}

static inline void reach_pomodoro_advance(reach_pomodoro &timer, bool finished)
{
    if (timer.phase == 0)
    {
        if (finished)
            ++timer.completed;
        timer.phase = finished && timer.completed % 4 == 0 ? 2 : 1;
        timer.remaining = timer.phase == 2 ? 900 : 300;
    }
    else
    {
        timer.phase = 0;
        timer.remaining = 1500;
    }
    timer.running = false;
}

static inline bool reach_pomodoro_tick(reach_pomodoro &timer, double now)
{
    if (!timer.running || reach_pomodoro_remaining(timer, now) > 0)
        return false;
    reach_pomodoro_advance(timer, true);
    return true;
}

#endif
