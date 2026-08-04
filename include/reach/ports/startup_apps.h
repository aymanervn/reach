#ifndef REACH_PORTS_STARTUP_APPS_H
#define REACH_PORTS_STARTUP_APPS_H

#include <stdint.h>

#include "reach/core/startup_apps.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_startup_apps reach_startup_apps;

    typedef struct reach_startup_apps_ops
    {
        reach_result (*enumerate)(reach_startup_apps *apps, reach_startup_app_list *out_list);
        reach_result (*set_enabled)(reach_startup_apps *apps,
                                    const reach_startup_app_entry *entry, int32_t enabled);
        void (*destroy)(reach_startup_apps *apps);
    } reach_startup_apps_ops;

    typedef struct reach_startup_apps_port
    {
        reach_startup_apps *apps;
        reach_startup_apps_ops ops;
    } reach_startup_apps_port;

#ifdef __cplusplus
}
#endif

#endif
