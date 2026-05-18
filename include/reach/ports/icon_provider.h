#ifndef REACH_PORTS_ICON_PROVIDER_H
#define REACH_PORTS_ICON_PROVIDER_H

#include <stdint.h>

#include "reach/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_icon_provider reach_icon_provider;

typedef struct reach_icon_request {
    uint16_t path[260];
    int32_t size_px;
} reach_icon_request;

typedef struct reach_icon_handle {
    uint64_t id;
    int32_t wants_backplate;
    uint16_t debug_name[260];
} reach_icon_handle;

typedef struct reach_icon_provider_ops {
    reach_result (*load)(reach_icon_provider *provider, const reach_icon_request *request, reach_icon_handle *out_icon);
    reach_result (*release)(reach_icon_provider *provider, reach_icon_handle icon);
    void (*destroy)(reach_icon_provider *provider);
} reach_icon_provider_ops;

typedef struct reach_icon_provider_port {
    reach_icon_provider *provider;
    reach_icon_provider_ops ops;
} reach_icon_provider_port;

#ifdef __cplusplus
}
#endif

#endif
