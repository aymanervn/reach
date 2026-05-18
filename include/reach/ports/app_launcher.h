#ifndef REACH_PORTS_APP_LAUNCHER_H
#define REACH_PORTS_APP_LAUNCHER_H

#include <stdint.h>

#include "reach/core/ui_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_app_launcher reach_app_launcher;

typedef struct reach_app_launch_request {
    uint16_t path[260];
    uint16_t arguments[260];
    int32_t force_new_instance;
} reach_app_launch_request;

typedef struct reach_app_launcher_ops {
    reach_result (*launch)(reach_app_launcher *launcher, const reach_app_launch_request *request);
    void (*destroy)(reach_app_launcher *launcher);
} reach_app_launcher_ops;

typedef struct reach_app_launcher_port {
    reach_app_launcher *launcher;
    reach_app_launcher_ops ops;
} reach_app_launcher_port;

#ifdef __cplusplus
}
#endif

#endif
