#ifndef REACH_PORTS_APPLICATION_RESOLVER_H
#define REACH_PORTS_APPLICATION_RESOLVER_H

#include <stdint.h>

#include "reach/core/application.h"
#include "reach/core/process_id.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_application_resolver reach_application_resolver;

    typedef struct reach_application_observation
    {
        reach_process_id process_id;
        uint16_t runtime_path[REACH_APPLICATION_TEXT_CAPACITY];
        uint16_t app_user_model_id[REACH_APPLICATION_TEXT_CAPACITY];
        uint16_t icon_ref[REACH_APPLICATION_TEXT_CAPACITY];
    } reach_application_observation;

    typedef struct reach_application_resolver_ops
    {
        reach_result (*resolve)(reach_application_resolver *resolver,
                                const reach_application_observation *observation,
                                reach_application *out_application);
        reach_result (*enrich)(reach_application_resolver *resolver,
                               reach_application *application);
        void (*destroy)(reach_application_resolver *resolver);
    } reach_application_resolver_ops;

    typedef struct reach_application_resolver_port
    {
        reach_application_resolver *resolver;
        reach_application_resolver_ops ops;
    } reach_application_resolver_port;

#ifdef __cplusplus
}
#endif

#endif
