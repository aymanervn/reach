#ifndef REACH_PORTS_INPUT_LANGUAGE_H
#define REACH_PORTS_INPUT_LANGUAGE_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/window_id.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_input_language reach_input_language;

    typedef struct reach_input_language_state
    {
        uint16_t code[8];
        uint32_t layout_id;
    } reach_input_language_state;

    typedef struct reach_input_language_ops
    {
        reach_result (*get_state)(reach_input_language *language, reach_window_id foreground_window,
                                  reach_input_language_state *out_state);
        reach_result (*cycle_next)(reach_input_language *language,
                                   reach_window_id foreground_window);
        void (*destroy)(reach_input_language *language);
    } reach_input_language_ops;

    typedef struct reach_input_language_port
    {
        reach_input_language *language;
        reach_input_language_ops ops;
    } reach_input_language_port;

#ifdef __cplusplus
}
#endif

#endif
