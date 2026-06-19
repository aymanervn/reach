#ifndef REACH_PORTS_SETTINGS_LAUNCHER_H
#define REACH_PORTS_SETTINGS_LAUNCHER_H

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_settings_launcher reach_settings_launcher;

    typedef struct reach_settings_launcher_ops
    {
        reach_result (*open)(reach_settings_launcher *launcher);
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
