#ifndef REACH_SERVICES_APP_CONTROL_H
#define REACH_SERVICES_APP_CONTROL_H

#include <stdint.h>

#include "reach/ports/app_launcher.h"
#include "reach/ports/explorer_service.h"
#include "reach/ports/window_manager.h"
#include "reach/support/layout.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_app_control reach_app_control;

    typedef enum reach_window_control_action
    {
        REACH_WINDOW_CONTROL_ACTIVATE = 0,
        REACH_WINDOW_CONTROL_MINIMIZE = 1,
        REACH_WINDOW_CONTROL_CLOSE = 2
    } reach_window_control_action;

    reach_result reach_app_control_create(reach_app_launcher_port launcher,
                                          reach_explorer_service_port explorer,
                                          reach_window_manager_port window_manager,
                                          void (*notify)(void *user), void *notify_user,
                                          reach_app_control **out_service);
    void reach_app_control_destroy(reach_app_control *service);

    void reach_app_control_stop(reach_app_control *service);

    int32_t reach_app_control_launch_available(const reach_app_control *service);
    reach_result reach_app_control_schedule_launch(reach_app_control *service,
                                                   const reach_app_launch_request *request);

    int32_t reach_app_control_reveal_available(const reach_app_control *service);
    reach_result reach_app_control_schedule_reveal(reach_app_control *service,
                                                   const uint16_t *path);

    reach_result reach_app_control_schedule_window(reach_app_control *service,
                                                   reach_window_control_action action,
                                                   uintptr_t window_id);
    reach_result reach_app_control_schedule_minimize(reach_app_control *service,
                                                     const uintptr_t *window_ids,
                                                     size_t window_count);
    reach_result reach_app_control_schedule_snap(reach_app_control *service, uintptr_t window_id,
                                                 reach_split_mode mode);
    int32_t reach_app_control_take_window_completed(reach_app_control *service,
                                                    reach_result *out_result);

#ifdef __cplusplus
}
#endif

#endif
