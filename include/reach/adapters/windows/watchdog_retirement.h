#ifndef REACH_ADAPTERS_WINDOWS_WATCHDOG_RETIREMENT_H
#define REACH_ADAPTERS_WINDOWS_WATCHDOG_RETIREMENT_H

#include <stdint.h>

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Installs from before Windows became Reach's only supervisor carry a ReachWatchdog logon
    // task that starts Reach inside Task Scheduler's job, where every launched app dies with the
    // task. The owner chose to keep this cleanup in every release rather than drop it later.

    int32_t reach_windows_watchdog_task_present(void);

    // Deletes the task only; needs elevation. A running watchdog keeps running until sign-out.
    reach_result reach_windows_delete_watchdog_task(void);

    // Called once per new Reach version; works off the UI thread. Renames reachWatchdog.exe aside
    // so the task has nothing to start, ends the watchdog unless this Reach lives in its job, and
    // explains the coming prompt, then asks once through reachElevate to delete the task. A
    // declined prompt is not repeated.
    void reach_windows_start_install_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif
