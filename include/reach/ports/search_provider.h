#ifndef REACH_PORTS_SEARCH_PROVIDER_H
#define REACH_PORTS_SEARCH_PROVIDER_H

#include <stddef.h>
#include <stdint.h>

#include "reach/support/search_types.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_search_provider reach_search_provider;

    typedef struct reach_search_result
    {
        uint32_t id;
        uint16_t title[REACH_SEARCH_RESULT_NAME_CAPACITY];
        uint16_t subtitle[REACH_SEARCH_RESULT_PATH_CAPACITY];
        uint16_t path[REACH_SEARCH_RESULT_PATH_CAPACITY];
        reach_search_result_kind kind;
        int32_t is_directory;
        int32_t score;
    } reach_search_result;

    typedef struct reach_search_provider_ops
    {
        reach_result (*query)(reach_search_provider *provider, const uint16_t *query);
        size_t (*result_count)(const reach_search_provider *provider);
        reach_result (*result_at)(const reach_search_provider *provider, size_t index,
                                  reach_search_result *out_result);
        void (*destroy)(reach_search_provider *provider);
    } reach_search_provider_ops;

    typedef struct reach_search_provider_port
    {
        reach_search_provider *provider;
        reach_search_provider_ops ops;
    } reach_search_provider_port;

#ifdef __cplusplus
}
#endif

#endif
