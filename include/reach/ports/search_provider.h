#ifndef REACH_PORTS_SEARCH_PROVIDER_H
#define REACH_PORTS_SEARCH_PROVIDER_H

#include <stddef.h>
#include <stdint.h>

#include "reach/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_search_provider reach_search_provider;

typedef struct reach_search_result {
    uint32_t id;
    uint16_t title[128];
    uint16_t subtitle[260];
} reach_search_result;

typedef struct reach_search_provider_ops {
    reach_result (*query)(reach_search_provider *provider, const uint16_t *query);
    size_t (*result_count)(const reach_search_provider *provider);
    reach_result (*result_at)(const reach_search_provider *provider, size_t index, reach_search_result *out_result);
    void (*destroy)(reach_search_provider *provider);
} reach_search_provider_ops;

typedef struct reach_search_provider_port {
    reach_search_provider *provider;
    reach_search_provider_ops ops;
} reach_search_provider_port;

#ifdef __cplusplus
}
#endif

#endif
