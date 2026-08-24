#ifndef REACH_PORTS_CONFIG_STORE_H
#define REACH_PORTS_CONFIG_STORE_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/config.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_config_store reach_config_store;

    typedef struct reach_config_store_ops
    {
        reach_result (*begin_transaction)(reach_config_store *store);
        void (*end_transaction)(reach_config_store *store);
        reach_result (*load)(reach_config_store *store, reach_config_snapshot *out_snapshot);
        reach_result (*save)(reach_config_store *store, const reach_config_snapshot *snapshot);
        void (*destroy)(reach_config_store *store);
    } reach_config_store_ops;

    typedef struct reach_config_store_port
    {
        reach_config_store *store;
        reach_config_store_ops ops;
    } reach_config_store_port;

#ifdef __cplusplus
}
#endif

#endif
