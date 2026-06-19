#ifndef REACH_APP_SETTINGS_APP_H
#define REACH_APP_SETTINGS_APP_H

#include <stdint.h>

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_settings_app reach_settings_app;

    reach_result reach_settings_app_create(reach_settings_app **out_app);
    reach_result reach_settings_app_start(reach_settings_app *app);
    reach_result reach_settings_app_update(reach_settings_app *app, double delta_seconds);
    reach_result reach_settings_app_dispatch_events(reach_settings_app *app);
    int32_t reach_settings_app_has_pending_events(const reach_settings_app *app);
    int32_t reach_settings_app_needs_frame(const reach_settings_app *app);
    int32_t reach_settings_app_running(const reach_settings_app *app);
    void reach_settings_app_activate(reach_settings_app *app);
    void reach_settings_app_destroy(reach_settings_app *app);

#ifdef __cplusplus
}
#endif

#endif
