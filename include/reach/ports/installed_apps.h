#ifndef REACH_PORTS_INSTALLED_APPS_H
#define REACH_PORTS_INSTALLED_APPS_H

#include "reach/core/installed_apps.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_installed_apps reach_installed_apps;

    typedef struct reach_installed_apps_ops
    {
        reach_result (*enumerate)(reach_installed_apps *apps, reach_installed_app_list *out_list);
        reach_result (*uninstall)(reach_installed_apps *apps, const reach_installed_app *entry);
        reach_result (*manage)(reach_installed_apps *apps, const reach_installed_app *entry);
        void (*thread_attach)(reach_installed_apps *apps);
        void (*thread_detach)(reach_installed_apps *apps);
        void (*destroy)(reach_installed_apps *apps);
    } reach_installed_apps_ops;

    typedef struct reach_installed_apps_port
    {
        reach_installed_apps *apps;
        reach_installed_apps_ops ops;
    } reach_installed_apps_port;

#ifdef __cplusplus
}
#endif

#endif
