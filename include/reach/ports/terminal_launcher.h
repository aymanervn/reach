#ifndef REACH_PORTS_TERMINAL_LAUNCHER_H
#define REACH_PORTS_TERMINAL_LAUNCHER_H

#include <stddef.h>
#include <stdint.h>

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_TERMINAL_COMMAND_CAPACITY 256

    typedef struct reach_terminal_launcher reach_terminal_launcher;

    typedef struct reach_terminal_launch_request
    {
        uint16_t command[REACH_TERMINAL_COMMAND_CAPACITY];
    } reach_terminal_launch_request;

    typedef struct reach_terminal_launcher_ops
    {
        reach_result (*launch)(reach_terminal_launcher *launcher,
                               const reach_terminal_launch_request *request);
        reach_result (*icon_ref)(reach_terminal_launcher *launcher, uint16_t *out_ref,
                                 size_t ref_capacity);
        void (*destroy)(reach_terminal_launcher *launcher);
    } reach_terminal_launcher_ops;

    typedef struct reach_terminal_launcher_port
    {
        reach_terminal_launcher *launcher;
        reach_terminal_launcher_ops ops;
    } reach_terminal_launcher_port;

#ifdef __cplusplus
}
#endif

#endif
