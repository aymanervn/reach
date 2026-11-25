#ifndef REACH_ADAPTERS_WINDOWS_HANG_GUARD_H
#define REACH_ADAPTERS_WINDOWS_HANG_GUARD_H

#ifdef __cplusplus
extern "C"
{
#endif

    // Watches the calling thread's message loop. When one pass between waits stops retrieving
    // posted messages for about 10 s, the guard writes a dump, logs the hang and ends the process
    // so Windows restarts the shell; after three such restarts in 5 minutes it only dumps and logs.
    // All four calls run on the watched UI thread; busy and idle mark the edges of its wait.
    void reach_hang_guard_start(void);
    void reach_hang_guard_busy(void);
    void reach_hang_guard_idle(void);
    void reach_hang_guard_stop(void);

#ifdef __cplusplus
}
#endif

#endif
