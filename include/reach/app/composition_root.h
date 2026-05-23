#ifndef REACH_APP_COMPOSITION_ROOT_H
#define REACH_APP_COMPOSITION_ROOT_H

#include "reach/shell/shell.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_app {
    reach_shell *shell;
    reach_input_source_port input_source;
} reach_app;

reach_result reach_app_create(const reach_shell_desc *desc, reach_app **out_app);
reach_result reach_app_start(reach_app *app);
reach_result reach_app_stop(reach_app *app);
reach_result reach_app_update(reach_app *app, double delta_seconds);
int32_t reach_app_needs_frame(const reach_app *app);
void reach_app_destroy(reach_app *app);

#ifdef __cplusplus
}
#endif

#endif
