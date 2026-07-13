#ifndef REACH_PORTS_SETTINGS_LAUNCHER_H
#define REACH_PORTS_SETTINGS_LAUNCHER_H

#include <stddef.h>
#include <stdint.h>

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_settings_launcher reach_settings_launcher;

    typedef struct reach_settings_launcher_ops
    {
        /* Resolve the settings app invocation (executable path + arguments);
           the actual launch runs through the app-launch service so a slow
           start never blocks the shell. */
        reach_result (*resolve)(reach_settings_launcher *launcher, uint16_t *out_path,
                                size_t path_capacity, uint16_t *out_arguments,
                                size_t arguments_capacity);
        void (*destroy)(reach_settings_launcher *launcher);
    } reach_settings_launcher_ops;

    typedef struct reach_settings_launcher_port
    {
        reach_settings_launcher *launcher;
        reach_settings_launcher_ops ops;
    } reach_settings_launcher_port;

#ifdef __cplusplus
}
#endif

#endif
