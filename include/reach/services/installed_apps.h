#ifndef REACH_SERVICES_INSTALLED_APPS_H
#define REACH_SERVICES_INSTALLED_APPS_H

#include "reach/ports/app_launcher.h"
#include "reach/ports/installed_apps.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_installed_apps_service reach_installed_apps_service;

    typedef enum reach_installed_apps_command
    {
        REACH_INSTALLED_APPS_COMMAND_NONE = 0,
        REACH_INSTALLED_APPS_COMMAND_REFRESH,
        REACH_INSTALLED_APPS_COMMAND_OPEN,
        REACH_INSTALLED_APPS_COMMAND_UNINSTALL,
        REACH_INSTALLED_APPS_COMMAND_MANAGE
    } reach_installed_apps_command;

    typedef struct reach_installed_apps_snapshot
    {
        reach_installed_app_list apps;
        reach_installed_apps_command completed_command;
        size_t completed_index;
        int32_t command_succeeded;
    } reach_installed_apps_snapshot;

    reach_result reach_installed_apps_service_create(
        reach_installed_apps_port port, reach_app_launcher_port launcher,
        void (*notify)(void *user), void *notify_user,
        reach_installed_apps_service **out_service);
    void reach_installed_apps_service_destroy(reach_installed_apps_service *service);
    void reach_installed_apps_service_refresh(reach_installed_apps_service *service);
    void reach_installed_apps_service_open(reach_installed_apps_service *service, size_t index);
    void reach_installed_apps_service_uninstall(reach_installed_apps_service *service,
                                                 size_t index);
    void reach_installed_apps_service_manage(reach_installed_apps_service *service, size_t index);
    int32_t reach_installed_apps_service_take(reach_installed_apps_service *service,
                                               reach_installed_apps_snapshot *out_snapshot);
    int32_t reach_installed_apps_service_pending(const reach_installed_apps_service *service);

#ifdef __cplusplus
}
#endif

#endif
