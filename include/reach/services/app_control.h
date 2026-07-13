#ifndef REACH_SERVICES_APP_CONTROL_H
#define REACH_SERVICES_APP_CONTROL_H

#include <stdint.h>

#include "reach/ports/app_launcher.h"
#include "reach/ports/window_manager.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * App-control service — the single action facade for app lifecycle:
     * launching (over the app-launcher port) and window control
     * (activate/minimize/close over the window-manager port, elevating
     * through the privileged-control path when needed). Observation stays in
     * window_tracking (a pure read model).
     *
     * Two execution lanes with deliberately different shutdown semantics:
     * - Launches: a FIFO queue feeds bounded DETACHED workers — a launch
     *   blocking inside the port (an app hanging while starting) never delays
     *   or drops another, and stop() abandons in-flight launches (a hung OS
     *   launch cannot be joined; the app-launcher port contract requires
     *   launch() to be instance-state-free).
     * - Window commands: a JOINED worker — the window-manager port is
     *   stateful, so this lane must never outlive the adapter; the commands
     *   are bounded (the privileged work happens in reachService).
     *
     * `notify` fires on the window lane after a command batch completes;
     * composition drains take_window_completed on the main thread. Ports are
     * borrowed; composition stops this service before destroying them.
     */
    typedef struct reach_app_control reach_app_control;

    typedef enum reach_window_control_action
    {
        REACH_WINDOW_CONTROL_ACTIVATE = 0,
        REACH_WINDOW_CONTROL_MINIMIZE = 1,
        REACH_WINDOW_CONTROL_CLOSE = 2
    } reach_window_control_action;

    reach_result reach_app_control_create(reach_app_launcher_port launcher,
                                          reach_window_manager_port window_manager,
                                          void (*notify)(void *user), void *notify_user,
                                          reach_app_control **out_service);
    void reach_app_control_destroy(reach_app_control *service);
    /* Join the window lane and abandon in-flight launches; idempotent. */
    void reach_app_control_stop(reach_app_control *service);

    int32_t reach_app_control_launch_available(const reach_app_control *service);
    reach_result reach_app_control_schedule_launch(reach_app_control *service,
                                                   const reach_app_launch_request *request);

    reach_result reach_app_control_schedule_window(reach_app_control *service,
                                                   reach_window_control_action action,
                                                   uintptr_t window_id);
    reach_result reach_app_control_schedule_minimize(reach_app_control *service,
                                                     const uintptr_t *window_ids,
                                                     size_t window_count);
    /* Returns 1 when a window-command batch completed since the last call;
       *out_result is its aggregate result. */
    int32_t reach_app_control_take_window_completed(reach_app_control *service,
                                                    reach_result *out_result);

#ifdef __cplusplus
}
#endif

#endif
