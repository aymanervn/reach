#ifndef REACH_PORTS_APP_LAUNCHER_H
#define REACH_PORTS_APP_LAUNCHER_H

#include <stdint.h>

#include "reach/core/application.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_app_launcher reach_app_launcher;

    typedef struct reach_app_launch_request
    {
        uint16_t path[260];
        uint16_t arguments[260];
        uint16_t app_user_model_id[260];
        reach_application_launch_kind launch_kind;
        int32_t force_new_instance;
        int32_t run_as_admin;
    } reach_app_launch_request;

    typedef enum reach_app_launch_failure
    {
        REACH_APP_LAUNCH_FAILURE_NONE = 0,
        REACH_APP_LAUNCH_FAILURE_NOT_FOUND = 1,
        REACH_APP_LAUNCH_FAILURE_CANCELLED = 2,
        REACH_APP_LAUNCH_FAILURE_ACCESS_DENIED = 3,
        REACH_APP_LAUNCH_FAILURE_UNKNOWN = 4
    } reach_app_launch_failure;

    typedef enum reach_system_tool
    {
        REACH_SYSTEM_TOOL_SYSTEM_INFORMATION = 1,
        REACH_SYSTEM_TOOL_TASK_MANAGER = 2,
        REACH_SYSTEM_TOOL_DEVICE_MANAGER = 3,
        REACH_SYSTEM_TOOL_CONTROL_PANEL = 4,
        REACH_SYSTEM_TOOL_ENVIRONMENT_VARIABLES = 5,
        REACH_SYSTEM_TOOL_SERVICES = 6
    } reach_system_tool;

    typedef struct reach_app_launcher_ops
    {

        reach_result (*launch)(reach_app_launcher *launcher,
                               const reach_app_launch_request *request,
                               reach_app_launch_failure *out_failure);
        reach_result (*resolve_system_tool)(reach_app_launcher *launcher, reach_system_tool tool,
                                            reach_app_launch_request *out_request);
        void (*destroy)(reach_app_launcher *launcher);
    } reach_app_launcher_ops;

    typedef struct reach_app_launcher_port
    {
        reach_app_launcher *launcher;
        reach_app_launcher_ops ops;
    } reach_app_launcher_port;

#ifdef __cplusplus
}
#endif

#endif
