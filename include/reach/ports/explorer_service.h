#ifndef REACH_PORTS_EXPLORER_SERVICE_H
#define REACH_PORTS_EXPLORER_SERVICE_H

#include <stdint.h>

#include "reach/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_explorer_service reach_explorer_service;

typedef struct reach_explorer_service_ops {
    reach_result (*open_default)(reach_explorer_service *service);
    reach_result (*open_path)(reach_explorer_service *service, const uint16_t *path);
    reach_result (*open_shell_location)(reach_explorer_service *service, const uint16_t *shell_location);
    void (*destroy)(reach_explorer_service *service);
} reach_explorer_service_ops;

typedef struct reach_explorer_service_port {
    reach_explorer_service *service;
    reach_explorer_service_ops ops;
} reach_explorer_service_port;

#ifdef __cplusplus
}
#endif

#endif
